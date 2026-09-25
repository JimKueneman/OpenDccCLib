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
 * @file dcc_service_mode_task_paged.c
 * @brief Task orchestrator for Paged mode CV programming (S-9.2.3 §E).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_task_paged.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

#include <string.h>

    /** @brief States of the paged mode task state machine. */
typedef enum {

    DCC_TASK_PAGED_STATE_IDLE,                /**< No operation in progress */
    DCC_TASK_PAGED_STATE_READ_CV,             /**< read_cv: paged_verify of the current scan value outstanding */
    DCC_TASK_PAGED_STATE_WRITE_CV,            /**< write_cv: paged_write outstanding */
    DCC_TASK_PAGED_STATE_WRITE_CV_VERIFY,     /**< write_cv: paged_verify of the written value outstanding */
    DCC_TASK_PAGED_STATE_READ_BIT_READ_BYTE,  /**< read_bit: scanning for the full byte */
    DCC_TASK_PAGED_STATE_WRITE_BIT_READ_BYTE, /**< write_bit: scanning for the current byte before modifying it */
    DCC_TASK_PAGED_STATE_WRITE_BIT_WRITE,     /**< write_bit: paged_write of the modified byte outstanding */
    DCC_TASK_PAGED_STATE_WRITE_BIT_VERIFY,    /**< write_bit: paged_verify of the modified byte outstanding */

} dcc_task_paged_state_enum;

    /** @brief Singleton context for the paged mode task. */
typedef struct {

    const interface_dcc_service_mode_task_paged_t *interface; /**< Injected primitive dependencies */
    dcc_task_paged_state_enum state;                          /**< Current state machine state */
    uint16_t cv_number;                                       /**< CV number of the operation in progress */
    uint8_t bit_position;                                     /**< Bit requested by read_bit / write_bit */
    bool bit_value;                                           /**< Bit value requested by write_bit */
    uint8_t value;                                            /**< Byte to write (write_cv), or the modified byte (write_bit) */
    uint8_t scan_value;                                       /**< Candidate value currently being verified in a 0-255 scan */
    uint8_t current_step;                                     /**< Steps completed so far, reported through on_progress */
    bool ack_result;                                          /**< Outcome of the most recent primitive: true = ACK measured */
    dcc_service_mode_task_on_complete_callback_t on_complete; /**< Completion callback for the operation in progress */
    dcc_service_mode_task_on_progress_callback_t on_progress; /**< Progress callback (nullable) */

} dcc_service_mode_task_paged_context_t;

    /** @brief Singleton task context. */
static dcc_service_mode_task_paged_context_t _context;

    /**
     * @brief Forwards a progress notification to the caller's on_progress callback, if one was supplied. estimated_steps is always 0.
     *
     * @param phase Phase of the operation the completed step belongs to.
     */
static void _report_progress(dcc_task_phase_enum phase) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, 0);

    }

}

    /**
     * @brief Returns the task to IDLE and reports the final result through on_complete, if one was supplied.
     *
     * @param result Final result of the operation.
     * @param value Value delivered with the result (CV byte, bit value, or 0).
     */
static void _complete(dcc_service_mode_result_enum result, uint8_t value) {

    _context.state = DCC_TASK_PAGED_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

    }

}

    /** @brief read_cv / read_bit: report the scanned value (or the requested bit of it) as the result. */
static void _report_found_value(void) {

    if (_context.state == DCC_TASK_PAGED_STATE_READ_BIT_READ_BYTE) {

        uint8_t bit_result = (_context.scan_value >> _context.bit_position) & 1u;
        _complete(DCC_SERVICE_MODE_SUCCESS, bit_result);

    } else {

        _complete(DCC_SERVICE_MODE_SUCCESS, _context.scan_value);

    }

}

    /** @brief write_bit: apply the requested bit to the scanned value and start the write of the modified byte. */
