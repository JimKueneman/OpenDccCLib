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
 * @date 25 Sep 2026
 */

#include "dcc_scheduler.h"
#include <string.h>

#ifdef DCC_COMPILE_COMMAND_STATION

// =============================================================================
// Internal helpers
// =============================================================================

    /**
     * @brief Find an active slot matching (address, tag).
     * @param context Scheduler instance.
     * @param address DCC address half of the duplicate-combining key.
     * @param tag Sub-key half of the duplicate-combining key.
     * @return Slot index, or -1 if not found.
     */
static int16_t _find_slot(dcc_scheduler_context_t *context, dcc_address_t address, dcc_tag_enum tag) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (context->slots[slot_index].active && context->slots[slot_index].address == address && context->slots[slot_index].tag == tag) {

            return (int16_t)slot_index;

        }

    }

    return -1;

}

    /**
     * @brief Find an inactive (free) slot.
     * @param context Scheduler instance.
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
     *
     * @details Lower dcc_priority_enum values rank higher; among equal
     * priorities the lowest slot index wins. Refresh slots are never
     * considered here -- priority ranks one-shots only.
     *
     * @param context Scheduler instance.
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

    /** @brief Which refresh slots a pass of _select_refresh() takes. */
typedef enum {

    DCC_REFRESH_PASS_ANY,       /**< flat ring (cold tier disabled) */
    DCC_REFRESH_PASS_OVERDUE,   /**< any refresh slot unsent for refresh_cold_max_cycles or more */
    DCC_REFRESH_PASS_PROMPT,    /**< still inside its burst after an insert */
    DCC_REFRESH_PASS_DUE        /**< out of its burst, unsent for refresh_cold_cycles or more */

} refresh_pass_enum;

    /**
     * @brief True if a slot is an active refresh slot that belongs to @p pass.
     * @param context Scheduler instance (supplies the cold-tier thresholds).
     * @param slot Slot to test.
     * @param pass Refresh pass being scanned.
     * @return true if the slot qualifies for that pass, false otherwise.
     */
static bool _refresh_slot_in_pass(const dcc_scheduler_context_t *context, const dcc_scheduler_slot_t *slot, refresh_pass_enum pass) {

    if (!slot->active || !slot->auto_refresh) {

        return false;

    }

    switch (pass) {

        case DCC_REFRESH_PASS_OVERDUE:

            return slot->unsent_cycles >= context->refresh_cold_max_cycles;

        case DCC_REFRESH_PASS_PROMPT:

            return slot->prompt_sends_left > 0;

        case DCC_REFRESH_PASS_DUE:

            return slot->prompt_sends_left == 0 && slot->unsent_cycles >= context->refresh_cold_cycles;

        default:

            return true;

    }

}

    /**
     * @brief First slot in @p pass, scanning round-robin from that pass's cursor.
     *
     * @details The burst pass (and the flat ring) use refresh_index, which an
     * insert also moves to the changed slot. The overdue and due passes use their
     * own cursor, refresh_cold_index, so a stream of changes cannot keep them
     * restarting from the same few slots. The cursor is advanced past the slot
     * that is returned.
     *
     * @param context Scheduler instance.
     * @param pass Refresh pass to scan.
     * @return Slot index, or -1 if no slot is in that pass.
     */
static int16_t _select_refresh_pass(dcc_scheduler_context_t *context, refresh_pass_enum pass) {

    uint8_t *cursor = (pass == DCC_REFRESH_PASS_OVERDUE || pass == DCC_REFRESH_PASS_DUE) ? &context->refresh_cold_index : &context->refresh_index;
    uint8_t start_index = *cursor;
    uint8_t scan_count;

    for (scan_count = 0; scan_count < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; scan_count++) {

        uint8_t slot_index = (start_index + scan_count) % USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT;

        if (_refresh_slot_in_pass(context, &context->slots[slot_index], pass)) {

            *cursor = (slot_index + 1) % USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT;
            return (int16_t)slot_index;

        }

    }

    return -1;

}

    /**
     * @brief Select the next auto-refresh slot.
     *
     * @details With the cold tier enabled (refresh_cold_cycles > 0), in order:
     *   1. an overdue slot -- the starvation bound: no refresh slot, in its burst
     *      or not, stays unsent past refresh_cold_max_cycles because others keep
     *      changing;
     *   2. a slot inside its burst -- a changed command;
     *   3. a slot out of its burst that is merely due for a keep-alive.
     * A merely-due slot never delays a changed one. Right after an overdue send a
     * waiting burst goes first, so when more slots are active than the ceiling can
     * serve (some always overdue) changed commands still get every other cycle.
     * With the cold tier disabled every active refresh slot is taken in a flat
     * ring.
     *
     * @param context Scheduler instance.
     * @return Slot index, or -1 if no refresh slot is due this cycle.
     */
