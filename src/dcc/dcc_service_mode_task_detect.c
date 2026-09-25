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

/* CV#8 (Manufacturer ID) is a known-populated CV -- the same one the spec uses
 * for Direct detection. Register 8 maps to CV#8 (Mobile) / CV#520 (Accessory). */
#define DCC_DETECT_CV          8u
#define DCC_DETECT_BIT         7u
#define DCC_DETECT_REGISTER    8u
#define DCC_DETECT_SCAN_MAX    255u
#define DCC_DETECT_ADDRESS_MAX 127u

typedef enum {

    DCC_TASK_DETECT_STATE_IDLE,
    DCC_TASK_DETECT_STATE_PROBE_DIRECT_0,
    DCC_TASK_DETECT_STATE_PROBE_DIRECT_1,
    DCC_TASK_DETECT_STATE_READ_DIRECT,
    DCC_TASK_DETECT_STATE_PROBE_PAGED_VERIFY,
    DCC_TASK_DETECT_STATE_PROBE_PAGED_SCAN,
    DCC_TASK_DETECT_STATE_PROBE_REGISTER_VERIFY,
    DCC_TASK_DETECT_STATE_PROBE_REGISTER_SCAN,
    DCC_TASK_DETECT_STATE_PROBE_ADDRESS_SCAN,

} dcc_task_detect_state_enum;

typedef struct {

    const interface_dcc_service_mode_task_detect_t *interface;
    dcc_task_detect_state_enum state;
    uint8_t supported_modes;
    uint8_t value;
    bool value_known;
    uint8_t scan_value;
    uint8_t read_bit;
    bool ack_result;
    dcc_service_mode_task_on_detect_callback_t on_detect;

} dcc_service_mode_task_detect_context_t;

static dcc_service_mode_task_detect_context_t _context;

static void _finish(void) {

    dcc_service_mode_result_enum result = _context.supported_modes ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_NO_ACK;

    _context.state = DCC_TASK_DETECT_STATE_IDLE;

    if (_context.on_detect) {

        _context.on_detect(result, _context.supported_modes);

    }

}

/* Same shape as _finish(), but for a primitive call that failed to start --
 * reports the given result (e.g. DCC_SERVICE_MODE_BUSY) instead of deriving
 * SUCCESS/NO_ACK from supported_modes, and always resets to IDLE so a later
 * detect_mode() call isn't permanently locked out. */
static void _fail(dcc_service_mode_result_enum result) {

    _context.state = DCC_TASK_DETECT_STATE_IDLE;

    if (_context.on_detect) {

        _context.on_detect(result, _context.supported_modes);

    }

}

static void _begin_direct_read(void) {

    /* CV#8 bit 7 already determined by detection; read bits 6..0 to complete the byte. */
    _context.read_bit = DCC_DETECT_BIT - 1u;
    _context.state = DCC_TASK_DETECT_STATE_READ_DIRECT;

    if (!_context.interface->direct_verify_bit(DCC_DETECT_CV, _context.read_bit, true)) {

        _fail(DCC_SERVICE_MODE_BUSY);

    }

}

static void _begin_register(void); /* forward declaration for the skip-ahead below */

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

void DccServiceModeTaskDetect_initialize(const interface_dcc_service_mode_task_detect_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

/* Return value and on_detect timing are NOT symmetric across the two ways this can fail to
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
 * unifies this does not silently change behavior. */
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

    /** @brief Probe one Direct-mode bit of the detect CV; a refused primitive fails the task with BUSY. */
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
