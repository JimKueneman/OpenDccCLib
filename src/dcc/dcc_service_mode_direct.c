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
 * @date 07 Apr 2026
 */

#include "dcc_service_mode_direct.h"

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

static void _append_xor(dcc_packet_t *packet) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count; byte_index++) {

        xor_byte ^= packet->data[byte_index];

    }

    packet->data[packet->byte_count] = xor_byte;
    packet->byte_count++;

}

static void _on_step_complete(dcc_service_mode_result_t result);

/* We need the context to reach interface->on_complete in the callback, but the callback
 * signature is fixed (takes only result). Store a module-level pointer to the
 * active context. This is safe because only one operation runs at a time. */
static dcc_service_mode_direct_context_t *_active_ctx = (void *)0;

static void _on_step_complete(dcc_service_mode_result_t result) {

    if (_active_ctx && _active_ctx->interface->on_complete) {

        _active_ctx->interface->on_complete(result);

    }

}

void DccServiceModeDirect_initialize(dcc_service_mode_direct_context_t *context, const interface_dcc_service_mode_direct_t *interface) {

    context->interface = interface;

}

bool DccServiceModeDirect_write_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024)
        return false;

    if (!context->interface->is_common_idle())
        return false;

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_WRITE_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = value;
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_ctx = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, true, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

bool DccServiceModeDirect_verify_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value) {

    dcc_packet_t packet;
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024)
        return false;

    if (!context->interface->is_common_idle())
        return false;

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_VERIFY_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = value;
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_ctx = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, false, 0);

}

bool DccServiceModeDirect_write_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    dcc_packet_t packet;
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024)
        return false;

    if (bit_position > 7)
        return false;

    if (!context->interface->is_common_idle())
        return false;

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = 0xF0 | (bit_value ? 0x08 : 0x00) | (bit_position & 0x07);
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_ctx = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, true, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

bool DccServiceModeDirect_verify_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    dcc_packet_t packet;
    uint16_t wire_cv;

    if (cv_number < 1 || cv_number > 1024)
        return false;

    if (bit_position > 7)
        return false;

    if (!context->interface->is_common_idle())
        return false;

    wire_cv = cv_number - 1;

    packet.data[0] = DCC_SERVICE_DIRECT_BIT_PREFIX | (uint8_t)((wire_cv >> 8) & 0x03);
    packet.data[1] = (uint8_t)(wire_cv & 0xFF);
    packet.data[2] = 0xE0 | (bit_value ? 0x08 : 0x00) | (bit_position & 0x07);
    packet.byte_count = 3;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_ctx = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, false, 0);

}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */
