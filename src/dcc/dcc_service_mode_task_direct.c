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
 * @file dcc_service_mode_task_direct.c
 * @brief Task orchestrator for Direct mode CV programming (S-9.2.3 §E).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_task_direct.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

#include <string.h>

    /** @brief States of the direct mode task state machine. */
typedef enum {

    DCC_TASK_DIRECT_STATE_IDLE,             /**< No operation in progress */
    DCC_TASK_DIRECT_STATE_READ_CV,          /**< read_cv: verify_bit of the current bit position outstanding */
    DCC_TASK_DIRECT_STATE_READ_CV_VERIFY,   /**< read_cv: verify_byte of the assembled value outstanding */
    DCC_TASK_DIRECT_STATE_WRITE_CV,         /**< write_cv: write_byte outstanding */
    DCC_TASK_DIRECT_STATE_WRITE_CV_VERIFY,  /**< write_cv: verify_byte of the written value outstanding */
    DCC_TASK_DIRECT_STATE_READ_BIT,         /**< read_bit: verify_bit of value 1 outstanding */
    DCC_TASK_DIRECT_STATE_READ_BIT_CONFIRM, /**< read_bit: confirming verify_bit of value 0 outstanding */
    DCC_TASK_DIRECT_STATE_WRITE_BIT,        /**< write_bit: write_bit outstanding */
    DCC_TASK_DIRECT_STATE_WRITE_BIT_VERIFY, /**< write_bit: verify_bit of the written value outstanding */

} dcc_task_direct_state_enum;

    /** @brief Singleton context for the direct mode task. */
typedef struct {

    const interface_dcc_service_mode_task_direct_t *interface; /**< Injected primitive dependencies */
    dcc_task_direct_state_enum state;                          /**< Current state machine state */
    uint16_t cv_number;                                        /**< CV number of the operation in progress */
    uint8_t bit_position;                                      /**< Bit being verified: read_cv scan position, or the requested bit */
    bool bit_value;                                            /**< Bit value requested by write_bit */
    uint8_t value;                                             /**< Byte to write, or the byte assembled by read_cv */
    bool any_ack;                                              /**< read_cv: at least one bit-verify was acknowledged */
    uint8_t current_step;                                      /**< Steps completed so far, reported through on_progress */
    bool ack_result;                                           /**< Outcome of the most recent primitive: true = ACK measured */
    dcc_service_mode_task_on_complete_callback_t on_complete;  /**< Completion callback for the operation in progress */
    dcc_service_mode_task_on_progress_callback_t on_progress;  /**< Progress callback (nullable) */

} dcc_service_mode_task_direct_context_t;

    /** @brief Singleton task context. */
static dcc_service_mode_task_direct_context_t _context;

    /**
     * @brief Forwards a progress notification to the caller's on_progress callback, if one was supplied.
     *
     * @param phase Phase of the operation the completed step belongs to.
     * @param estimated_steps Total step count reported to the caller.
     */
static void _report_progress(dcc_task_phase_enum phase, uint8_t estimated_steps) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, estimated_steps);

    }

}

    /**
     * @brief Returns the task to IDLE and reports the final result through on_complete, if one was supplied.
     *
     * @param result Final result of the operation.
     * @param value Value delivered with the result (CV byte, bit value, or 0).
     */
static void _complete(dcc_service_mode_result_enum result, uint8_t value) {

    _context.state = DCC_TASK_DIRECT_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

    }

}

    /**
     * @brief Advances read_cv after one bit-verify completes.
     *
     * @details Algorithm:
     * -# If the verify was acknowledged, set that bit in the assembled value and remember that something answered
     * -# Move to the next bit position and report progress (9 steps total)
     * -# If all 8 bits are done, enter READ_CV_VERIFY and issue a verify_byte of the assembled value;
     *    a missing ACK reads as 0, so a track with no decoder assembles 0x00 exactly like a CV that holds 0
     * -# Otherwise issue the verify_bit (value 1) for the next bit
     * -# If the primitive refuses to start, complete with BUSY
     */
