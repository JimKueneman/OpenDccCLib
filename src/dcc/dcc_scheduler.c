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
 * @file dcc_scheduler.c
 * @brief Packet scheduler with duplicate combining and auto-refresh.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "dcc_scheduler.h"
#include <string.h>

#ifdef DCC_COMPILE_COMMAND_STATION

// =============================================================================
// Internal helpers
// =============================================================================

    /**
     * @brief Find an active slot matching (address, tag).
     * @return Slot index, or -1 if not found.
     */
static int16_t _find_slot(dcc_scheduler_context_t *context, dcc_address_t address, dcc_tag_enum tag) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (context->slots[slot_index].active &&
            context->slots[slot_index].address == address &&
            context->slots[slot_index].tag == tag) {

            return (int16_t)slot_index;

        }

    }

    return -1;

}

    /**
     * @brief Find an inactive (free) slot.
     * @return Slot index, or -1 if all slots are in use.
     */
static int16_t _find_free_slot(dcc_scheduler_context_t *context) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (!context->slots[slot_index].active) {

            return (int16_t)slot_index;

        }

    }

    return -1;

}

    /**
     * @brief Select the highest-priority one-shot slot (non-auto-refresh with
     *        repeat_count > 0).
     * @return Slot index, or -1 if no one-shot packets pending.
     */
static int16_t _select_one_shot(dcc_scheduler_context_t *context) {

    int16_t best_index = -1;
    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (!context->slots[slot_index].active) {

            continue;

        }
        if (context->slots[slot_index].auto_refresh) {

            continue;

        }
        if (context->slots[slot_index].packet.repeat_count == 0) {

            continue;

        }

        if (best_index == -1 || context->slots[slot_index].priority < context->slots[best_index].priority) {

            best_index = (int16_t)slot_index;

        }

    }

    return best_index;

}

    /**
     * @brief Select the next auto-refresh slot via round-robin.
     * @return Slot index, or -1 if no refresh slots exist.
     */
static int16_t _select_refresh(dcc_scheduler_context_t *context) {

    uint8_t start_index = context->refresh_index;
    uint8_t scan_count;

    for (scan_count = 0; scan_count < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; scan_count++) {

        uint8_t slot_index = (start_index + scan_count) % USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT;

        if (context->slots[slot_index].active && context->slots[slot_index].auto_refresh) {

            context->refresh_index = (slot_index + 1) % USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT;
            return (int16_t)slot_index;

        }

    }

    return -1;

}

// =============================================================================
// Public API
// =============================================================================

void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface) {

    uint8_t slot_index;

    context->interface = interface;
    context->refresh_index = 0;
    context->packet_complete_flag = false;
    context->first_packet_sent = false;
    context->last_addr_byte = 0x00;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        context->slots[slot_index].active = false;

    }

}

bool DccScheduler_insert(dcc_scheduler_context_t *context, const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {

    int16_t slot_index;
    uint8_t byte_index;

    /* Duplicate combining: look for existing (address, tag) slot */
    slot_index = _find_slot(context, address, tag);

    if (slot_index < 0) {

        /* No existing slot — allocate a new one */
        slot_index = _find_free_slot(context);
        if (slot_index < 0) {

            return false;  /* No free slots */

        }

    }

    /* Copy packet data */
    for (byte_index = 0; byte_index < packet->byte_count && byte_index < DCC_PACKET_MAX_BYTES; byte_index++) {

        context->slots[slot_index].packet.data[byte_index] = packet->data[byte_index];

    }

    context->slots[slot_index].packet.byte_count = packet->byte_count;
    context->slots[slot_index].packet.preamble_bits = packet->preamble_bits;
    context->slots[slot_index].packet.repeat_count = packet->repeat_count;

    context->slots[slot_index].address = address;
    context->slots[slot_index].tag = tag;
    context->slots[slot_index].priority = priority;
    context->slots[slot_index].auto_refresh = auto_refresh;
    context->slots[slot_index].active = true;

    return true;

}

void DccScheduler_remove_address(dcc_scheduler_context_t *context, dcc_address_t address) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (context->slots[slot_index].active && context->slots[slot_index].address == address) {

            context->slots[slot_index].active = false;

        }

    }

}