static void _begin_write_bit(void) {

    uint8_t modified = _context.scan_value;

    if (_context.bit_value) {

        modified |= (uint8_t)(1u << _context.bit_position);

    } else {

        modified &= (uint8_t)(~(1u << _context.bit_position));

    }

    _context.value = modified;
    _context.state = DCC_TASK_PAGED_STATE_WRITE_BIT_WRITE;

    if (!_context.interface->paged_write(_context.cv_number, modified)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Advances a 0-255 paged_verify scan (read_cv, read_bit, or the read phase of write_bit) after one verify completes.
     *
     * @details Algorithm:
     * -# Count the step and report READ progress
     * -# On ACK the scanned value is the CV byte: hand off to _begin_write_bit() when next_on_found is
     *    WRITE_BIT_WRITE, otherwise report it (or its requested bit) through _report_found_value()
     * -# If value 255 has been tried without an ACK, complete with ERROR (no value acknowledged)
     * -# Otherwise verify the next candidate value, completing with BUSY if the primitive refuses to start
     *
     * @param next_on_found State to continue in when the value is found: WRITE_BIT_WRITE continues into the write, IDLE reports the value.
     */
static void _advance_scan(dcc_task_paged_state_enum next_on_found) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_READ);

    if (_context.ack_result) {

        if (next_on_found == DCC_TASK_PAGED_STATE_IDLE) {

            _report_found_value();

        } else {

            _begin_write_bit();

        }

    } else if (_context.scan_value == 255u) {

        _complete(DCC_SERVICE_MODE_ERROR, 0);

    } else {

        _context.scan_value++;

        if (!_context.interface->paged_verify(_context.cv_number, _context.scan_value)) {

            _complete(DCC_SERVICE_MODE_BUSY, 0);

        }

    }

}

    /**
     * @brief Advances write_cv after the paged_write completes: reports progress and issues the targeted paged_verify.
     *
     * @details The ACK outcome of the write itself is not evaluated; the verify that follows decides the
     * result. Completes with BUSY if the paged_verify cannot be started.
     */