static void _advance_read_cv(void) {

    if (_context.ack_result) {

        _context.value |= (uint8_t)(1u << _context.bit_position);
        _context.any_ack = true;

    }

    _context.bit_position++;
    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_READ, 9);

    if (_context.bit_position > 7) {

        /* A missing ACK reads as a 0 bit, so with no decoder on the track the
         * eight bit-verifies assemble 0x00, the same as a CV that holds 0.
         * Confirm the assembled byte with one verify_byte before reporting it. */
        _context.state = DCC_TASK_DIRECT_STATE_READ_CV_VERIFY;

        if (!_context.interface->verify_byte(_context.cv_number, _context.value)) {

            _complete(DCC_SERVICE_MODE_BUSY, 0);

        }

    } else if (!_context.interface->verify_bit(_context.cv_number, _context.bit_position, true)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes read_cv after the confirming verify_byte.
     *
     * @details Reports the final progress step, then SUCCESS with the assembled byte when the verify_byte was
     * acknowledged. Without that ACK, reports VERIFY_FAIL with the assembled byte if some bit-verifies had
     * answered (a misread), or NO_ACK with value 0 if nothing answered at all (no decoder, or one that does not ACK).
     */
static void _advance_read_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY, 9);

    if (_context.ack_result) {

        _complete(DCC_SERVICE_MODE_SUCCESS, _context.value);

    } else if (_context.any_ack) {

        /* The decoder answered some bits but not the byte they make up: a misread. */
        _complete(DCC_SERVICE_MODE_VERIFY_FAIL, _context.value);

    } else {

        /* Nothing answered at all: no decoder, or one that doesn't ACK. */
        _complete(DCC_SERVICE_MODE_NO_ACK, 0);

    }

}

    /**
     * @brief Advances write_cv after the write_byte completes: reports progress and issues the verify_byte.
     *
     * @details The ACK outcome of the write itself is not evaluated; the verify that follows decides the
     * result. Completes with BUSY if the verify_byte cannot be started.
     */
