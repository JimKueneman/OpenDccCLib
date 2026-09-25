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
 * @file dcc_service_mode_address.c
 * @brief Address-only mode programming (CV 1 write and verify).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_address.h"

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

#include <string.h>

    /** @brief Step of the two-step address-only operation (stored in the context address_state field). */
typedef enum {

    DCC_ADDRESS_STATE_IDLE,         /**< No operation in progress */
    DCC_ADDRESS_STATE_PAGE_PRESET,  /**< Writing page 1 to the page register */
    DCC_ADDRESS_STATE_COMMAND       /**< Writing or verifying CV 1 via register 1 */

} address_state_enum;

    /** @brief Context of the operation in flight; the step-callback signature carries no context, and only one operation runs at a time. */
static dcc_service_mode_address_context_t *_active_context = (void *)0;

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
     * DCC_PREAMBLE_BITS_SERVICE preamble, sent once (repeat_count 0). The page-preset writes the
     * page register (register 6); the address command targets CV 1 via the register-1 form (0111C000).
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

    /**
     * @brief Step callback after the register-1 command: finishes the operation.
     *
     * @details Returns to IDLE and forwards the result to interface->on_complete when set.
     *
     * @param result Outcome of the CV 1 write or verify step.
     */
static void _on_command_complete(dcc_service_mode_result_enum result) {

    _active_context->address_state = DCC_ADDRESS_STATE_IDLE;

    if (_active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

    /**
     * @brief Step callback after the page-preset: starts the register-1 command.
     *
     * @details Algorithm:
     * -# Ignore the preset result: per S-9.2.3 the address command (CV 1) follows the page-preset unconditionally
     * -# Move to COMMAND and build the register-1 packet from the latched address and write flag
     * -# Write: start with DCC_SERVICE_MODE_COMMAND_REPEAT command packets and
     *    DCC_SERVICE_MODE_RECOVERY_COUNT_LONG (10) recovery packets
     * -# Verify: start with DCC_SERVICE_MODE_COMMAND_REPEAT command packets and no recovery
     * -# If the common module refuses to start, return to IDLE and report DCC_SERVICE_MODE_BUSY through
     *    on_complete rather than leave the state machine stuck waiting for a callback that never comes
     *
     * @param result Outcome of the page-preset step (not used).
     */
static void _on_preset_complete(dcc_service_mode_result_enum result) {

    dcc_packet_t packet;
    (void)result;
    memset(&packet, 0, sizeof(packet));

    _active_context->address_state = DCC_ADDRESS_STATE_COMMAND;
    _build_register_packet(&packet, 1, _active_context->address, _active_context->is_write);

    bool started;

    if (_active_context->is_write) {

        started = _active_context->interface->begin_operation(&packet, &_on_command_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT_LONG);

    } else {

        started = _active_context->interface->begin_operation(&packet, &_on_command_complete, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    }

    /* begin_operation() refuses to start when the shared context is not idle.
     * Fail cleanly rather than leave this state machine stuck in COMMAND with
     * no completion callback ever arriving (which would also reject every
     * later call, since the state never returns to IDLE). */
    if (!started) {

        _active_context->address_state = DCC_ADDRESS_STATE_IDLE;

        if (_active_context->interface->on_complete) {

            _active_context->interface->on_complete(DCC_SERVICE_MODE_BUSY);

        }

    }

}

    /**
     * @brief Common entry for write and verify: validate, latch the address, start the page-preset.
     *
     * @details Algorithm:
     * -# Return false if address is outside 1-127, the common module is busy, or address_state is not IDLE
     * -# Latch the address and write flag; enter PAGE_PRESET and record this context for the callbacks
     * -# Start a write of DCC_SERVICE_MODE_PAGE_PRESET_PAGE to DCC_SERVICE_MODE_PAGE_REGISTER with
     *    DCC_SERVICE_MODE_COMMAND_REPEAT command packets and DCC_SERVICE_MODE_RECOVERY_COUNT recovery packets;
     *    _on_preset_complete continues with the register-1 command
     * -# If the common module refuses to start, return to IDLE and return false
     *
     * @param context Pointer to the address-only service mode context.
     * @param address Short address (1-127) to write or verify.
     * @param is_write true for write, false for verify.
     *
     * @return true if the page-preset started, false if validation failed or the common module refused.
     */
static bool _begin_with_preset(dcc_service_mode_address_context_t *context, uint8_t address, bool is_write) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (address < 1 || address > 127) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    if (context->address_state != DCC_ADDRESS_STATE_IDLE) {

        return false;

    }

    context->address = address;
    context->is_write = is_write;
    context->address_state = DCC_ADDRESS_STATE_PAGE_PRESET;
    _active_context = context;

    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, DCC_SERVICE_MODE_PAGE_PRESET_PAGE, true);

    if (!context->interface->begin_operation(&packet, &_on_preset_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT)) {

        context->address_state = DCC_ADDRESS_STATE_IDLE;
        return false;

    }

    return true;

}

    /**
     * @brief Initialize the address-only service mode module.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_address_context_t instance.
     * @param interface Pointer to populated interface_dcc_service_mode_address_t struct.
     * @endverbatim
     */
void DccServiceModeAddress_initialize(dcc_service_mode_address_context_t *context, const interface_dcc_service_mode_address_t *interface) {

    context->interface = interface;
    context->address_state = DCC_ADDRESS_STATE_IDLE;

}

    /**
     * @brief Write the short address (CV 1) using address-only mode.
     *
     * @details Delegates to _begin_with_preset() as a write.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_address_context_t instance.
     * @param address The short address to write (1-127).
     * @endverbatim
     *
     * @return true if the operation started, false if address is out of range, the common module is busy, or an address operation is already in progress.
     */
bool DccServiceModeAddress_write(dcc_service_mode_address_context_t *context, uint8_t address) {

    return _begin_with_preset(context, address, true);

}

    /**
     * @brief Verify the short address (CV 1) using address-only mode.
     *
     * @details Delegates to _begin_with_preset() as a verify.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_address_context_t instance.
     * @param address The short address to verify (1-127).
     * @endverbatim
     *
     * @return true if the operation started, false if address is out of range, the common module is busy, or an address operation is already in progress.
     */
bool DccServiceModeAddress_verify(dcc_service_mode_address_context_t *context, uint8_t address) {

    return _begin_with_preset(context, address, false);

}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */
