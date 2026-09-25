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
 * @file dcc_bit_encoder.h
 * @brief ISR-level bit encoder for DCC packet transmission.
 *
 * @details Serializes a dcc_packet_t into preamble + framed bytes + end bit
 * on the wire from a fixed-period 58 us tick ISR. The encoder decides on each
 * tick whether the next tick must toggle the DCC output pin (toggle_next); the
 * caller performs that toggle through pin_toggle() before calling
 * DccBitEncoder_tick_isr() again. A one-bit half is one tick (58 us), a
 * zero-bit half is two ticks (116 us).
 *
 * The bit encoder holds a single active packet plus a packet_loaded flag: it
 * transmits the active packet, and on completion signals the main loop, which
 * loads the next packet via DccBitEncoder_load_packet(). (There is no separate
 * front/back buffer swap; the reload happens within the inter-packet window.)
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_BIT_ENCODER__
#define __DCC_BIT_ENCODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Toggle this channel's DCC output GPIO pin (ISR context).
         *  Used by the fixed-period tick ISR (DccBitEncoder_tick_isr). */
    void (*pin_toggle)(void);

        /** @brief Begin the RailCom cutout sequence at the end bit (starts the
         *  cutout timer). NULL = hardware not RailCom-capable, no cutout. */
    void (*railcom_cutout_begin)(void);

        /** @brief Called when packet transmission is complete (ISR context).
         *  Sets a flag for the main loop to process. */
    void (*on_packet_complete)(void);

} interface_dcc_bit_encoder_t;

    /** @brief Instance context for the bit encoder module.
     *
     *  @details Holds all per-instance state that was formerly file-scope static.
     *  Allocate one of these per DCC output channel.
     */
typedef struct {

        /** @brief Injected dependencies; NULL until DccBitEncoder_initialize(). */
    const interface_dcc_bit_encoder_t *interface;

        /** @brief Current bit-level transmit state (@ref dcc_bit_state_enum). */
    dcc_bit_state_enum state;

        /** @brief Packet being transmitted; a copy of what was handed to DccBitEncoder_load_packet(). */
    dcc_packet_t active_packet;

        /** @brief Handoff flag: set by DccBitEncoder_load_packet() after the copy, cleared by the ISR when the end bit is done. */
    volatile bool packet_loaded;

        /** @brief true between DccBitEncoder_start() and DccBitEncoder_stop(); the tick ISR does nothing while false. */
    bool running;

        /** @brief Preamble one-bits still to send in the PREAMBLE state. */
    uint8_t preamble_count;

        /** @brief Index of the byte being sent within active_packet.data. */
    uint8_t byte_index;

        /** @brief Bit position (7..0) of the data bit currently being sent; the byte is done once bit 0 has gone out. */
    uint8_t bit_index;

        /** @brief 0 during the first half of a bit, 1 during the second; the state machine advances when the bit completes. */
    uint8_t half_bit;

        /** @brief Tick counter for fixed-period ISR (DccBitEncoder_tick_isr).
         *  Counts 58us ticks within the current half-bit period. */
    uint8_t tick_counter;

        /** @brief true if the current bit is a one-bit (1 tick per half),
         *  false for a zero-bit (2 ticks per half). Used by tick ISR. */
    bool current_bit_is_one;

        /** @brief Look-ahead flag: true = toggle pin on the very first
         *  instruction of the next ISR call. Pre-computed by the state
         *  machine so the toggle has deterministic, minimal latency. */
    bool toggle_next;

        /** @brief Set by the END_BIT handler, consumed on the following tick.
         *  The state machine runs one half-bit ahead of the wire (it advances on
         *  the tick that starts a bit's second half), so the end bit's LAST edge
         *  is the pin toggle of the tick after the handler. S-9.3.2 measures
         *  T_CS from that edge; railcom_cutout_begin() fires on that tick. */
    bool railcom_cutout_arm_pending;

} dcc_bit_encoder_context_t;

        /**
         * @brief Initialize the bit encoder module.
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         * @param interface Pointer to populated @ref interface_dcc_bit_encoder_t struct.
         */
    extern void DccBitEncoder_initialize(dcc_bit_encoder_context_t *context, const interface_dcc_bit_encoder_t *interface);

        /**
         * @brief Fixed-period tick ISR entry point — call every 58us from shared timer.
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         *
         * @details The timer period never changes. One-bits toggle every tick
         * (58us half-period), zero-bits skip one tick and toggle on the second
         * (116us half-period). The caller must toggle the pin via pin_toggle()
         * BEFORE this call whenever toggle_next is set; this call then advances
         * the state machine and recomputes toggle_next for the next tick. The
         * RailCom cutout (railcom_cutout_begin) is armed on the tick whose toggle
         * is the end bit's last edge, and on_packet_complete fires when the end
         * bit is done.
         */
    extern void DccBitEncoder_tick_isr(dcc_bit_encoder_context_t *context);

        /**
         * @brief Load a new packet for transmission.
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         * @param packet Pointer to @ref dcc_packet_t to transmit. Contents are copied.
         *
         * @details Called from main loop context, only while
         * DccBitEncoder_is_idle() is true. The copy is published to the ISR as a
         * whole; the encoder begins transmitting the packet, starting with its
         * preamble, at the next full-bit boundary.
         */
    extern void DccBitEncoder_load_packet(dcc_bit_encoder_context_t *context, const dcc_packet_t *packet);

        /**
         * @brief Check if the bit encoder has finished transmitting its current packet.
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         * @return true if idle (ready for a new packet), false if transmitting.
         */
    extern bool DccBitEncoder_is_idle(const dcc_bit_encoder_context_t *context);

        /**
         * @brief Start the bit encoder. Begins generating DCC signal (idle one-bits until a packet is loaded).
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         */
    extern void DccBitEncoder_start(dcc_bit_encoder_context_t *context);

        /**
         * @brief Stop the bit encoder. Halts DCC signal generation.
         * @param context Pointer to @ref dcc_bit_encoder_context_t instance.
         */
    extern void DccBitEncoder_stop(dcc_bit_encoder_context_t *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_BIT_ENCODER__ */
