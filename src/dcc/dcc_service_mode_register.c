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
 * @file dcc_service_mode_register.c
 * @brief Physical register mode CV programming (legacy, registers 1-8).
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#include "dcc_service_mode_register.h"

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

#include <string.h>

static dcc_service_mode_register_context_t *_active_context = (void *)0;

static void _append_xor(dcc_packet_t *packet) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count; byte_index++) {

        xor_byte ^= packet->data[byte_index];

    }

    packet->data[packet->byte_count] = xor_byte;
    packet->byte_count++;

}

static void _on_step_complete(dcc_service_mode_result_t result) {

    if (_active_context && _active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

void DccServiceModeRegister_initialize(dcc_service_mode_register_context_t *context, const interface_dcc_service_mode_register_t *interface) {

    context->interface = interface;

}

bool DccServiceModeRegister_write(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (register_number < 1 || register_number > 8) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    packet.data[0] = DCC_SERVICE_REGISTER_WRITE_PREFIX | ((register_number - 1) & 0x07);
    packet.data[1] = value;
    packet.byte_count = 2;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, true, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

bool DccServiceModeRegister_verify(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (register_number < 1 || register_number > 8) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    packet.data[0] = DCC_SERVICE_REGISTER_VERIFY_PREFIX | ((register_number - 1) & 0x07);
    packet.data[1] = value;
    packet.byte_count = 2;
    _append_xor(&packet);
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    _active_context = context;
    return context->interface->begin_operation(&packet, &_on_step_complete, false, 0);

}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */
