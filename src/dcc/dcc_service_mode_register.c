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

typedef enum {

    REGISTER_STATE_IDLE,
    REGISTER_STATE_PAGE_PRESET,
    REGISTER_STATE_COMMAND

} register_state_enum;

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

static void _build_register_packet(dcc_packet_t *packet, uint8_t register_number, uint8_t value, bool write) {

    uint8_t prefix = write
                     ? DCC_SERVICE_REGISTER_WRITE_PREFIX
                     : DCC_SERVICE_REGISTER_VERIFY_PREFIX;

    packet->data[0] = prefix | ((register_number - 1) & 0x07);
    packet->data[1] = value;
    packet->byte_count = 2;
    _append_xor(packet);
    packet->preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet->repeat_count = 0;

}

static void _on_command_complete(dcc_service_mode_result_t result) {

    _active_context->register_state = REGISTER_STATE_IDLE;

    if (_active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

    /* Page-preset finished. Per S-9.2.3 the register command follows the
     * page-preset unconditionally -- the preset only guarantees the page
     * register state, so its own ACK result is not required to proceed.
     * Register VERIFY uses 7+ command packets; a write to Register 1 uses the
     * longer 10-packet recovery (both per S-9.2.3). */
static void _on_preset_complete(dcc_service_mode_result_t result) {

    dcc_packet_t packet;
    (void)result;
    memset(&packet, 0, sizeof(packet));

    _active_context->register_state = REGISTER_STATE_COMMAND;
    _build_register_packet(&packet, _active_context->register_number, _active_context->value, _active_context->is_write);

    if (_active_context->is_write) {

        uint8_t recovery = (_active_context->register_number == 1)
                           ? DCC_SERVICE_MODE_RECOVERY_COUNT_LONG
                           : DCC_SERVICE_MODE_RECOVERY_COUNT;

        _active_context->interface->begin_operation(&packet, &_on_command_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, recovery);

    } else {

        _active_context->interface->begin_operation(&packet, &_on_command_complete, false, DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT, 0);

    }

}

    /* Common entry: validate, latch the pending command, and start the
     * page-preset (write page register -> page 1) that precedes it. */
static bool _begin_with_preset(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value, bool is_write) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (register_number < 1 || register_number > 8) {

        return false;

    }

    if (!context->interface->is_common_idle()) {

        return false;

    }

    if (context->register_state != REGISTER_STATE_IDLE) {

        return false;

    }

    context->register_number = register_number;
    context->value = value;
    context->is_write = is_write;
    context->register_state = REGISTER_STATE_PAGE_PRESET;
    _active_context = context;

    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, DCC_SERVICE_MODE_PAGE_PRESET_PAGE, true);

    if (!context->interface->begin_operation(&packet, &_on_preset_complete, true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT)) {

        context->register_state = REGISTER_STATE_IDLE;
        return false;

    }

    return true;

}

void DccServiceModeRegister_initialize(dcc_service_mode_register_context_t *context, const interface_dcc_service_mode_register_t *interface) {

    context->interface = interface;
    context->register_state = REGISTER_STATE_IDLE;

}

bool DccServiceModeRegister_write(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value) {

    return _begin_with_preset(context, register_number, value, true);

}

bool DccServiceModeRegister_verify(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value) {

    return _begin_with_preset(context, register_number, value, false);

}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */
