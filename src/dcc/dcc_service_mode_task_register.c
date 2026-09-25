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
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_task_register.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

#include <string.h>

    /** @brief States of the register mode task state machine. */
typedef enum {

    DCC_TASK_REGISTER_STATE_IDLE,                /**< No operation in progress */
    DCC_TASK_REGISTER_STATE_READ_CV,             /**< read_cv: register_verify of the current scan value outstanding */
    DCC_TASK_REGISTER_STATE_WRITE_CV,            /**< write_cv: register_write outstanding */
    DCC_TASK_REGISTER_STATE_WRITE_CV_VERIFY,     /**< write_cv: register_verify of the written value outstanding */
    DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE,  /**< read_bit: scanning for the full byte */
    DCC_TASK_REGISTER_STATE_WRITE_BIT_READ_BYTE, /**< write_bit: scanning for the current byte before modifying it */
    DCC_TASK_REGISTER_STATE_WRITE_BIT_WRITE,     /**< write_bit: register_write of the modified byte outstanding */
    DCC_TASK_REGISTER_STATE_WRITE_BIT_VERIFY,    /**< write_bit: register_verify of the modified byte outstanding */
    DCC_TASK_REGISTER_STATE_FACTORY_RESET,       /**< factory_reset: register_write of 8 to register 8 outstanding */
    DCC_TASK_REGISTER_STATE_VERIFY_VALUE,        /**< verify_value: single register_verify outstanding */

} dcc_task_register_state_enum;

    /** @brief Singleton context for the register mode task. */
typedef struct {

    const interface_dcc_service_mode_task_register_t *interface; /**< Injected primitive dependencies */
    dcc_task_register_state_enum state;                          /**< Current state machine state */
    uint8_t register_number;                                     /**< Physical register (1-8) the operation in progress addresses */
    uint8_t bit_position;                                        /**< Bit requested by read_bit / write_bit */
    bool bit_value;                                              /**< Bit value requested by write_bit */
    uint8_t value;                                               /**< Byte to write or verify, or the modified byte (write_bit) */
    uint8_t scan_value;                                          /**< Candidate value currently being verified in a 0-255 scan */
    uint8_t current_step;                                        /**< Steps completed so far, reported through on_progress */
    bool ack_result;                                             /**< Outcome of the most recent primitive: true = ACK measured */
    dcc_decoder_type_enum decoder_type;                          /**< Decoder type the CV-to-register mapping was made for */
    dcc_service_mode_task_on_complete_callback_t on_complete;    /**< Completion callback for the operation in progress */
    dcc_service_mode_task_on_progress_callback_t on_progress;    /**< Progress callback (nullable) */

} dcc_service_mode_task_register_context_t;

    /** @brief Singleton task context. */
static dcc_service_mode_task_register_context_t _context;

    /**
     * @brief Maps a CV number to a physical register (1-8) for the given decoder type (S-9.2.3 §E).
     *
     * @details Mobile: CV1-4 -> R1-4, CV29 -> R5, CV7 -> R7, CV8 -> R8. Accessory: CV513 -> R1, CV7 -> R7, CV520 -> R8.
     *
     * @param cv_number CV number to map.
     * @param decoder_type Selects the Mobile or Accessory register map.
     *
     * @return Register number 1-8, or 0 if the CV is not accessible in register mode.
     */
static uint8_t _cv_to_register(uint16_t cv_number, dcc_decoder_type_enum decoder_type) {

    if (decoder_type == DCC_DECODER_TYPE_MOBILE) {

        switch (cv_number) {

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
    switch (cv_number) {

        case 513: return 1;
        case 7:   return 7;
        case 520: return 8;
        default:  return 0;

    }

}

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

    _context.state = DCC_TASK_REGISTER_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

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
    _context.state = DCC_TASK_REGISTER_STATE_WRITE_BIT_WRITE;

    if (!_context.interface->register_write(_context.register_number, modified)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Advances a 0-255 register_verify scan (read_cv, read_bit, or the read phase of write_bit) after one verify completes.
     *
     * @details Algorithm:
     * -# Count the step and report READ progress
     * -# On ACK the scanned value is the register byte: hand off to _begin_write_bit() when to_write_bit is set,
     *    report the requested bit of it when in READ_BIT_READ_BYTE, otherwise report the byte itself
     * -# If value 255 has been tried without an ACK, complete with ERROR (no value acknowledged)
     * -# Otherwise verify the next candidate value, completing with BUSY if the primitive refuses to start
     *
     * @param to_write_bit true to continue into the write_bit read-modify-write once the byte is found.
     */
static void _advance_scan(bool to_write_bit) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_READ);

    if (_context.ack_result) {

        if (to_write_bit) {

            _begin_write_bit();

        } else if (_context.state == DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE) {

            uint8_t bit_result = (_context.scan_value >> _context.bit_position) & 1u;
            _complete(DCC_SERVICE_MODE_SUCCESS, bit_result);

        } else {

            _complete(DCC_SERVICE_MODE_SUCCESS, _context.scan_value);

        }

    } else if (_context.scan_value == 255u) {

        _complete(DCC_SERVICE_MODE_ERROR, 0);

    } else {

        _context.scan_value++;

        if (!_context.interface->register_verify(_context.register_number, _context.scan_value)) {

            _complete(DCC_SERVICE_MODE_BUSY, 0);

        }

    }

}

    /**
     * @brief Advances write_cv after the register_write completes: reports progress and issues the targeted register_verify.
     *
     * @details The ACK outcome of the write itself is not evaluated; the verify that follows decides the
     * result. Completes with BUSY if the register_verify cannot be started.
     */
static void _advance_write_cv(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_REGISTER_STATE_WRITE_CV_VERIFY;

    if (!_context.interface->register_verify(_context.register_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes write_cv after the register_verify.
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

    /** @brief Advances write_bit after the register_write of the modified byte: issues its register_verify, completing with BUSY if it cannot start. */
static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_REGISTER_STATE_WRITE_BIT_VERIFY;

    if (!_context.interface->register_verify(_context.register_number, _context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /** @brief Completes write_bit: SUCCESS if the modified byte verified, VERIFY_FAIL otherwise; value = the bit written (0 or 1). */
static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

    /** @brief Completes factory_reset: SUCCESS if the write was acknowledged, NO_ACK otherwise (an ACK is optional per spec); value 0. */
static void _advance_factory_reset(void) {

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_NO_ACK;
    _complete(result, 0);

}

    /**
     * @brief Initialize the register task module. Call once during DccConfig_initialize().
     *
     * @details Clears the singleton context (state IDLE, no callbacks) and stores the interface pointer.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_service_mode_task_register_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccServiceModeTaskRegister_initialize(const interface_dcc_service_mode_task_register_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

    /**
     * @brief Read a CV byte using register mode. Iterates register_verify 0-255 until ACK.
     *
     * @details Algorithm:
     * -# Map the CV to a register for decoder_type; reject if the CV is not register-accessible
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: register, decoder type, scan value 0, cleared step count, callbacks
     * -# Enter READ_CV and issue register_verify for value 0
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The scan continues from DccServiceModeTaskRegister_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number to read (mapped to register via decoder_type).
     * @param decoder_type Mobile or Accessory - determines CV-to-register mapping.
     * @param on_complete Called when complete: SUCCESS with the CV byte found, ERROR if no value was ACKed, or BUSY.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is not accessible in register mode, or the first verify could not start.
     */
bool DccServiceModeTaskRegister_read_cv(uint16_t cv_number, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    uint8_t register_number = _cv_to_register(cv_number, decoder_type);

    if (register_number == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.register_number          = register_number;
    _context.decoder_type = decoder_type;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_READ_CV;

    if (!_context.interface->register_verify(register_number, 0)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a CV byte then verify using register mode. Always 2 operations.
     *
     * @details Algorithm:
     * -# Map the CV to a register for decoder_type; reject if the CV is not register-accessible
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: register, decoder type, value, cleared step count, callbacks
     * -# Enter WRITE_CV and issue register_write
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The verify runs from DccServiceModeTaskRegister_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number to write (mapped to register via decoder_type).
     * @param value Byte to write.
     * @param decoder_type Mobile or Accessory - determines CV-to-register mapping.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the byte written, or BUSY.
     * @param on_progress Called after each step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is not accessible in register mode, or the write could not start.
     */
bool DccServiceModeTaskRegister_write_cv(uint16_t cv_number, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    uint8_t register_number = _cv_to_register(cv_number, decoder_type);

    if (register_number == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.register_number          = register_number;
    _context.decoder_type = decoder_type;
    _context.value        = value;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_WRITE_CV;

    if (!_context.interface->register_write(register_number, value)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Read a single CV bit using register mode. Reads full byte, extracts bit.
     *
     * @details Algorithm:
     * -# Reject bit positions above 7
     * -# Map the CV to a register for decoder_type; reject if the CV is not register-accessible
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: register, decoder type, bit position, scan value 0, cleared step count, callbacks
     * -# Enter READ_BIT_READ_BYTE and issue register_verify for value 0
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The scan continues from DccServiceModeTaskRegister_on_primitive_complete(); the requested bit is extracted from the value found
     *
     * @verbatim
     * @param cv_number CV number (mapped to register via decoder_type).
     * @param bit_position Bit position (0-7).
     * @param decoder_type Mobile or Accessory - determines CV-to-register mapping.
     * @param on_complete Called when complete: SUCCESS with value 0 or 1, ERROR if no byte value was ACKed, or BUSY.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the bit is above 7, the CV is not register-accessible, or the first verify could not start.
     */
bool DccServiceModeTaskRegister_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit_position > 7) {

        return false;

    }

    uint8_t register_number = _cv_to_register(cv_number, decoder_type);

    if (register_number == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.register_number          = register_number;
    _context.decoder_type = decoder_type;
    _context.bit_position          = bit_position;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_READ_BIT_READ_BYTE;

    if (!_context.interface->register_verify(register_number, 0)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a single CV bit using register mode. Read-modify-write sequence.
     *
     * @details Algorithm:
     * -# Reject bit positions above 7
     * -# Map the CV to a register for decoder_type; reject if the CV is not register-accessible
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: register, decoder type, bit position, bit value, scan value 0, cleared step count, callbacks
     * -# Enter WRITE_BIT_READ_BYTE and issue register_verify for value 0 to start the read scan
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# From DccServiceModeTaskRegister_on_primitive_complete(): once the byte is found, write the modified byte, then verify it
     *
     * @verbatim
     * @param cv_number CV number (mapped to register via decoder_type).
     * @param bit_position Bit position (0-7).
     * @param bit_value Value to write.
     * @param decoder_type Mobile or Accessory - determines CV-to-register mapping.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the bit written, ERROR if the read scan found nothing, or BUSY.
     * @param on_progress Called after each scan step of the read phase only (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the bit is above 7, the CV is not register-accessible, or the first verify could not start.
     */
bool DccServiceModeTaskRegister_write_bit(
            uint16_t cv_number,
            uint8_t bit_position,
            bool bit_value,
            dcc_decoder_type_enum decoder_type,
            dcc_service_mode_task_on_complete_callback_t on_complete,
            dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit_position > 7) {

        return false;

    }

    uint8_t register_number = _cv_to_register(cv_number, decoder_type);

    if (register_number == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.register_number          = register_number;
    _context.decoder_type = decoder_type;
    _context.bit_position          = bit_position;
    _context.bit_value    = bit_value;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_WRITE_BIT_READ_BYTE;

    if (!_context.interface->register_verify(register_number, 0)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

    /** @brief Completes verify_value: SUCCESS if the register_verify was acknowledged, VERIFY_FAIL otherwise; value = the expected byte. */
static void _advance_verify_value(void) {

    /* Single register verify: ACK = the held value matched (SUCCESS); otherwise
     * the value did not verify (VERIFY_FAIL). */
    _complete(_context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL, _context.value);

}

    /**
     * @brief Verify a single register value (one register-verify op).
     *
     * @details Algorithm:
     * -# Map the CV to a register for decoder_type; reject if the CV is not register-accessible
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: register, value, decoder type, cleared step count, callbacks
     * -# Enter VERIFY_VALUE and issue register_verify for the expected value
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The result is reported from DccServiceModeTaskRegister_on_primitive_complete()
     *
     * @verbatim
     * @param cv_number CV number (mapped to a physical register for decoder_type).
     * @param value Expected byte value.
     * @param decoder_type Mobile or Accessory (selects the CV->register map).
     * @param on_complete Called with SUCCESS if the value verified (ACK), VERIFY_FAIL otherwise.
     * @param on_progress Not used by this operation (nullable).
     * @endverbatim
     *
     * @return true if started; false if another operation is running, the CV is not register-accessible, or the verify could not start.
     */
bool DccServiceModeTaskRegister_verify_value(uint16_t cv_number, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    uint8_t register_number = _cv_to_register(cv_number, decoder_type);

    if (register_number == 0) {

        return false;

    }

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.register_number          = register_number;
    _context.value        = value;
    _context.decoder_type = decoder_type;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_REGISTER_STATE_VERIFY_VALUE;

    if (!_context.interface->register_verify(register_number, value)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Issue a factory reset (write 8 to register 8). Applies to both Mobile and Accessory.
     *
     * @details Algorithm:
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: cleared step count, completion callback, no progress callback
     * -# Enter FACTORY_RESET and issue register_write(8, 8)
     * -# If the primitive refuses to start, return to IDLE and report failure to the caller
     * -# The result (SUCCESS on ACK, otherwise NO_ACK) is reported from DccServiceModeTaskRegister_on_primitive_complete()
     *
     * @verbatim
     * @param on_complete Called when the write completes: SUCCESS if ACKed, NO_ACK otherwise; value 0.
     * @endverbatim
     *
     * @return true if started; false if another operation is running or the write could not start.
     */
bool DccServiceModeTaskRegister_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete) {

    if (_context.state != DCC_TASK_REGISTER_STATE_IDLE) {

        return false;

    }

    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = NULL;
    _context.state        = DCC_TASK_REGISTER_STATE_FACTORY_RESET;

    if (!_context.interface->register_write(8, 8)) {

        _context.state = DCC_TASK_REGISTER_STATE_IDLE;
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
void DccServiceModeTaskRegister_on_primitive_complete(dcc_service_mode_result_enum result) {

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

        case DCC_TASK_REGISTER_STATE_VERIFY_VALUE:

            _advance_verify_value();
            break;

        default:

            break;

    }

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */
