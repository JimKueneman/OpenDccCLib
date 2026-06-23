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
 * @date 07 Apr 2026
 */

#include "dcc_service_mode_paged.h"

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

#include <string.h>

// =============================================================================
// Internal types
// =============================================================================

typedef enum {

    PAGED_STATE_IDLE,
    PAGED_STATE_PAGE_SELECT,
    PAGED_STATE_DATA_ACCESS

} paged_state_enum;

/* Module-level pointer to the active context for callbacks */
static dcc_service_mode_paged_context_t *_active_context = (void *)0;

// =============================================================================
// Static helpers
// =============================================================================

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

    /* Forward declarations for callback chain */
static void _on_data_access_complete(dcc_service_mode_result_t result);

static void _on_page_select_complete(dcc_service_mode_result_t result) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (result != DCC_SERVICE_MODE_SUCCESS) {

        _active_context->paged_state = PAGED_STATE_IDLE;

        if (_active_context->interface->on_complete) {

            _active_context->interface->on_complete(result);

        }

        return;

    }

    /* Page select succeeded — start data access step */
    _active_context->paged_state = PAGED_STATE_DATA_ACCESS;
    _build_register_packet(&packet, _active_context->data_register, _active_context->data_value, _active_context->is_write);
    _active_context->interface->begin_operation(&packet, &_on_data_access_complete, _active_context->is_write, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

static void _on_data_access_complete(dcc_service_mode_result_t result) {

    _active_context->paged_state = PAGED_STATE_IDLE;

    if (_active_context->interface->on_complete) {

        _active_context->interface->on_complete(result);

    }

}

// =============================================================================
// Public API
// =============================================================================

void DccServiceModePaged_initialize(dcc_service_mode_paged_context_t *context, const interface_dcc_service_mode_paged_t *interface) {

    context->interface = interface;
    context->paged_state = PAGED_STATE_IDLE;

}

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

    if (context->paged_state != PAGED_STATE_IDLE) {

        return false;

    }

    page = (uint8_t)(((cv_number - 1) / 4) + 1);
    context->data_register = (uint8_t)(((cv_number - 1) % 4) + 1);
    context->data_value = value;
    context->is_write = true;

    context->paged_state = PAGED_STATE_PAGE_SELECT;
    _active_context = context;
    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, page, true);

    return context->interface->begin_operation(&packet, &_on_page_select_complete, true, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

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

    if (context->paged_state != PAGED_STATE_IDLE) {

        return false;

    }

    page = (uint8_t)(((cv_number - 1) / 4) + 1);
    context->data_register = (uint8_t)(((cv_number - 1) % 4) + 1);
    context->data_value = value;
    context->is_write = false;

    context->paged_state = PAGED_STATE_PAGE_SELECT;
    _active_context = context;
    _build_register_packet(&packet, DCC_SERVICE_MODE_PAGE_REGISTER, page, true);

    return context->interface->begin_operation(&packet, &_on_page_select_complete, true, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */
