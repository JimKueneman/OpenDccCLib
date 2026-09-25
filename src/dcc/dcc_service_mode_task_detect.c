/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file dcc_service_mode_task_detect.c
 * @brief Task orchestrator for service mode decoder detection (S-9.2.3 Appendix A/B).
 *
 * @details Determines ALL supported service modes, returned as a
 * DCC_SERVICE_MODE_SUPPORTED_* bitmask:
 *   1. Direct  -- spec method (§E lines 95-99): two bit-verifies of CV#8 bit 7,
 *      one with value 0 and one with value 1; ACK of either => Direct supported.
 *      If supported, the remaining CV#8 bits are read so the byte value is known.
 *   2. Paged   -- if the CV#8 value is already known, a single verify; otherwise a
 *      0..255 scan of CV#8 (Manufacturer ID) that also learns the value.
 *   3. Register -- same as Paged, against register 8 (= CV#8 mfg ID).
 *   4. Address-Only -- separate 0..127 scan of CV#1 (cannot reach CV#8). NOT assumed
 *      to be universally supported (the spec mandates it for command stations, not
 *      for every decoder).
 * If no stage acknowledges, supported_modes == 0 and result is NO_ACK.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_task_detect.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

#include <string.h>

    /** @brief CV probed by the Direct and Paged stages: CV#8 (Manufacturer ID) is a known-populated CV, the same one the spec uses for Direct detection. */
#define DCC_DETECT_CV          8u
    /** @brief CV#8 bit the Direct stage verifies first (spec §E lines 95-99). */
#define DCC_DETECT_BIT         7u
    /** @brief Register probed by the Register stage; register 8 maps to CV#8 (Mobile) / CV#520 (Accessory). */
#define DCC_DETECT_REGISTER    8u
    /** @brief Last candidate value tried by the Paged and Register 0..255 scans. */
#define DCC_DETECT_SCAN_MAX    255u
    /** @brief Last candidate address tried by the Address-Only 0..127 scan. */
#define DCC_DETECT_ADDRESS_MAX 127u

    /** @brief States of the detection task state machine. */
typedef enum {

    DCC_TASK_DETECT_STATE_IDLE,                  /**< No detection in progress */
    DCC_TASK_DETECT_STATE_PROBE_DIRECT_0,        /**< Direct: verify of CV#8 bit 7 == 0 outstanding */
    DCC_TASK_DETECT_STATE_PROBE_DIRECT_1,        /**< Direct: verify of CV#8 bit 7 == 1 outstanding */
    DCC_TASK_DETECT_STATE_READ_DIRECT,           /**< Direct supported: reading CV#8 bits 6..0 to learn the byte */
    DCC_TASK_DETECT_STATE_PROBE_PAGED_VERIFY,    /**< Paged: single verify of the known CV#8 value outstanding */
    DCC_TASK_DETECT_STATE_PROBE_PAGED_SCAN,      /**< Paged: 0..255 scan of CV#8 in progress */
    DCC_TASK_DETECT_STATE_PROBE_REGISTER_VERIFY, /**< Register: single verify of the known register 8 value outstanding */
    DCC_TASK_DETECT_STATE_PROBE_REGISTER_SCAN,   /**< Register: 0..255 scan of register 8 in progress */
    DCC_TASK_DETECT_STATE_PROBE_ADDRESS_SCAN,    /**< Address-Only: 0..127 scan of CV#1 in progress */

} dcc_task_detect_state_enum;

    /** @brief Singleton context for the detection task. */
typedef struct {

    const interface_dcc_service_mode_task_detect_t *interface; /**< Injected primitive dependencies (unwired members are NULL) */
    dcc_task_detect_state_enum state;                          /**< Current state machine state */
    uint8_t supported_modes;                                   /**< DCC_SERVICE_MODE_SUPPORTED_* bits accumulated so far */
    uint8_t value;                                             /**< CV#8 value as learned so far */
    bool value_known;                                          /**< true once the full CV#8 value has been established */
    uint8_t scan_value;                                        /**< Candidate value currently being verified in a scan */
    uint8_t read_bit;                                          /**< Direct read: CV#8 bit currently being verified (6 down to 0) */
    bool ack_result;                                           /**< Outcome of the most recent primitive: true = ACK measured */
    dcc_service_mode_task_on_detect_callback_t on_detect;      /**< Detection-complete callback */

} dcc_service_mode_task_detect_context_t;

    /** @brief Singleton task context. */
static dcc_service_mode_task_detect_context_t _context;

    /**
     * @brief Ends detection normally: returns to IDLE and reports through on_detect.
     *
     * @details The result is SUCCESS when at least one mode was detected, NO_ACK when supported_modes is 0.
     */
static void _finish(void) {

    dcc_service_mode_result_enum result = _context.supported_modes ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_NO_ACK;

    _context.state = DCC_TASK_DETECT_STATE_IDLE;

    if (_context.on_detect) {

        _context.on_detect(result, _context.supported_modes);

    }

}

    /**
     * @brief Ends detection after a primitive call failed to start.
     *
     * @details Same shape as _finish(), but reports the given result (e.g. DCC_SERVICE_MODE_BUSY) instead of
     * deriving SUCCESS/NO_ACK from supported_modes, and always resets to IDLE so a later detect_mode() call
     * isn't permanently locked out. The modes found so far are still delivered.
     *
     * @param result Result to report through on_detect.
     */
static void _fail(dcc_service_mode_result_enum result) {

    _context.state = DCC_TASK_DETECT_STATE_IDLE;

    if (_context.on_detect) {

        _context.on_detect(result, _context.supported_modes);

    }

}

    /**
     * @brief Starts reading the rest of CV#8 once Direct is known to be supported.
     *
     * @details Bit 7 was already determined by the probe; bits 6..0 are verified one at a time from
     * READ_DIRECT so the byte is known before the Paged and Register stages. Fails with BUSY if the first
     * verify_bit cannot be started.
     */
static void _begin_direct_read(void) {

    /* CV#8 bit 7 already determined by detection; read bits 6..0 to complete the byte. */
    _context.read_bit = DCC_DETECT_BIT - 1u;
    _context.state = DCC_TASK_DETECT_STATE_READ_DIRECT;

    if (!_context.interface->direct_verify_bit(DCC_DETECT_CV, _context.read_bit, true)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

static void _begin_register(void); /* forward declaration for the skip-ahead below */

    /**
     * @brief Starts the Paged stage.
     *
     * @details Skips straight to the Register stage when paged_verify is not wired. If the CV#8 value is
     * already known it is confirmed with a single paged_verify (PROBE_PAGED_VERIFY); otherwise a 0..255 scan
     * begins (PROBE_PAGED_SCAN). Fails with BUSY if the primitive cannot be started.
     */
static void _begin_paged(void) {

    if (!_context.interface->paged_verify) {

        _begin_register(); /* Paged mode not compiled in; nothing to probe. */
        return;

    }

    if (_context.value_known) {

        _context.state = DCC_TASK_DETECT_STATE_PROBE_PAGED_VERIFY;

        if (!_context.interface->paged_verify(DCC_DETECT_CV, _context.value)) {

            _fail(DCC_SERVICE_MODE_BUSY);

        }

    } else {

        _context.scan_value = 0;
        _context.state = DCC_TASK_DETECT_STATE_PROBE_PAGED_SCAN;

        if (!_context.interface->paged_verify(DCC_DETECT_CV, 0)) {

            _fail(DCC_SERVICE_MODE_BUSY);

        }

    }

}

static void _begin_address(void); /* forward declaration for the skip-ahead below */

    /**
     * @brief Starts the Register stage.
     *
     * @details Skips straight to the Address-Only stage when register_verify is not wired. If the CV#8 value
     * is already known it is confirmed with a single register_verify of register 8 (PROBE_REGISTER_VERIFY);
     * otherwise a 0..255 scan begins (PROBE_REGISTER_SCAN). Fails with BUSY if the primitive cannot be started.
     */
static void _begin_register(void) {

    if (!_context.interface->register_verify) {

        _begin_address(); /* Register mode not compiled in; nothing to probe. */
        return;

    }

    if (_context.value_known) {

        _context.state = DCC_TASK_DETECT_STATE_PROBE_REGISTER_VERIFY;

        if (!_context.interface->register_verify(DCC_DETECT_REGISTER, _context.value)) {

            _fail(DCC_SERVICE_MODE_BUSY);

        }

    } else {

        _context.scan_value = 0;
        _context.state = DCC_TASK_DETECT_STATE_PROBE_REGISTER_SCAN;

        if (!_context.interface->register_verify(DCC_DETECT_REGISTER, 0)) {

            _fail(DCC_SERVICE_MODE_BUSY);

        }

    }

}

    /**
     * @brief Starts the Address-Only stage.
     *
     * @details Finishes detection when address_verify is not wired. Otherwise begins a 0..127 scan of CV#1
     * (PROBE_ADDRESS_SCAN); fails with BUSY if the primitive cannot be started.
     */
static void _begin_address(void) {

    if (!_context.interface->address_verify) {

        _finish(); /* Address-only mode not compiled in; detection is done. */
        return;

    }

    _context.scan_value = 0;
    _context.state = DCC_TASK_DETECT_STATE_PROBE_ADDRESS_SCAN;

    if (!_context.interface->address_verify(0)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

    /**
     * @brief Initialize the detect task module. Call once during DccConfig_initialize().
     *
     * @details Clears the singleton context (state IDLE, no callback) and stores the interface pointer.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_service_mode_task_detect_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccServiceModeTaskDetect_initialize(const interface_dcc_service_mode_task_detect_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

    /**
     * @brief Probe the decoder for ALL supported service modes (Direct, Paged, Register, Address-Only).
     *
     * @details Algorithm:
     * -# Reject if a detection is already in progress (state not IDLE)
     * -# Clear the accumulated modes, CV#8 value, scan state and store the callback
     * -# If direct_verify_bit is not wired, skip straight to _begin_paged() and return true
     * -# Otherwise enter PROBE_DIRECT_0 and issue a Direct verify of CV#8 bit 7 == 0
     * -# If that primitive refuses to start, return to IDLE and report failure to the caller
     * -# The remaining stages run from DccServiceModeTaskDetect_on_primitive_complete()
     *
     * Return value and on_detect timing are NOT symmetric across the two ways this can fail to
     * really start, by design choice, not oversight -- flagged in Jim Kueneman's review of
     * upstream PR #2 (2026-09-23), documented here rather than unified (see that PR's discussion
     * for why: unifying would mean propagating a start/fail result up through the whole
     * _begin_paged() -> _begin_register() -> _begin_address() cascade, each of which can also
     * legitimately complete synchronously with a genuine "no modes supported" result, not just
     * fail to start).
     *
     *   - Direct wired, direct_verify_bit() fails to start: returns false, on_detect is NOT
     *     called. The false return is the only signal.
     *   - Direct not wired (or every mode unwired, cascading all the way through _begin_paged()/
     *     _begin_register()/_begin_address()): this function calls the next stage and returns
     *     true UNCONDITIONALLY. If that stage's own probe then fails to start -- or there is
     *     nothing left to probe at all -- the resulting _fail()/_finish() call fires on_detect
     *     SYNCHRONOUSLY, before this function has returned to its own caller.
     *
     * A caller that assumes "true means wait for the callback" can therefore see on_detect fire
     * before it has finished handling this call. See dcc_service_mode_task_detect_Test.cxx's
     * "Synchronous-callback asymmetry" tests, which pin down both sides so a future change that
     * unifies this does not silently change behavior.
     *
     * @verbatim
     * @param on_detect Called when detection completes; supported_modes = capability bitmask.
     * @endverbatim
     *
     * @return true if started; false if another detection is running, or if the Direct primitive is wired and refuses to start.
     */
bool DccServiceModeTaskDetect_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect) {

    if (_context.state != DCC_TASK_DETECT_STATE_IDLE) {

        return false;

    }

    _context.supported_modes = 0;
    _context.value           = 0;
    _context.value_known     = false;
    _context.scan_value      = 0;
    _context.read_bit        = 0;
    _context.ack_result      = false;
    _context.on_detect       = on_detect;

    if (!_context.interface->direct_verify_bit) {

        /* Direct mode not compiled in; skip straight to Paged. _begin_paged()
         * sets its own state, and reports through on_detect if it in turn
         * has to fail/skip further -- this call still counts as "started". */
        _begin_paged();
        return true;

    }

    _context.state           = DCC_TASK_DETECT_STATE_PROBE_DIRECT_0;

    if (!_context.interface->direct_verify_bit(DCC_DETECT_CV, DCC_DETECT_BIT, false)) {

        _context.state = DCC_TASK_DETECT_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Probe one Direct-mode bit of the detect CV (verify value 1); a refused primitive fails the task with BUSY.
     *
     * @param bit_position CV#8 bit to verify (0-7).
     */
static void _probe_direct_bit(uint8_t bit_position) {

    if (!_context.interface->direct_verify_bit(DCC_DETECT_CV, bit_position, true)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

    /** @brief Paged scan: advance to the next candidate value and verify it. */
static void _scan_paged_next(void) {

    _context.scan_value++;

    if (!_context.interface->paged_verify(DCC_DETECT_CV, _context.scan_value)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

    /** @brief Register scan: advance to the next candidate value and verify it. */
static void _scan_register_next(void) {

    _context.scan_value++;

    if (!_context.interface->register_verify(DCC_DETECT_REGISTER, _context.scan_value)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

    /** @brief Address scan: advance to the next candidate address and verify it. */
static void _scan_address_next(void) {

    _context.scan_value++;

    if (!_context.interface->address_verify(_context.scan_value)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

    /**
     * @brief Notify the task module that the primitive has finished its full operation (including recovery packets).
     *
     * @details Algorithm:
     * -# Record the ACK outcome: SUCCESS from the primitive means a valid ACK was measured, anything else means no ACK
     * -# PROBE_DIRECT_0 / PROBE_DIRECT_1: an ACK marks Direct supported, fixes CV#8 bit 7 and starts the Direct read
     *    of bits 6..0; no ACK on bit 7 == 0 tries bit 7 == 1, no ACK on that moves on to Paged with the value unknown
     * -# READ_DIRECT: record the bit, then verify the next lower bit or, after bit 0, mark the value known and start Paged
     * -# PROBE_PAGED_VERIFY / PROBE_REGISTER_VERIFY: an ACK marks that mode supported; either way move to the next stage
     * -# PROBE_PAGED_SCAN / PROBE_REGISTER_SCAN: an ACK marks the mode supported and learns the value; an exhausted
     *    scan moves on without it; otherwise verify the next candidate
     * -# PROBE_ADDRESS_SCAN: an ACK marks Address-Only supported; an ACK or an exhausted scan finishes detection
     * -# Ignore the event when IDLE
     *
     * @verbatim
     * @param result Result of the primitive operation (passed through from primitive callback).
     * @endverbatim
     */
void DccServiceModeTaskDetect_on_primitive_complete(dcc_service_mode_result_enum result) {

    /* The common module measures the ACK pulse width internally and reports the
     * outcome here: SUCCESS = valid ACK detected, anything else = no ACK. */
    _context.ack_result = (result == DCC_SERVICE_MODE_SUCCESS);

    switch (_context.state) {

        case DCC_TASK_DETECT_STATE_PROBE_DIRECT_0:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_DIRECT;
                _context.value = 0; /* CV#8 bit 7 == 0 */
                _begin_direct_read();

            } else {

                _context.state = DCC_TASK_DETECT_STATE_PROBE_DIRECT_1;
                _probe_direct_bit(DCC_DETECT_BIT);

            }

            break;

        case DCC_TASK_DETECT_STATE_PROBE_DIRECT_1:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_DIRECT;
                _context.value = (uint8_t)(1u << DCC_DETECT_BIT); /* CV#8 bit 7 == 1 */
                _begin_direct_read();

            } else {

                _begin_paged(); /* Direct not supported; value still unknown */

            }

            break;

        case DCC_TASK_DETECT_STATE_READ_DIRECT:

            if (_context.ack_result) {

                _context.value |= (uint8_t)(1u << _context.read_bit);

            }

            if (_context.read_bit == 0u) {

                _context.value_known = true;
                _begin_paged();

            } else {

                _context.read_bit--;
                _probe_direct_bit(_context.read_bit);

            }

            break;

        case DCC_TASK_DETECT_STATE_PROBE_PAGED_VERIFY:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_PAGED;

            }

            _begin_register();

            break;

        case DCC_TASK_DETECT_STATE_PROBE_PAGED_SCAN:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_PAGED;
                _context.value = _context.scan_value;
                _context.value_known = true;
                _begin_register();

            } else if (_context.scan_value == DCC_DETECT_SCAN_MAX) {

                _begin_register();

            } else {

                _scan_paged_next();

            }

            break;

        case DCC_TASK_DETECT_STATE_PROBE_REGISTER_VERIFY:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_REGISTER;

            }

            _begin_address();

            break;

        case DCC_TASK_DETECT_STATE_PROBE_REGISTER_SCAN:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_REGISTER;
                _context.value = _context.scan_value;
                _context.value_known = true;
                _begin_address();

            } else if (_context.scan_value == DCC_DETECT_SCAN_MAX) {

                _begin_address();

            } else {

                _scan_register_next();

            }

            break;

        case DCC_TASK_DETECT_STATE_PROBE_ADDRESS_SCAN:

            if (_context.ack_result) {

                _context.supported_modes |= DCC_SERVICE_MODE_SUPPORTED_ADDRESS;
                _finish();

            } else if (_context.scan_value == DCC_DETECT_ADDRESS_MAX) {

                _finish();

            } else {

                _scan_address_next();

            }

            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */
