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
 * @file dcc_service_mode_direct.c
 * @brief Direct mode CV programming (byte write/verify, bit write/verify).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_direct.h"

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

#include <string.h>

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

static void _on_step_complete(dcc_service_mode_result_enum result);

    /** @brief Context of the operation in flight; the step-callback signature carries no context, and only one operation runs at a time. */
static dcc_service_mode_direct_context_t *_active_context = (void *)0;

    /**
     * @brief Step callback from the common module: forwards the result to the user.
     *
     * @details Calls interface->on_complete of the active context when both are set.
     *
     * @param result Outcome of the single operation step.
     */
static void _on_step_complete(dcc_service_mode_result_enum result) {

    if (_active_context && _active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

    /**
     * @brief Initialize the direct service mode module.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_direct_context_t instance.
     * @param interface Pointer to populated interface_dcc_service_mode_direct_t struct.
     * @endverbatim
     */
void DccServiceModeDirect_initialize(dcc_service_mode_direct_context_t *context, const interface_dcc_service_mode_direct_t *interface) {

    context->interface = interface;

}

    /**
     * @brief Write a byte to a CV using direct mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024 or the common module is busy
     * -# Encode the 0-based wire CV (cv_number - 1): data[0] = DCC_SERVICE_DIRECT_WRITE_PREFIX | CV bits 9-8,
     *    data[1] = CV bits 7-0, data[2] = value, then the XOR byte
     * -# Latch this context for the callback and start a write operation with
     *    DCC_SERVICE_MODE_COMMAND_REPEAT command packets and DCC_SERVICE_MODE_RECOVERY_COUNT recovery packets
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to write (1-1024).
     * @param value Byte value to write.
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number is out of range or the common module is busy.
     */
bool DccServiceModeDirect_write_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_WRITE_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = value;
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

    /**
     * @brief Verify a CV byte value using direct mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024 or the common module is busy
     * -# Encode the 0-based wire CV (cv_number - 1): data[0] = DCC_SERVICE_DIRECT_VERIFY_PREFIX | CV bits 9-8,
     *    data[1] = CV bits 7-0, data[2] = value, then the XOR byte
     * -# Latch this context for the callback and start a verify operation with
     *    DCC_SERVICE_MODE_COMMAND_REPEAT command packets and no recovery packets
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to verify (1-1024).
     * @param value Expected byte value.
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number is out of range or the common module is busy.
     */
bool DccServiceModeDirect_verify_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_VERIFY_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = value;
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

}

    /**
     * @brief Write a single bit to a CV using direct mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024, bit_position is above 7, or the common module is busy
     * -# Encode the 0-based wire CV (cv_number - 1): data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | CV bits 9-8,
     *    data[1] = CV bits 7-0, data[2] = 1111DBBB (D = bit_value, BBB = bit_position), then the XOR byte
     * -# Latch this context for the callback and start a write operation with
     *    DCC_SERVICE_MODE_COMMAND_REPEAT command packets and DCC_SERVICE_MODE_RECOVERY_COUNT recovery packets
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to write (1-1024).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Value to write (true = 1, false = 0).
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number or bit_position is out of range or the common module is busy.
     */
bool DccServiceModeDirect_write_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = 0xF0 | (bit_value ? 0x08 : 0x00) | (bit_position & 0x07);
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

    /**
     * @brief Verify a single bit in a CV using direct mode.
     *
     * @details Algorithm:
     * -# Return false if cv_number is outside 1-1024, bit_position is above 7, or the common module is busy
     * -# Encode the 0-based wire CV (cv_number - 1): data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | CV bits 9-8,
     *    data[1] = CV bits 7-0, data[2] = 1110DBBB (D = bit_value, BBB = bit_position), then the XOR byte
     * -# Latch this context for the callback and start a verify operation with
     *    DCC_SERVICE_MODE_COMMAND_REPEAT command packets and no recovery packets
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to verify (1-1024).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Expected bit value (true = 1, false = 0).
     * @endverbatim
     *
     * @return true if the operation started, false if cv_number or bit_position is out of range or the common module is busy.
     */
bool DccServiceModeDirect_verify_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024) {

        return false;

    }

    if (bit_position > 7) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = 0xE0 | (bit_value ? 0x08 : 0x00) | (bit_position & 0x07);
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */
