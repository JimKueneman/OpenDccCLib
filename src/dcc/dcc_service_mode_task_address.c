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
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_task_address.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

#include <string.h>

    /** @brief Highest short address (7-bit CV#1) the scan and write accept. */
#define DCC_TASK_ADDRESS_MAX 127u

    /** @brief States of the address-only mode task state machine. */
typedef enum {

    DCC_TASK_ADDRESS_STATE_IDLE,             /**< No operation in progress */
    DCC_TASK_ADDRESS_STATE_READ,             /**< read: address_verify of the current scan value outstanding */
    DCC_TASK_ADDRESS_STATE_WRITE,            /**< write: address_write outstanding */
    DCC_TASK_ADDRESS_STATE_WRITE_VERIFY,     /**< write: address_verify of the written address outstanding */
    DCC_TASK_ADDRESS_STATE_READ_BIT_READ,    /**< read_bit: scanning for the full address */
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_READ,   /**< write_bit: scanning for the current address before modifying it */
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_WRITE,  /**< write_bit: address_write of the modified address outstanding */
    DCC_TASK_ADDRESS_STATE_WRITE_BIT_VERIFY, /**< write_bit: address_verify of the modified address outstanding */
    DCC_TASK_ADDRESS_STATE_VERIFY,           /**< verify: single address_verify outstanding */

} dcc_task_address_state_enum;

    /** @brief Singleton context for the address-only mode task. */
typedef struct {

    const interface_dcc_service_mode_task_address_t *interface; /**< Injected primitive dependencies */
    dcc_task_address_state_enum state;                          /**< Current state machine state */
    uint8_t bit_position;                                       /**< Bit requested by read_bit / write_bit */
    bool bit_value;                                             /**< Bit value requested by write_bit */
    uint8_t value;                                              /**< Address to write or verify, or the modified address (write_bit) */
    uint8_t scan_value;                                         /**< Candidate address currently being verified in a 0-127 scan */
    uint8_t current_step;                                       /**< Steps completed so far, reported through on_progress */
    bool ack_result;                                            /**< Outcome of the most recent primitive: true = ACK measured */
    dcc_service_mode_task_on_complete_callback_t on_complete;   /**< Completion callback for the operation in progress */
    dcc_service_mode_task_on_progress_callback_t on_progress;   /**< Progress callback (nullable) */

} dcc_service_mode_task_address_context_t;

    /** @brief Singleton task context. */
static dcc_service_mode_task_address_context_t _context;

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
     * @param value Value delivered with the result (address, bit value, or 0).
     */
static void _complete(dcc_service_mode_result_enum result, uint8_t value) {

    _context.state = DCC_TASK_ADDRESS_STATE_IDLE;

    if (_context.on_complete) {

        _context.on_complete(result, value);

    }

}

    /** @brief write_bit: apply the requested bit to the scanned value and start the write of the modified address. A refused start completes the operation with BUSY. */
static void _begin_write_bit(void) {

    uint8_t modified = _context.scan_value;

    if (_context.bit_value) {

        modified |= (uint8_t)(1u << _context.bit_position);

    } else {

        modified &= (uint8_t)(~(1u << _context.bit_position));

    }

    _context.value = modified;
    _context.state = DCC_TASK_ADDRESS_STATE_WRITE_BIT_WRITE;
    if (!_context.interface->address_write(modified)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Advances a 0-127 address_verify scan (read, read_bit, or the read phase of write_bit) after one verify completes.
     *
     * @details Algorithm:
     * -# Count the step and report READ progress
     * -# On ACK the scanned value is the address: hand off to _begin_write_bit() when to_write_bit is set,
     *    report the requested bit of it when in READ_BIT_READ, otherwise report the address itself
     * -# If address 127 has been tried without an ACK, complete with ERROR (no address acknowledged)
     * -# Otherwise verify the next candidate address; a refused start completes the operation with BUSY
     *
     * @param to_write_bit true to continue into the write_bit read-modify-write once the address is found.
     */
static void _advance_scan(bool to_write_bit) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_READ);

    if (_context.ack_result) {

        if (to_write_bit) {

            _begin_write_bit();

        } else if (_context.state == DCC_TASK_ADDRESS_STATE_READ_BIT_READ) {

            uint8_t bit_result = (_context.scan_value >> _context.bit_position) & 1u;
            _complete(DCC_SERVICE_MODE_SUCCESS, bit_result);

        } else {

            _complete(DCC_SERVICE_MODE_SUCCESS, _context.scan_value);

        }

    } else if (_context.scan_value == DCC_TASK_ADDRESS_MAX) {

        _complete(DCC_SERVICE_MODE_ERROR, 0);

    } else {

        _context.scan_value++;
        if (!_context.interface->address_verify(_context.scan_value)) {

            _complete(DCC_SERVICE_MODE_BUSY, 0);

        }

    }

}

    /**
     * @brief Advances write after the address_write completes: reports progress and issues the address_verify.
     *
     * @details The ACK outcome of the write itself is not evaluated; the verify that follows decides the
     * result. A refused start completes the operation with BUSY.
     */