static int16_t _select_refresh(dcc_scheduler_context_t *context) {

    int16_t slot_index;

    if (context->refresh_cold_cycles == 0) {

        return _select_refresh_pass(context, DCC_REFRESH_PASS_ANY);

    }

    if (context->refresh_last_was_overdue) {

        slot_index = _select_refresh_pass(context, DCC_REFRESH_PASS_PROMPT);

        if (slot_index >= 0) {

            context->refresh_last_was_overdue = false;
            return slot_index;

        }

    }

    slot_index = _select_refresh_pass(context, DCC_REFRESH_PASS_OVERDUE);
    context->refresh_last_was_overdue = (slot_index >= 0);

    if (slot_index < 0) {

        slot_index = _select_refresh_pass(context, DCC_REFRESH_PASS_PROMPT);

    }

    if (slot_index < 0) {

        slot_index = _select_refresh_pass(context, DCC_REFRESH_PASS_DUE);

    }

    return slot_index;

}

    /**
     * @brief Age every refresh slot by one packet cycle.
     *
     * @details Called once per packet cycle, before a slot is chosen, so a slot's
     * cadence depends only on its own last send -- not on its ring position or on
     * how many other slots exist. Slots in their burst age too, so a burst that
     * keeps losing to other changes still falls overdue. Saturates rather than
     * wrapping. No-op while the cold tier is disabled.
     *
     * @param context Scheduler instance.
     */
static void _age_refresh_slots(dcc_scheduler_context_t *context) {

    uint8_t slot_index;

    if (context->refresh_cold_cycles == 0) {

        return;

    }

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        dcc_scheduler_slot_t *slot = &context->slots[slot_index];

        if (slot->active && slot->auto_refresh && slot->unsent_cycles < UINT16_MAX) {

            slot->unsent_cycles++;

        }

    }

}

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the scheduler module.
     *
     * @details Stores the interface, marks every slot inactive with zeroed
     * pacing counters, resets both round-robin cursors and loads the refresh
     * pacing from DCC_REFRESH_PROMPT_SENDS, DCC_REFRESH_COLD_CYCLES and
     * DCC_REFRESH_COLD_MAX_CYCLES.
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @param interface Pointer to populated interface_dcc_scheduler_t struct.
     * @endverbatim
     */
void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface) {

    uint8_t slot_index;

    context->interface = interface;
    context->refresh_index = 0;
    context->packet_complete_flag = false;
    context->first_packet_sent = false;
    context->last_addr_byte = 0x00;
    context->refresh_prompt_sends = DCC_REFRESH_PROMPT_SENDS;
    context->refresh_cold_cycles = DCC_REFRESH_COLD_CYCLES;
    context->refresh_cold_max_cycles = DCC_REFRESH_COLD_MAX_CYCLES;
    context->refresh_cold_index = 0;
    context->refresh_last_was_overdue = false;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        context->slots[slot_index].active = false;
        context->slots[slot_index].prompt_sends_left = 0;
        context->slots[slot_index].unsent_cycles = 0;

    }

}

    /**
     * @brief Insert or update a packet in the scheduler.
     *
     * @details Algorithm:
     * -# Refuse a one-shot whose repeat_count is 0 (it would never be sent and never freed)
     * -# Look for an active slot with the same (address, tag)
     * -# If none, take a free slot (fail if there is none) and zero its unsent_cycles
     * -# If one exists, it is an auto-refresh slot and the cold tier is off, move
     *    refresh_index to it so the changed command takes the next refresh turn
     * -# Copy the packet bytes, byte_count, preamble_bits and repeat_count into the slot
     * -# Store address, tag, priority and auto_refresh, and mark the slot active
     * -# Re-arm the prompt burst (prompt_sends_left = refresh_prompt_sends); unsent_cycles
     *    is left alone because an insert is not a send
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @param packet Pointer to dcc_packet_t to schedule (contents are copied).
     * @param address DCC address for duplicate combining key.
     * @param tag Sub-key for duplicate combining.
     * @param priority Packet priority level (ranks one-shots only).
     * @param auto_refresh true = keep in refresh cycle indefinitely.
     * @endverbatim
     *
     * @return true if the packet was scheduled, false if no free slot or if a
     *  one-shot was handed over with repeat_count 0.
     */
bool DccScheduler_insert(dcc_scheduler_context_t *context, const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {

    int16_t slot_index;
    uint8_t byte_index;

    /* A one-shot with repeat_count 0 would never be sent and never freed. */
    if (!auto_refresh && packet->repeat_count == 0) {

        return false;

    }

    /* Duplicate combining: look for existing (address, tag) slot */
    slot_index = _find_slot(context, address, tag);

    if (slot_index < 0) {

        /* No existing slot — allocate a new one */
        slot_index = _find_free_slot(context);
        if (slot_index < 0) {

            return false;  /* No free slots */

        }

        context->slots[slot_index].unsent_cycles = 0;

    } else if (auto_refresh && context->slots[slot_index].auto_refresh && context->refresh_cold_cycles == 0) {

        /* Flat ring: a changed command for a refresh slot takes the next refresh turn
         * instead of waiting for the ring to come round to it (up to one full ring of
         * packets). With the cold tier on, the burst pass already sends it promptly, and
         * moving the shared cursor here would let one slot changed every cycle keep every
         * other burst waiting for the overdue bound. */
        context->refresh_index = (uint8_t)slot_index;

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

    /* Every insert is a command to get out promptly: a burst at full rate, then
     * the keep-alive. Unused by one-shot slots. unsent_cycles is left alone: an
     * insert is not a send, and the overdue bound counts from the last send. */
    context->slots[slot_index].prompt_sends_left = context->refresh_prompt_sends;

    return true;

}

    /**
     * @brief Remove all slots for a given address.
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @param address The address to purge.
     * @endverbatim
     */
void DccScheduler_remove_address(dcc_scheduler_context_t *context, dcc_address_t address) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        if (context->slots[slot_index].active && context->slots[slot_index].address == address) {

            context->slots[slot_index].active = false;

        }

    }

}

    /**
     * @brief Remove all active slots and reset both round-robin cursors.
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @endverbatim
     */
void DccScheduler_clear(dcc_scheduler_context_t *context) {

    uint8_t slot_index;

    for (slot_index = 0; slot_index < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; slot_index++) {

        context->slots[slot_index].active = false;

    }

    context->refresh_index = 0;
    context->refresh_cold_index = 0;
    context->refresh_last_was_overdue = false;

}

    /**
     * @brief Notify the scheduler that the bit encoder finished a packet (ISR context).
     *
     * @details Only sets packet_complete_flag; DccScheduler_run() consumes it.
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @endverbatim
     */
void DCC_ISR_FUNC(DccScheduler_on_packet_complete)(dcc_scheduler_context_t *context) {

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
     *
     * @param first_byte First (address) byte of the packet.
     * @return true if the byte is in 0x70-0x7F, false otherwise.
     */
static bool _aliases_service_mode(uint8_t first_byte) {

    return first_byte >= 0x70 && first_byte <= 0x7F;

}

    /**
     * @brief Load an idle packet and clear the same-address guard state.
     * @param context Scheduler instance.
     * @param idle_packet Caller-owned packet the idle packet is built into.
     */
static void _load_idle(dcc_scheduler_context_t *context, dcc_packet_t *idle_packet) {

    context->interface->build_idle_packet(idle_packet);
    context->interface->load_packet(idle_packet);
    context->last_addr_byte = 0x00;
    context->first_packet_sent = true;

}

    /**
     * @brief Main loop processing. Selects next packet and feeds to bit encoder.
     *
     * @details Algorithm:
     * -# Return if no interface is wired
     * -# After the first packet, return until packet_complete_flag is set, then clear it
     * -# Return if the bit encoder does not report idle (belt and suspenders)
     * -# Age every refresh slot by one packet cycle
     * -# Pick the highest-priority one-shot; if found:
     *    - If its first byte is 0x70-0x7F and equals last_addr_byte, load an idle spacer
     *      instead (S-9.2 Section C footnote 11 5 ms gap) and retry next cycle
     *    - Otherwise load it, record last_addr_byte, fire on_packet_sent, decrement
     *      repeat_count and free the slot when it reaches 0
     * -# Else pick the next refresh slot; if found, apply the same same-address guard,
     *    load it, fire on_packet_sent, spend one burst send and zero unsent_cycles
     * -# Else load an idle packet
     *
     * @verbatim
     * @param context Pointer to dcc_scheduler_context_t instance.
     * @endverbatim
     */
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

    /* One packet cycle has passed: age the refresh slots before choosing. */
    _age_refresh_slots(context);

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

        /* Spend one send of the burst; the keep-alive clock restarts on every send. */
        if (context->slots[slot_index].prompt_sends_left > 0) {

            context->slots[slot_index].prompt_sends_left--;

        }

        context->slots[slot_index].unsent_cycles = 0;

        return;

    }

    /* 3. Nothing to send — idle */
    _load_idle(context, &idle_packet);

}

#endif /* DCC_COMPILE_COMMAND_STATION */