static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE, 2);
    _context.state = DCC_TASK_DIRECT_STATE_WRITE_CV_VERIFY;

    if (!_context.interface->verify_byte(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes write_cv after the verify_byte.
     *
     * @details Reports the final progress step, then SUCCESS if the verify was acknowledged or VERIFY_FAIL
     * if not. The value delivered is the byte that was written.
     */
static void _advance_write_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY, 2);

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

    /**
     * @brief Advances read_bit after the verify of value 1.
     *
     * @details An ACK completes with SUCCESS and value 1. No ACK is also what an absent decoder looks like,
     * so the task enters READ_BIT_CONFIRM and verifies value 0 before reporting. Completes with BUSY if that
     * verify_bit cannot be started.
     */
static void _advance_read_bit(void) {

    if (_context.ack_result) {

        _complete(DCC_SERVICE_MODE_SUCCESS, 1u);
        return;

    }

    /* No ACK for value 1 is also what an absent decoder looks like. Confirm the
     * bit really is 0 with a verify of the opposite value before reporting it. */
    _context.state = DCC_TASK_DIRECT_STATE_READ_BIT_CONFIRM;

    if (!_context.interface->verify_bit(_context.cv_number, _context.bit_position, false)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes read_bit after the confirming verify of value 0.
     *
     * @details An ACK completes with SUCCESS and value 0. If neither value was acknowledged, completes with
     * NO_ACK (no decoder, or one that does not ACK).
     */
static void _advance_read_bit_confirm(void) {

    if (_context.ack_result) {

        _complete(DCC_SERVICE_MODE_SUCCESS, 0u);

    } else {

        /* Neither value answered: no decoder, or one that doesn't ACK. */
        _complete(DCC_SERVICE_MODE_NO_ACK, 0);

    }

}

    /** @brief Advances write_bit after the write_bit primitive: issues the verify_bit of the written value, completing with BUSY if it cannot start. */
static void _advance_write_bit(void) {

    _context.state = DCC_TASK_DIRECT_STATE_WRITE_BIT_VERIFY;

    if (!_context.interface->verify_bit(_context.cv_number, _context.bit_position, _context.bit_value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /** @brief Completes write_bit: SUCCESS if the verify_bit was acknowledged, VERIFY_FAIL otherwise; value = the bit written (0 or 1). */
static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

    /**
     * @brief Initialize the direct task module. Call once during DccConfig_initialize().
     *
     * @details Clears the singleton context (state IDLE, no callbacks) and stores the interface pointer.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_service_mode_task_direct_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccServiceModeTaskDirect_initialize(const interface_dcc_service_mode_task_direct_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

    /**
     * @brief Read a CV byte using direct mode. Sequences 8 verify_bit operations (bits 0-7), then one verify_byte of the assembled value.
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, bit position 0, cleared value / any_ack / step count, callbacks
     * -# Enter READ_CV and issue verify_bit for bit 0 with value 1
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The remaining steps run from DccServiceModeTaskDirect_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param on_complete Called when complete: SUCCESS with the CV byte, NO_ACK, VERIFY_FAIL, or BUSY.
     * @param on_progress Called after each of the 9 steps (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is out of range, or the first primitive could not start.
     */
bool DccServiceModeTaskDirect_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (_context.state != DCC_TASK_DIRECT_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.bit_position          = 0;
    _context.value        = 0;
    _context.any_ack      = false;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_DIRECT_STATE_READ_CV;

    if (!_context.interface->verify_bit(cv_number, 0, true)) {

        _context.state = DCC_TASK_DIRECT_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a CV byte then verify. Always 2 operations (write + verify).
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, value, cleared step count, callbacks
     * -# Enter WRITE_CV and issue write_byte
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The verify runs from DccServiceModeTaskDirect_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param value Byte to write.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the byte written, or BUSY.
     * @param on_progress Called after each step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is out of range, or the write could not start.
     */
bool DccServiceModeTaskDirect_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (_context.state != DCC_TASK_DIRECT_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.value        = value;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_DIRECT_STATE_WRITE_CV;

    if (!_context.interface->write_byte(cv_number, value)) {

        _context.state = DCC_TASK_DIRECT_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Read a single CV bit: verify value 1, then value 0 if that was silent.
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024 or bit positions above 7
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, bit position, callbacks
     * -# Enter READ_BIT and issue verify_bit with value 1
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The confirming verify of value 0 runs from DccServiceModeTaskDirect_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position (0-7).
     * @param on_complete Called when complete: SUCCESS with value 1 or 0, NO_ACK if neither verify was ACKed, or BUSY.
     * @param on_progress Not used by this operation (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, a parameter is out of range, or the first verify could not start.
     */
bool DccServiceModeTaskDirect_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (_context.state != DCC_TASK_DIRECT_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.bit_position          = bit_position;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_DIRECT_STATE_READ_BIT;

    if (!_context.interface->verify_bit(cv_number, bit_position, true)) {

        _context.state = DCC_TASK_DIRECT_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a single CV bit then verify. Always 2 operations (write_bit + verify_bit).
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024 or bit positions above 7
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, bit position, bit value, callbacks
     * -# Enter WRITE_BIT and issue write_bit
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The verify runs from DccServiceModeTaskDirect_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position (0-7).
     * @param bit_value Value to write.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the bit written (0 or 1), or BUSY.
     * @param on_progress Not used by this operation (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, a parameter is out of range, or the write could not start.
     */
bool DccServiceModeTaskDirect_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (_context.state != DCC_TASK_DIRECT_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.bit_position          = bit_position;
    _context.bit_value    = bit_value;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_DIRECT_STATE_WRITE_BIT;

    if (!_context.interface->write_bit(cv_number, bit_position, bit_value)) {

        _context.state = DCC_TASK_DIRECT_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Notify the task module that the primitive has finished its full operation (including recovery packets).
     *
     * @details Algorithm:
     * -# Record the ACK outcome: SUCCESS from the primitive means a valid ACK was measured, anything else means no ACK
     * -# Dispatch on the current state to the matching _advance_* step handler
     * -# Ignore the event when IDLE (no operation in progress)
     *
     * @verbatim
     * @param result Result of the primitive operation (passed through from primitive callback).
     * @endverbatim
     */
void DccServiceModeTaskDirect_on_primitive_complete(dcc_service_mode_result_enum result) {

    /* The common module measures the ACK pulse width internally and reports the
     * outcome here: SUCCESS = valid ACK detected, anything else = no ACK. */
    _context.ack_result = (result == DCC_SERVICE_MODE_SUCCESS);

    switch (_context.state) {

        case DCC_TASK_DIRECT_STATE_READ_CV:

            _advance_read_cv();
            break;

        case DCC_TASK_DIRECT_STATE_READ_CV_VERIFY:

            _advance_read_cv_verify();
            break;

        case DCC_TASK_DIRECT_STATE_WRITE_CV:

            _advance_write_cv();
            break;

        case DCC_TASK_DIRECT_STATE_WRITE_CV_VERIFY:

            _advance_write_cv_verify();
            break;

        case DCC_TASK_DIRECT_STATE_READ_BIT:

            _advance_read_bit();
            break;

        case DCC_TASK_DIRECT_STATE_READ_BIT_CONFIRM:

            _advance_read_bit_confirm();
            break;

        case DCC_TASK_DIRECT_STATE_WRITE_BIT:

            _advance_write_bit();
            break;

        case DCC_TASK_DIRECT_STATE_WRITE_BIT_VERIFY:

            _advance_write_bit_verify();
            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */
