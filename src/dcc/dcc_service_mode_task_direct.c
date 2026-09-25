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

typedef enum {

    DCC_TASK_DIRECT_STATE_IDLE,
    DCC_TASK_DIRECT_STATE_READ_CV,
    DCC_TASK_DIRECT_STATE_READ_CV_VERIFY,
    DCC_TASK_DIRECT_STATE_WRITE_CV,
    DCC_TASK_DIRECT_STATE_WRITE_CV_VERIFY,
    DCC_TASK_DIRECT_STATE_READ_BIT,
    DCC_TASK_DIRECT_STATE_READ_BIT_CONFIRM,
    DCC_TASK_DIRECT_STATE_WRITE_BIT,
    DCC_TASK_DIRECT_STATE_WRITE_BIT_VERIFY,

} dcc_task_direct_state_enum;

typedef struct {

    const interface_dcc_service_mode_task_direct_t *interface;
    dcc_task_direct_state_enum state;
    uint16_t cv_number;
    uint8_t bit_position;
    bool bit_value;
    uint8_t value;
    bool any_ack;
    uint8_t current_step;
    bool ack_result;
    dcc_service_mode_task_on_complete_callback_t on_complete;
    dcc_service_mode_task_on_progress_callback_t on_progress;

} dcc_service_mode_task_direct_context_t;

static dcc_service_mode_task_direct_context_t _context;

static void _report_progress(dcc_task_phase_enum phase, uint8_t estimated_steps) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, estimated_steps);

    }

}

static void _complete(dcc_service_mode_result_t result, uint8_t value) {

    _context.state = DCC_TASK_DIRECT_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

    }

}

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

static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE, 8);
    _context.state = DCC_TASK_DIRECT_STATE_WRITE_CV_VERIFY;

    if (!_context.interface->verify_byte(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

static void _advance_write_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY, 8);

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

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

static void _advance_read_bit_confirm(void) {

    if (_context.ack_result) {

        _complete(DCC_SERVICE_MODE_SUCCESS, 0u);

    } else {

        /* Neither value answered: no decoder, or one that doesn't ACK. */
        _complete(DCC_SERVICE_MODE_NO_ACK, 0);

    }

}

static void _advance_write_bit(void) {

    _context.state = DCC_TASK_DIRECT_STATE_WRITE_BIT_VERIFY;

    if (!_context.interface->verify_bit(_context.cv_number, _context.bit_position, _context.bit_value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

void DccServiceModeTaskDirect_initialize(const interface_dcc_service_mode_task_direct_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

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

void DccServiceModeTaskDirect_on_primitive_complete(dcc_service_mode_result_t result) {

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
