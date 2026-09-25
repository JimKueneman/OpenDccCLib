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
 * @file dcc_scheduler.h
 * @brief Packet scheduler with duplicate combining and auto-refresh.
 *
 * @details Manages a static array of scheduler slots. Supports:
 * - Duplicate combining via (address, tag) composite key
 * - Auto-refresh of speed/function slots per NMRA requirements
 * - One-shot packets always go out before refresh slots; priority
 *   (e-stop > speed > function > accessory > CV > idle) ranks only the one-shots
 * - Round-robin through refresh slots, with a prompt burst after every insert
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_SCHEDULER__
#define __DCC_SCHEDULER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Load a packet into the bit encoder for transmission. */
    void (*load_packet)(const dcc_packet_t *packet);

        /** @brief Check if bit encoder is idle (ready for next packet). */
    bool (*is_encoder_idle)(void);

        /**
         * @brief Application notification that the scheduler has DISPATCHED a
         *        packet to the bit encoder. NULL = no notification.
         *
         * @details Fired by DccScheduler_run() immediately after load_packet(),
         * i.e. at the START of transmission -- the first bit goes out on the next
         * encoder tick (~58 us later). It is NOT fired when the packet finishes;
         * the encoder is single-buffered, so this is the earliest per-packet hook
         * and is effectively coincident with transmit-start. For the "packet
         * finished" event use @ref DccScheduler_on_packet_complete instead.
         * Not called for idle packets (only scheduled one-shot / refresh packets).
         */
    void (*on_packet_sent)(const dcc_packet_t *packet);

        /** @brief Build an idle packet into the provided packet struct. */
    void (*build_idle_packet)(dcc_packet_t *packet);

} interface_dcc_scheduler_t;

    /** @brief Instance context for the scheduler module.
     *
     *  @details Holds all per-instance state that was formerly file-scope static.
     *  Allocate one of these per DCC output channel that needs packet scheduling.
     */
typedef struct {

        /** @brief Injected dependencies; NULL until DccScheduler_initialize(). */
    const interface_dcc_scheduler_t *interface;

        /** @brief Slot pool; inactive slots are free. */
    dcc_scheduler_slot_t slots[USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT];

        /** @brief Round-robin cursor of the burst pass and of the flat ring; an insert moves it to the changed slot. */
    uint8_t refresh_index;

        /** @brief Set from ISR context by DccScheduler_on_packet_complete(), consumed by DccScheduler_run(). */
    volatile bool packet_complete_flag;

        /** @brief false until the first packet is loaded; DccScheduler_run() does not wait for packet_complete_flag before that. */
    bool first_packet_sent;

        /** @brief First byte (address byte) of the last non-idle packet loaded.
         *  Used to enforce the S-9.2 Section C footnote 11 >=5 ms gap between two
         *  same-address short-address (112-127) packets. 0x00 = none / last load
         *  was an idle packet. */
    uint8_t last_addr_byte;

        /** @brief Full-rate sends a refresh slot gets after each insert; set from DCC_REFRESH_PROMPT_SENDS by DccScheduler_initialize(). */
    uint8_t refresh_prompt_sends;

        /** @brief Keep-alive interval in packet cycles; set from DCC_REFRESH_COLD_CYCLES. 0 = flat round-robin, no cold tier. */
    uint16_t refresh_cold_cycles;

        /** @brief Starvation bound in packet cycles; set from DCC_REFRESH_COLD_MAX_CYCLES. A slot unsent this long is overdue. */
    uint16_t refresh_cold_max_cycles;

        /** @brief Round-robin cursor of the overdue and due passes (the burst pass uses refresh_index) -- see _select_refresh() in dcc_scheduler.c. */
    uint8_t refresh_cold_index;

        /** @brief true when the last refresh send was an overdue one, so a waiting burst goes next. */
    bool refresh_last_was_overdue;

} dcc_scheduler_context_t;

        /**
         * @brief Initialize the scheduler module.
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         * @param interface Pointer to populated @ref interface_dcc_scheduler_t struct.
         */
    extern void DccScheduler_initialize(dcc_scheduler_context_t *context, const interface_dcc_scheduler_t *interface);

        /**
         * @brief Main loop processing. Selects next packet and feeds to bit encoder.
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         *
         * @details Called from DccConfig_run(). Does nothing until the bit encoder
         * has finished the previous packet and reports idle. Then sends the
         * highest-priority pending one-shot if there is one, otherwise the next
         * auto-refresh slot in round-robin order, otherwise an idle packet. Two
         * consecutive packets to the same short address 112-127 are separated by
         * an idle packet (S-9.2 Section C footnote 11).
         */
    extern void DccScheduler_run(dcc_scheduler_context_t *context);

        /**
         * @brief Insert or update a packet in the scheduler.
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         *
         * @details If a slot with matching (address, tag) exists and is active,
         * its packet data is overwritten (duplicate combining). Otherwise a new
         * slot is allocated. Every insert re-arms the refresh slot's prompt burst
         * (refresh_prompt_sends full-rate sends before it drops to the keep-alive).
         *
         * @param packet Pointer to @ref dcc_packet_t to schedule.
         * @note Contents of the packet are copied; the caller's copy may be reused after the call.
         * @param address DCC address for duplicate combining key.
         * @param tag Sub-key for duplicate combining (@ref dcc_tag_enum).
         * @param priority Packet priority level (@ref dcc_priority_enum).
         * @param auto_refresh true = keep in refresh cycle indefinitely.
         * @return true if packet was scheduled, false if no free slots or if a
         *  one-shot was handed over with repeat_count 0 (it would never be sent).
         */
    extern bool DccScheduler_insert(dcc_scheduler_context_t *context, const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh);

        /**
         * @brief Remove all slots for a given address.
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         * @param address The address to purge.
         */
    extern void DccScheduler_remove_address(dcc_scheduler_context_t *context, dcc_address_t address);

        /**
         * @brief Remove all active slots (clear the scheduler).
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         */
    extern void DccScheduler_clear(dcc_scheduler_context_t *context);

        /**
         * @brief Notify the scheduler that the bit encoder finished a packet.
         * @param context Pointer to @ref dcc_scheduler_context_t instance.
         *
         * @details Called from ISR context (via on_packet_complete callback) when a
         * packet has FINISHED transmitting. Sets a flag that DccScheduler_run() will
         * process. This is the "packet finished" counterpart to the on_packet_sent
         * dispatch notification (see @ref interface_dcc_scheduler_t::on_packet_sent).
         */
    extern void DccScheduler_on_packet_complete(dcc_scheduler_context_t *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_SCHEDULER__ */