static void _advance_write(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_WRITE);
    _context.state = DCC_TASK_ADDRESS_STATE_WRITE_VERIFY;
    if (!_context.interface->address_verify(_context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /**
     * @brief Completes write after the address_verify.
     *
     * @details Reports the final progress step, then SUCCESS if the verify was acknowledged or VERIFY_FAIL
     * if not. The value delivered is the address that was written.
     */
static void _advance_write_verify(void) {

    _context.current_step++;
    _report_progress(DCC_TASK_PHASE_VERIFY);

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    _complete(result, _context.value);

}

    /** @brief Advances write_bit after the address_write of the modified address: issues its address_verify (a refused start puts the task back to IDLE and returns false). */
static void _advance_write_bit_write(void) {

    _context.state = DCC_TASK_ADDRESS_STATE_WRITE_BIT_VERIFY;
    if (!_context.interface->address_verify(_context.value)) {

        _complete(DCC_SERVICE_MODE_BUSY, 0);

    }

}

    /** @brief Completes write_bit: SUCCESS if the modified address verified, VERIFY_FAIL otherwise; value = the bit written (0 or 1). */
static void _advance_write_bit_verify(void) {

    dcc_service_mode_result_enum result = _context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL;
    uint8_t bit_result = _context.bit_value ? 1u : 0u;
    _complete(result, bit_result);

}

    /**
     * @brief Initialize the address task module. Call once during DccConfig_initialize().
     *
     * @details Clears the singleton context (state IDLE, no callbacks) and stores the interface pointer.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_service_mode_task_address_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccServiceModeTaskAddress_initialize(const interface_dcc_service_mode_task_address_t *interface) {

    memset(&_context, 0, sizeof(_context));
    _context.interface = interface;

}

    /**
     * @brief Read CV#1 (short address) using address-only mode. Iterates address_verify 0-127 until ACK.
     *
     * @details Algorithm:
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: scan value 0, cleared step count, callbacks
     * -# Enter READ and issue address_verify for address 0 (a refused start puts the task back to IDLE and returns false)
     * -# The scan continues from DccServiceModeTaskAddress_on_primitive_complete()
     *
     * @verbatim
     * @param on_complete Called when complete: SUCCESS with the address found (0-127), or ERROR if no address was ACKed.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started, false if another operation is running.
     */
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

    if (!_context.interface->address_verify(0)) {

        _context.state = DCC_TASK_ADDRESS_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write CV#1 (short address) then verify. Always 2 operations (write + verify).
     *
     * @details Algorithm:
     * -# Reject addresses outside 1-127
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: address, cleared step count, callbacks
     * -# Enter WRITE and issue address_write (a refused start puts the task back to IDLE and returns false)
     * -# The verify runs from DccServiceModeTaskAddress_on_primitive_complete()
     *
     * @verbatim
     * @param address Address to write (1-127).
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the address written.
     * @param on_progress Called after each step (nullable).
     * @endverbatim
     *
     * @return true if started, false if another operation is running or the address is out of range.
     */
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

    if (!_context.interface->address_write(address)) {

        _context.state = DCC_TASK_ADDRESS_STATE_IDLE;
        return false;

    }

    return true;

}

    /** @brief Completes verify: SUCCESS if the address_verify was acknowledged, VERIFY_FAIL otherwise; value = the expected address. */
static void _advance_verify(void) {

    /* Single CV#1 verify: ACK = the address matched (SUCCESS); otherwise the
     * value did not verify (VERIFY_FAIL). */
    _complete(_context.ack_result ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_VERIFY_FAIL, _context.value);

}

    /**
     * @brief Verify CV#1 (short address) against a value (one verify op).
     *
     * @details Algorithm:
     * -# Reject addresses outside 1-127
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: address, cleared step count, callbacks
     * -# Enter VERIFY and issue address_verify for the expected address (a refused start puts the task back to IDLE and returns false)
     * -# The result is reported from DccServiceModeTaskAddress_on_primitive_complete()
     *
     * @verbatim
     * @param address Expected address value (1-127).
     * @param on_complete Called with SUCCESS if it verified (ACK), VERIFY_FAIL otherwise.
     * @param on_progress Not used by this operation (nullable).
     * @endverbatim
     *
     * @return true if started, false if another operation is running or the address is out of range.
     */
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

    if (!_context.interface->address_verify(address)) {

        _context.state = DCC_TASK_ADDRESS_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Read a single bit from CV#1. Reads full byte via iteration, extracts bit.
     *
     * @details Algorithm:
     * -# Reject bit positions above 6
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: bit position, scan value 0, cleared step count, callbacks
     * -# Enter READ_BIT_READ and issue address_verify for address 0 (a refused start puts the task back to IDLE and returns false)
     * -# The scan continues from DccServiceModeTaskAddress_on_primitive_complete(); the requested bit is extracted from the address found
     *
     * @verbatim
     * @param bit_position Bit position (0-6).
     * @param on_complete Called when complete: SUCCESS with value 0 or 1, or ERROR if no address was ACKed.
     * @param on_progress Called after each scan step (nullable).
     * @endverbatim
     *
     * @return true if started, false if another operation is running or the bit is out of range.
     */
bool DccServiceModeTaskAddress_read_bit(uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit_position > 6) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.bit_position          = bit_position;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_READ_BIT_READ;

    if (!_context.interface->address_verify(0)) {

        _context.state = DCC_TASK_ADDRESS_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Write a single bit to CV#1 using read-modify-write. Reads byte first, then writes modified byte.
     *
     * @details Algorithm:
     * -# Reject bit positions above 6
     * -# Reject if another operation is in progress (state not IDLE)
     * -# Load the context: bit position, bit value, scan value 0, cleared step count, callbacks
     * -# Enter WRITE_BIT_READ and issue address_verify for address 0 to start the read scan (a refused start puts the task back to IDLE and returns false)
     * -# From DccServiceModeTaskAddress_on_primitive_complete(): once the address is found, write the modified address, then verify it
     *
     * @verbatim
     * @param bit_position Bit position (0-6).
     * @param bit_value Value to write.
     * @param on_complete Called when complete: SUCCESS or VERIFY_FAIL with the bit written, or ERROR if the read scan found nothing.
     * @param on_progress Called after each scan step of the read phase only (nullable).
     * @endverbatim
     *
     * @return true if started, false if another operation is running or the bit is out of range.
     */
bool DccServiceModeTaskAddress_write_bit(uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (bit_position > 6) {

        return false;

    }

    if (_context.state != DCC_TASK_ADDRESS_STATE_IDLE) {

        return false;

    }

    _context.bit_position          = bit_position;
    _context.bit_value    = bit_value;
    _context.scan_value   = 0;
    _context.current_step = 0;
    _context.ack_result   = false;
    _context.on_complete  = on_complete;
    _context.on_progress  = on_progress;
    _context.state        = DCC_TASK_ADDRESS_STATE_WRITE_BIT_READ;

    if (!_context.interface->address_verify(0)) {

        _context.state = DCC_TASK_ADDRESS_STATE_IDLE;
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
void DccServiceModeTaskAddress_on_primitive_complete(dcc_service_mode_result_enum result) {

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