static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_PAGED_STATE_WRITE_CV_VERIFY;

    if (!_context.interface->paged_verify(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes write_cv after the paged_verify.
     *
     * @details Reports the final progress step, then SUCCESS if the verify was acknowledged or VERIFY_FAIL
     * if not. The value delivered is the byte that was written.
     */
static void _advance_write_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY);

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

    /** @brief Advances write_bit after the paged_write of the modified byte: issues its paged_verify, completing with BUSY if it cannot start. */
static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_PAGED_STATE_WRITE_BIT_VERIFY;

    if (!_context.interface->paged_verify(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /** @brief Completes write_bit: SUCCESS if the modified byte verified, VERIFY_FAIL otherwise; value = the bit written (0 or 1). */
static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

    /**
     * @brief Initialize the paged task module. Call once during DccConfig_initialize().
     *
     * @details Clears the singleton context (state IDLE, no callbacks) and stores the interface pointer.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_service_mode_task_paged_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccServiceModeTaskPaged_initialize(const interface_dcc_service_mode_task_paged_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

    /**
     * @brief Read a CV byte using paged mode. Iterates paged_verify 0-255 until ACK.
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, scan value 0, cleared step count, callbacks
     * -# Enter READ_CV and issue paged_verify for value 0
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The scan continues from DccServiceModeTaskPaged_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param on_complete Called when complete: SUCCESS with the CV byte found, ERROR if no value was ACKed, or BUSY.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is out of range, or the first verify could not start.
     */
bool DccServiceModeTaskPaged_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (_context.state != DCC_TASK_PAGED_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_PAGED_STATE_READ_CV;

    if (!_context.interface->paged_verify(cv_number, 0)) {

        _context.state = DCC_TASK_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a CV byte then verify. Always 2 operations (write + targeted verify).
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, value, cleared step count, callbacks
     * -# Enter WRITE_CV and issue paged_write
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The verify runs from DccServiceModeTaskPaged_on_primitive_complete()
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
bool DccServiceModeTaskPaged_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (_context.state != DCC_TASK_PAGED_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.value        = value;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_PAGED_STATE_WRITE_CV;

    if (!_context.interface->paged_write(cv_number, value)) {

        _context.state = DCC_TASK_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Read a single CV bit using paged mode. Reads full byte, extracts bit.
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024 or bit positions above 7
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, bit position, scan value 0, cleared step count, callbacks
     * -# Enter READ_BIT_READ_BYTE and issue paged_verify for value 0
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The scan continues from DccServiceModeTaskPaged_on_primitive_complete(); the requested bit is extracted from the value found
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position (0-7).
     * @param on_complete Called when complete: SUCCESS with value 0 or 1, ERROR if no byte value was ACKed, or BUSY.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, a parameter is out of range, or the first verify could not start.
     */
bool DccServiceModeTaskPaged_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (_context.state != DCC_TASK_PAGED_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.bit_position          = bit_position;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_PAGED_STATE_READ_BIT_READ_BYTE;

    if (!_context.interface->paged_verify(cv_number, 0)) {

        _context.state = DCC_TASK_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a single CV bit using paged mode. Read-modify-write sequence.
     *
     * @details Algorithm:
     * -# Reject CV numbers outside 1-1024 or bit positions above 7
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: CV number, bit position, bit value, scan value 0, cleared step count, callbacks
     * -# Enter WRITE_BIT_READ_BYTE and issue paged_verify for value 0 to start the read scan
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# From DccServiceModeTaskPaged_on_primitive_complete(): once the byte is found, write the modified byte, then verify it
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position (0-7).
     * @param bit_value Value to write.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the bit written, ERROR if the read scan found nothing, or BUSY.
     * @param on_progress Called after each scan step of the read phase only (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, a parameter is out of range, or the first verify could not start.
     */
bool DccServiceModeTaskPaged_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (_context.state != DCC_TASK_PAGED_STATE_IDLE) {

        return false;

    }

    _context.cv_number           = cv_number;
    _context.bit_position          = bit_position;
    _context.bit_value    = bit_value;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_PAGED_STATE_WRITE_BIT_READ_BYTE;

    if (!_context.interface->paged_verify(cv_number, 0)) {

        _context.state = DCC_TASK_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Notify the task module that the primitive has finished its full operation (including recovery packets).
     *
     * @details Algorithm:
     * -# Record the ACK outcome: SUCCESS from the primitive means a valid ACK was measured, anything else means no ACK
     * -# Dispatch on the current state: the three scanning states share _advance_scan(), the others go to their _advance_* handler
     * -# Ignore the event when IDLE (no operation in progress)
     *
     * @verbatim
     * @param result Result of the primitive operation (passed through from primitive callback).
     * @endverbatim
     */
void DccServiceModeTaskPaged_on_primitive_complete(dcc_service_mode_result_enum result) {

    /* The common module measures the ACK pulse width internally and reports the
     * outcome here: SUCCESS = valid ACK detected, anything else = no ACK. */
    _context.ack_result = (result == DCC_SERVICE_MODE_SUCCESS);

    switch (_context.state) {

        case DCC_TASK_PAGED_STATE_READ_CV:

            _advance_scan(DCC_TASK_PAGED_STATE_IDLE);
            break;

        case DCC_TASK_PAGED_STATE_READ_BIT_READ_BYTE:

            _advance_scan(DCC_TASK_PAGED_STATE_IDLE);
            break;

        case DCC_TASK_PAGED_STATE_WRITE_BIT_READ_BYTE:

            _advance_scan(DCC_TASK_PAGED_STATE_WRITE_BIT_WRITE);
            break;

        case DCC_TASK_PAGED_STATE_WRITE_CV:

            _advance_write_cv();
            break;

        case DCC_TASK_PAGED_STATE_WRITE_CV_VERIFY:

            _advance_write_cv_verify();
            break;

        case DCC_TASK_PAGED_STATE_WRITE_BIT_WRITE:

            _advance_write_bit_write();
            break;

        case DCC_TASK_PAGED_STATE_WRITE_BIT_VERIFY:

            _advance_write_bit_verify();
            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */
