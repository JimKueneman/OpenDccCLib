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
 * on the wire by driving the hardware timer compare register. The user's
 * timer output compare toggle mode handles the actual pin transitions.
 *
 * The bit encoder is double-buffered: it reads from an "active" packet while
 * the scheduler prepares the "ready" packet. On packet completion, it signals
 * the main loop to swap buffers.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
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

        /** @brief Set timer compare value for next half-bit period (ISR context).
         *  Used by the variable-period ISR (DccBitEncoder_half_bit_isr). */
    void (*timer_set_period)(uint16_t half_bit_period_usec);

        /** @brief Toggle this channel's DCC output GPIO pin (ISR context).
         *  Used by the fixed-period tick ISR (DccBitEncoder_tick_isr). */
    void (*pin_toggle)(void);

        /** @brief Tristate H-bridge for RailCom cutout. NULL = skip cutout. */
    void (*railcom_cutout_begin)(void);

        /** @brief Resume H-bridge after RailCom cutout. NULL = skip cutout. */
    void (*railcom_cutout_end)(void);

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

    const interface_dcc_bit_encoder_t *interface;
    dcc_bit_state_enum state;
    dcc_packet_t active_packet;
    volatile bool packet_loaded;
    bool running;
    uint8_t preamble_count;
    uint8_t byte_index;
    uint8_t bit_index;
    uint8_t half_bit;

        /** @brief Tick counter for fixed-period ISR (DccBitEncoder_tick_isr).
         *  Counts 58us ticks within the current half-bit period. */
    uint8_t tick_counter;

        /** @brief true if the current bit is a one-bit (1 tick per half),
         *  false for a zero-bit (2 ticks per half). Used by tick ISR. */
    bool current_bit_is_one;

        /** @brief Set by the RailCom cutout timer (Timer 2) when cutout
         *  is complete. Polled by the tick ISR in RAILCOM_CUTOUT state. */
    volatile bool cutout_complete;

        /** @brief Look-ahead flag: true = toggle pin on the very first
         *  instruction of the next ISR call. Pre-computed by the state
         *  machine so the toggle has deterministic, minimal latency. */
    bool toggle_next;

} dcc_bit_encoder_context_t;

    /**
     * @brief Initialize the bit encoder module.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     *  interface Pointer to populated  interface_dcc_bit_encoder_t struct.
     */
extern void DccBitEncoder_initialize(dcc_bit_encoder_context_t *context, const interface_dcc_bit_encoder_t *interface);

    /**
     * @brief ISR entry point — call from timer compare-match ISR.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     *
     * @details Each call handles one half-bit. The state machine advances through:
     * PREAMBLE -> START_BIT -> DATA -> (separator or END_BIT) -> RAILCOM_CUTOUT -> IDLE.
     * Calls timer_set_period() to set the next half-bit timing.
     */
extern void DccBitEncoder_half_bit_isr(dcc_bit_encoder_context_t *context);

    /**
     * @brief Fixed-period tick ISR entry point — call every 58us from shared timer.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     *
     * @details Alternative to DccBitEncoder_half_bit_isr() for the shared-timer
     * architecture. The timer period never changes. One-bits toggle every tick
     * (58us half-period), zero-bits skip one tick and toggle on the second
     * (116us half-period). Uses pin_toggle() instead of timer_set_period().
     */
extern void DccBitEncoder_tick_isr(dcc_bit_encoder_context_t *context);

    /**
     * @brief Load a new packet for transmission.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     *  packet Pointer to  dcc_packet_t to transmit. Contents are copied.
     *
     * @details Called from main loop context (under lock). The bit encoder begins
     * transmitting this packet starting with the preamble on the next ISR cycle.
     */
extern void DccBitEncoder_load_packet(dcc_bit_encoder_context_t *context, const dcc_packet_t *packet);

    /**
     * @brief Check if the bit encoder has finished transmitting its current packet.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     * @return true if idle (ready for a new packet), false if transmitting.
     */
extern bool DccBitEncoder_is_idle(const dcc_bit_encoder_context_t *context);

    /**
     * @brief Start the bit encoder. Begins generating DCC signal.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     */
extern void DccBitEncoder_start(dcc_bit_encoder_context_t *context);

    /**
     * @brief Stop the bit encoder. Halts DCC signal generation.
     *  context Pointer to  dcc_bit_encoder_context_t instance.
     */
extern void DccBitEncoder_stop(dcc_bit_encoder_context_t *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_BIT_ENCODER__ */
