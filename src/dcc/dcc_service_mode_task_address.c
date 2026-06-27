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
 * @file dcc_service_mode_task_address.c
 * @brief Task orchestrator for Address-Only mode CV programming (S-9.2.3 §E).
 *
 * @author Jim Kueneman
 * @date 23 Jun 2026
 */

#include "dcc_service_mode_task_address.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

#include <string.h>

#define DCC_TASK_ADDRESS_MAX 127u

typedef enum {

    DCC_TASK_ADDRESS_STATE_IDLE,
    DCC_TASK_ADDRESS_STATE_READ,
    DCC_TASK_ADDRESS_STATE_WRITE,
    DCC_TASK_ADDRESS_STATE_WRITE_VERIFY,
    DCC_TASK_ADDRESS_STATE_READ_BIT_READ,
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_READ,
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_WRITE,
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_VERIFY,
    DCC_TASK_ADDRESS_STATE_VERIFY,

} dcc_task_address_state_enum;

typedef struct {

    const interface_dcc_service_mode_task_address_t *interface;
    dcc_task_address_state_enum state;
    uint8_t bit;
    bool bit_value;
    uint8_t value;
    uint8_t scan_value;
    uint8_t current_step;
    bool ack_result;
    dcc_service_mode_task_on_complete_callback_t on_complete;
    dcc_service_mode_task_on_progress_callback_t on_progress;

} dcc_service_mode_task_address_context_t;

static dcc_service_mode_task_address_context_t _context;

static void _report_progress(dcc_task_phase_enum phase) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, 0);

    }

}

static void _complete(dcc_service_mode_result_t result, uint8_t value) {

    _context.state = DCC_TASK_ADDRESS_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

    }

}

static void _advance_scan(bool to_write_bit) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_READ);

    if (_context.ack_result) {

        if (to_write_bit) {

            uint8_t modified = _context.scan_value;

            if (_context.bit_value) {

                modified |= (uint8_t)(1u << _context.bit);

            } else {

                modified &= (uint8_t)(~(1u << _context.bit));

            }

            _context.value = modified;
            _context.state = DCC_TASK_ADDRESS_STATE_WRITE_BIT_WRITE;
            _context.interface->address_write(modified);

        } else if (_context.state == DCC_TASK_ADDRESS_STATE_READ_BIT_READ) {

            uint8_t bit_result = (_context.scan_value >> _context.bit) & 1u;
            _complete(DCC_SERVICE_MODE_SUCCESS, bit_result);

        } else {

            _complete(DCC_SERVICE_MODE_SUCCESS, _context.scan_value);

        }

    } else if (_context.scan_value == DCC_TASK_ADDRESS_MAX) {

        _complete(DCC_SERVICE_MODE_ERROR, 0);

    } else {

        _context.scan_value++;
        _context.interface->address_verify(_context.scan_value);

    }

}

static void _advance_write(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_ADDRESS_STATE_WRITE_VERIFY;
    _context.interface->address_verify(_context.value);

}

static void _advance_write_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY);

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_ADDRESS_STATE_WRITE_BIT_VERIFY;
    _context.interface->address_verify(_context.value);

}

static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

void DccServiceModeTaskAddress_initialize(const interface_dcc_service_mode_task_address_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

bool DccServiceModeTaskAddress_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_READ;

    _context.interface->address_verify(0);

    return true;

}

bool DccServiceModeTaskAddress_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (address < 1 || address > DCC_TASK_ADDRESS_MAX) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.value        = address;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_WRITE;

    _context.interface->address_write(address);

    return true;

}

static void _advance_verify(void) {

    /* Single CV#1 verify: ACK = the address matched (SUCCESS); otherwise the
     * value did not verify (VERIFY_FAIL). */
    _complete(_context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL,
              _context.value);

}

bool DccServiceModeTaskAddress_verify(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (address < 1 || address > DCC_TASK_ADDRESS_MAX) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.value        = address;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_VERIFY;

    _context.interface->address_verify(address);

    return true;

}

bool DccServiceModeTaskAddress_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit > 6) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.bit          = bit;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_READ_BIT_READ;

    _context.interface->address_verify(0);

    return true;

}

bool DccServiceModeTaskAddress_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit > 6) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.bit          = bit;
    _context.bit_value    = bit_value;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_WRITE_BIT_READ;

    _context.interface->address_verify(0);

    return true;

}

void DccServiceModeTaskAddress_on_primitive_complete(dcc_service_mode_result_t result) {

    /* The common module measures the ACK pulse width internally and reports the
     * outcome here: SUCCESS = valid ACK detected, anything else = no ACK. */
    _context.ack_result = (result == DCC_SERVICE_MODE_SUCCESS);

    switch (_context.state) {

        case DCC_TASK_ADDRESS_STATE_READ:

            _advance_scan(false);
            break;

        case DCC_TASK_ADDRESS_STATE_READ_BIT_READ:

            _advance_scan(false);
            break;

        case DCC_TASK_ADDRESS_STATE_WRITE_BIT_READ:

            _advance_scan(true);
            break;

        case DCC_TASK_ADDRESS_STATE_WRITE:

            _advance_write();
            break;

        case DCC_TASK_ADDRESS_STATE_WRITE_VERIFY:

            _advance_write_verify();
            break;

        case DCC_TASK_ADDRESS_STATE_WRITE_BIT_WRITE:

            _advance_write_bit_write();
            break;

        case DCC_TASK_ADDRESS_STATE_WRITE_BIT_VERIFY:

            _advance_write_bit_verify();
            break;

        case DCC_TASK_ADDRESS_STATE_VERIFY:

            _advance_verify();
            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */
