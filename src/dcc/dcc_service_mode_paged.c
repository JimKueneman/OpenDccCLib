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
 * @file dcc_service_mode_paged.c
 * @brief Paged mode CV programming via register 6 page pointer.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_paged.h"

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

#include <string.h>

// =============================================================================
// Internal types
// =============================================================================

    /** @brief Step of the two-step paged operation (stored in the context paged_state field). */
typedef enum {

    DCC_PAGED_STATE_IDLE,           /**< No operation in progress */
    DCC_PAGED_STATE_PAGE_SELECT,    /**< Writing the page number to the page register */
    DCC_PAGED_STATE_DATA_ACCESS     /**< Writing or verifying the data register within the page */

} paged_state_enum;

    /** @brief Context of the operation in flight; the step-callback signature carries no context, and only one operation runs at a time. */
static dcc_service_mode_paged_context_t *_active_context = (void *)0;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Append the XOR error-detection byte to a packet.
     *
     * @details XORs data[0..byte_count-1] into data[byte_count] and increments byte_count.
     *
     * @param packet Pointer to the packet being built; byte_count must be the payload length.
     */
static void _append_xor(dcc_packet_t *packet) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count; byte_index++) {

        xor_byte ^= packet->data[byte_index];

    }

    packet->data[packet->byte_count] = xor_byte;
    packet->byte_count++;

}

    /**
     * @brief Build a register-mode packet (S-9.2.3 register form).
     *
     * @details data[0] = prefix | (register_number - 1), data[1] = value, then the XOR byte;
     * DCC_PREAMBLE_BITS_SERVICE preamble, sent once (repeat_count 0).
     *
     * @param packet Pointer to the packet to fill.
     * @param register_number Register number (1-8), encoded 0-based on the wire.
     * @param value Byte value to write or verify.
     * @param write true selects DCC_SERVICE_REGISTER_WRITE_PREFIX, false DCC_SERVICE_REGISTER_VERIFY_PREFIX.
     */
static void _build_register_packet(dcc_packet_t *packet, uint8_t register_number, uint8_t value, bool write) {

    uint8_t prefix = write ? DCC_SERVICE_REGISTER_WRITE_PREFIX : DCC_SERVICE_REGISTER_VERIFY_PREFIX;

    packet->data[0] = prefix | ((register_number - 1) & 0x07);
    packet->data[1] = value;
    packet->byte_count = 2;
    _append_xor(packet);
    packet->preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet->repeat_count = 0;

}

    /* Forward declarations for callback chain */
static void _on_data_access_complete(dcc_service_mode_result_enum result);

    /**
     * @brief Step callback after the page-select write: starts the data step.
     *
     * @details Algorithm:
     * -# Ignore the page-select result: per S-9.2.3 the data step follows unconditionally
     *    (the page write completes by ACK or by packet count, and ACK is optional)
     * -# Move to DATA_ACCESS and build the data-register packet from the latched register, value and write flag
     * -# Start the data step with DCC_SERVICE_MODE_COMMAND_REPEAT command packets (recovery packets apply to writes only)
     * -# If the common module refuses to start, return to IDLE and report DCC_SERVICE_MODE_BUSY through
     *    on_complete rather than leave the state machine stuck waiting for a callback that never comes
     *
     * @param result Outcome of the page-select step (not used).
     */
static void _on_page_select_complete(dcc_service_mode_result_enum result) {

    dcc_packet_t packet;
    (void)result;
    memset(&packet, 0, sizeof(packet));

    /* Page-select complete. Per S-9.2.3 the data step follows unconditionally:
     * the page-write phase completes either by ACK or by sending its packet
     * count, and ACK is optional (some decoders never assert one), so the page
     * select's own ACK result is not required to proceed. */
    _active_context->paged_state = DCC_PAGED_STATE_DATA_ACCESS;
    _build_register_packet(&packet, _active_context->data_register, _active_context->data_value, _active_context->is_write);

    /* begin_operation() refuses to start when the shared context is not idle.
     * Fail cleanly rather than leave paged_state stuck in DATA_ACCESS with no
     * completion callback ever arriving (which would also reject every later
     * paged call, since the state never returns to IDLE). */
    if (!_active_context->interface->begin_operation(&packet, &_on_data_access_complete, _active_context->is_write, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT)) {

        _active_context->paged_state = DCC_PAGED_STATE_IDLE;

        if (_active_context->interface->on_complete) {

            _active_context->interface->on_complete(DCC_SERVICE_MODE_BUSY);

        }

    }

}

    /**
     * @brief Step callback after the data step: finishes the paged operation.
     *
     * @details Returns to IDLE and forwards the result to interface->on_complete when set.
     *
     * @param result Outcome of the data step.
     */
static void _on_data_access_complete(dcc_service_mode_result_enum result) {

    _active_context->paged_state = DCC_PAGED_STATE_IDLE;

    if (_active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the paged service mode module.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_paged_context_t instance.
     * @param interface Pointer to populated interface_dcc_service_mode_paged_t struct.
     * @endverbatim
     */
void DccServiceModePaged_initialize(dcc_service_mode_paged_context_t *context, const interface_dcc_service_mode_paged_t *interface) {

    context->interface = interface;
    context->paged_state = DCC_PAGED_STATE_IDLE;

}

    /**
     * @brief Write a CV value using paged mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024, the common module is busy, or paged_state is not IDLE
     * -# Map the 0-based CV: page = ((cv_number - 1) / 4) + 1, data register = ((cv_number - 1) % 4) + 1
     * -# Latch the data register, value and write flag; enter PAGE_SELECT and record this context for the callbacks
     * -# Start a write of the page number to DCC_SERVICE_MODE_PAGE_REGISTER with DCC_SERVICE_MODE_COMMAND_REPEAT
     *    command packets and DCC_SERVICE_MODE_RECOVERY_COUNT recovery packets; _on_page_select_complete continues
     * -# If the common module refuses to start, return to IDLE and return false
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_paged_context_t instance.
     * @param cv_number CV number to write (1-1024).
     * @param value Byte value to write.
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number is out of range, the common module is busy, or a paged operation is already in progress.
     */
bool DccServiceModePaged_write(dcc_service_mode_paged_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint8_t page;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    if (context->paged_state != DCC_PAGED_STATE_IDLE) {

        return false;

    }

    page = (uint8_t)(((cv_number - 1) / 4) + 1);
    context->data_register = (uint8_t)(((cv_number - 1) % 4) + 1);
    context->data_value = value;
    context->is_write = true;

    context->paged_state = DCC_PAGED_STATE_PAGE_SELECT;
    _active_context = context;
    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, page, true);

    if (!context->interface->begin_operation(&packet, &_on_page_select_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT)) {

        context->paged_state = DCC_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Verify a CV value using paged mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024, the common module is busy, or paged_state is not IDLE
     * -# Map the 0-based CV: page = ((cv_number - 1) / 4) + 1, data register = ((cv_number - 1) % 4) + 1
     * -# Latch the data register, expected value and verify flag; enter PAGE_SELECT and record this context for the callbacks
     * -# Start a write of the page number to DCC_SERVICE_MODE_PAGE_REGISTER (the page select is always a write)
     *    with DCC_SERVICE_MODE_COMMAND_REPEAT command packets and DCC_SERVICE_MODE_RECOVERY_COUNT recovery packets;
     *    _on_page_select_complete continues with the verify
     * -# If the common module refuses to start, return to IDLE and return false
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_paged_context_t instance.
     * @param cv_number CV number to verify (1-1024).
     * @param value Expected byte value.
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number is out of range, the common module is busy, or a paged operation is already in progress.
     */
bool DccServiceModePaged_verify(dcc_service_mode_paged_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint8_t page;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    if (context->paged_state != DCC_PAGED_STATE_IDLE) {

        return false;

    }

    page = (uint8_t)(((cv_number - 1) / 4) + 1);
    context->data_register = (uint8_t)(((cv_number - 1) % 4) + 1);
    context->data_value = value;
    context->is_write = false;

    context->paged_state = DCC_PAGED_STATE_PAGE_SELECT;
    _active_context = context;
    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, page, true);

    if (!context->interface->begin_operation(&packet, &_on_page_select_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT)) {

        context->paged_state = DCC_PAGED_STATE_IDLE;
        return false;

    }

    return true;

}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */
