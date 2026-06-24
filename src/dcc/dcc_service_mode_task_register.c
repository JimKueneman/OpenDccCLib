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
 * @file dcc_service_mode_task_register.c
 * @brief Task orchestrator for Register mode CV programming (S-9.2.3 §E).
 *
 * @author Jim Kueneman
 * @date 23 Jun 2026
 */

#include "dcc_service_mode_task_register.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

#include <string.h>

typedef enum {

    DCC_TASK_REGISTER_STATE_IDLE,
    DCC_TASK_REGISTER_STATE_READ_CV,
    DCC_TASK_REGISTER_STATE_WRITE_CV,
    DCC_TASK_REGISTER_STATE_WRITE_CV_VERIFY,
    DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE,
    DCC_TASK_REGISTER_STATE_WRITE_BIT_READ_BYTE,
    DCC_TASK_REGISTER_STATE_WRITE_BIT_WRITE,
    DCC_TASK_REGISTER_STATE_WRITE_BIT_VERIFY,
    DCC_TASK_REGISTER_STATE_FACTORY_RESET,

} dcc_task_register_state_enum;

typedef struct {

    const interface_dcc_service_mode_task_register_t *interface;
    dcc_task_register_state_enum state;
    uint8_t reg;
    uint8_t bit;
    bool bit_value;
    uint8_t value;
    uint8_t scan_value;
    uint8_t current_step;
    bool ack_result;
    dcc_decoder_type_enum decoder_type;
    dcc_service_mode_task_on_complete_callback_t on_complete;
    dcc_service_mode_task_on_progress_callback_t on_progress;

} dcc_service_mode_task_register_context_t;

static dcc_service_mode_task_register_context_t _context;

/* Map a CV number to a physical register (1-8) for the given decoder type.
 * Returns 0 if the CV is not accessible in register mode (S-9.2.3 §E). */
static uint8_t _cv_to_register(uint16_t cv, dcc_decoder_type_enum decoder_type) {

    if (decoder_type == DCC_DECODER_TYPE_MOBILE) {

        switch (cv) {

            case 1:  return 1;
            case 2:  return 2;
            case 3:  return 3;
            case 4:  return 4;
            case 29: return 5;
            case 7:  return 7;
            case 8:  return 8;
            default: return 0;

        }

    }

    /* DCC_DECODER_TYPE_ACCESSORY */
    switch (cv) {

        case 513: return 1;
        case 7:   return 7;
        case 520: return 8;
        default:  return 0;

    }

}

static void _report_progress(dcc_task_phase_enum phase) {

    if (_context.on_progress) {

        _context.on_progress(phase, _context.current_step, 0);

    }

}

static void _complete(dcc_service_mode_result_t result, uint8_t value) {

    _context.state = DCC_TASK_REGISTER_STATE_IDLE;

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
            _context.state = DCC_TASK_REGISTER_STATE_WRITE_BIT_WRITE;
            _context.interface->register_write(_context.reg, modified);

        } else if (_context.state == DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE) {

            uint8_t bit_result = (_context.scan_value >> _context.bit) & 1u;
            _complete(DCC_SERVICE_MODE_SUCCESS, bit_result);

        } else {

            _complete(DCC_SERVICE_MODE_SUCCESS, _context.scan_value);

        }

    } else if (_context.scan_value == 255u) {

        _complete(DCC_SERVICE_MODE_ERROR, 0);

    } else {

        _context.scan_value++;
        _context.interface->register_verify(_context.reg, _context.scan_value);

    }

}

static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_REGISTER_STATE_WRITE_CV_VERIFY;
    _context.interface->register_verify(_context.reg, _context.value);

}

static void _advance_write_cv_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY);

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_REGISTER_STATE_WRITE_BIT_VERIFY;
    _context.interface->register_verify(_context.reg, _context.value);

}

static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

static void _advance_factory_reset(void) {

    dcc_service_mode_result_t result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_NO_ACK;
    _complete(result, 0);

}

void DccServiceModeTaskRegister_initialize(const interface_dcc_service_mode_task_register_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

bool DccServiceModeTaskRegister_read_cv(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    uint8_t reg = _cv_to_register(cv, decoder_type);

    if (reg == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.reg          = reg;
    _context.decoder_type = decoder_type;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_READ_CV;

    _context.interface->register_verify(reg, 0);

    return true;

}

bool DccServiceModeTaskRegister_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    uint8_t reg = _cv_to_register(cv, decoder_type);

    if (reg == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.reg          = reg;
    _context.decoder_type = decoder_type;
    _context.value        = value;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_WRITE_CV;

    _context.interface->register_write(reg, value);

    return true;

}

bool DccServiceModeTaskRegister_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit > 7) {

        return false;

    }

    uint8_t reg = _cv_to_register(cv, decoder_type);

    if (reg == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.reg          = reg;
    _context.decoder_type = decoder_type;
    _context.bit          = bit;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE;

    _context.interface->register_verify(reg, 0);

    return true;

}

bool DccServiceModeTaskRegister_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit > 7) {

        return false;

    }

    uint8_t reg = _cv_to_register(cv, decoder_type);

    if (reg == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.reg          = reg;
    _context.decoder_type = decoder_type;
    _context.bit          = bit;
    _context.bit_value    = bit_value;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_WRITE_BIT_READ_BYTE;

    _context.interface->register_verify(reg, 0);

    return true;

}

bool DccServiceModeTaskRegister_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete) {

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = NULL;
    _context.state        = DCC_TASK_REGISTER_STATE_FACTORY_RESET;

    _context.interface->register_write(8, 8);

    return true;

}

void DccServiceModeTaskRegister_on_primitive_complete(dcc_service_mode_result_t result) {

    /* The common module measures the ACK pulse width internally and reports the
     * outcome here: SUCCESS = valid ACK detected, anything else = no ACK. */
    _context.ack_result = (result == DCC_SERVICE_MODE_SUCCESS);

    switch (_context.state) {

        case DCC_TASK_REGISTER_STATE_READ_CV:

            _advance_scan(false);
            break;

        case DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE:

            _advance_scan(false);
            break;

        case DCC_TASK_REGISTER_STATE_WRITE_BIT_READ_BYTE:

            _advance_scan(true);
            break;

        case DCC_TASK_REGISTER_STATE_WRITE_CV:

            _advance_write_cv();
            break;

        case DCC_TASK_REGISTER_STATE_WRITE_CV_VERIFY:

            _advance_write_cv_verify();
            break;

        case DCC_TASK_REGISTER_STATE_WRITE_BIT_WRITE:

            _advance_write_bit_write();
            break;

        case DCC_TASK_REGISTER_STATE_WRITE_BIT_VERIFY:

            _advance_write_bit_verify();
            break;

        case DCC_TASK_REGISTER_STATE_FACTORY_RESET:

            _advance_factory_reset();
            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */
