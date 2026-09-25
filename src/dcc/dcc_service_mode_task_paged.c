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

typedef enum {

    DCC_TASK_PAGED_STATE_IDLE,
    DCC_TASK_PAGED_STATE_READ_CV,
    DCC_TASK_PAGED_STATE_WRITE_CV,
    DCC_TASK_PAGED_STATE_WRITE_CV_VERIFY,
    DCC_TASK_PAGED_STATE_READ_BIT_READ_BYTE,
    DCC_TASK_PAGED_STATE_WRITE_BIT_READ_BYTE,
    DCC_TASK_PAGED_STATE_WRITE_BIT_WRITE,
    DCC_TASK_PAGED_STATE_WRITE_BIT_VERIFY,

} dcc_task_paged_state_enum;

typedef struct {

    const interface_dcc_service_mode_task_paged_t *interface;
    dcc_task_paged_state_enum state;
    uint16_t cv_number;
    uint8_t bit_position;
    bool bit_value;
    uint8_t value;
    uint8_t scan_value;
    uint8_t current_step;
    bool ack_result;
    dcc_service_mode_task_on_complete_callback_t on_complete;
    dcc_service_mode_task_on_progress_callback_t on_progress;

} dcc_service_mode_task_paged_context_t;

static dcc_service_mode_task_paged_context_t _context;

static void _report_progress(dcc_task_phase_enum phase) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, 0);

    }

}

static void _complete(dcc_service_mode_result_t result, uint8_t value) {

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

static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_PAGED_STATE_WRITE_CV_VERIFY;

    if (!_context.interface->paged_verify(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

static void _advance_write_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY);

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_PAGED_STATE_WRITE_BIT_VERIFY;

    if (!_context.interface->paged_verify(_context.cv_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

void DccServiceModeTaskPaged_initialize(const interface_dcc_service_mode_task_paged_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

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

void DccServiceModeTaskPaged_on_primitive_complete(dcc_service_mode_result_t result) {

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