void DccScheduler_clear(dcc_scheduler_context_t *context) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        context->slots[slot_index].active = false;

    }

    context->refresh_index = 0;

}

void DccScheduler_on_packet_complete(dcc_scheduler_context_t *context) {

    context->packet_complete_flag = true;

}

    /**
     * @brief True if a packet's first byte aliases a service-mode command byte.
     *
     * @details S-9.2 Section C footnote 11: a short-address ops packet for
     * addresses 112-127 has a first byte of 0x70-0x7F, which is bit-identical to
     * the service-mode register/paged command byte (0111CRRR). Two such packets
     * to the same address within 5 ms can be misread as service-mode programming
     * by older decoders. Long (0xC0-0xFF) and accessory (0x80-0xBF) first bytes
     * never alias, so only short addresses 112-127 are affected.
     */
static bool _aliases_service_mode(uint8_t first_byte) {

    return first_byte >= 0x70 && first_byte <= 0x7F;

}

    /**
     * @brief Load an idle packet and clear the same-address guard state.
     */
static void _load_idle(dcc_scheduler_context_t *context, dcc_packet_t *idle_packet) {

    context->interface->build_idle_packet(idle_packet);
    context->interface->load_packet(idle_packet);
    context->last_addr_byte = 0x00;
    context->first_packet_sent = true;

}

void DccScheduler_run(dcc_scheduler_context_t *context) {

    int16_t slot_index;
    dcc_packet_t idle_packet;
    memset(&idle_packet, 0, sizeof(idle_packet));

    if (!context->interface) {

        return;

    }

    /* On first call, the encoder is idle and needs a packet immediately.
     * After that, we wait for the packet_complete_flag from the ISR. */
    if (context->first_packet_sent) {

        if (!context->packet_complete_flag) {

            return;

        }
        context->packet_complete_flag = false;

    }

    /* Also verify the encoder is actually idle (belt and suspenders) */
    if (!context->interface->is_encoder_idle()) {

        return;

    }

    /* 1. Try one-shot packets first (highest priority wins) */
    slot_index = _select_one_shot(context);

    if (slot_index >= 0) {

        uint8_t first_byte = context->slots[slot_index].packet.data[0];

        /* S-9.2 Section C fn.11: do not follow a short-address 112-127 packet
         * (first byte 0x70-0x7F, which aliases a service-mode command) with the
         * SAME address inside 5 ms. Emit an idle spacer (~5.8 ms) and retry the
         * real packet next cycle; repeat_count is left untouched. */
        if (_aliases_service_mode(first_byte) && first_byte == context->last_addr_byte) {

            _load_idle(context, &idle_packet);
            return;

        }

        context->interface->load_packet(&context->slots[slot_index].packet);
        context->last_addr_byte = first_byte;
        context->first_packet_sent = true;

        if (context->interface->on_packet_sent) {

            context->interface->on_packet_sent(&context->slots[slot_index].packet);

        }

        /* Decrement repeat count; deactivate when exhausted */
        context->slots[slot_index].packet.repeat_count--;
        if (context->slots[slot_index].packet.repeat_count == 0) {

            context->slots[slot_index].active = false;

        }

        return;

    }

    /* 2. Try auto-refresh round-robin */
    slot_index = _select_refresh(context);

    if (slot_index >= 0) {

        uint8_t first_byte = context->slots[slot_index].packet.data[0];

        /* Same 5 ms same-address guard as above (S-9.2 Section C fn.11). */
        if (_aliases_service_mode(first_byte) && first_byte == context->last_addr_byte) {

            _load_idle(context, &idle_packet);
            return;

        }

        context->interface->load_packet(&context->slots[slot_index].packet);
        context->last_addr_byte = first_byte;
        context->first_packet_sent = true;

        if (context->interface->on_packet_sent) {

            context->interface->on_packet_sent(&context->slots[slot_index].packet);

        }

        return;

    }

    /* 3. Nothing to send — idle */
    _load_idle(context, &idle_packet);

}

#endif /* DCC_COMPILE_COMMAND_STATION */
