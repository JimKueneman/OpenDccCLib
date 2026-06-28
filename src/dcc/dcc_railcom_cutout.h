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
 * @file dcc_railcom_cutout.h
 * @brief RailCom cutout timer state machine for the command station.
 *
 * @details Manages the one-shot Timer 2 that creates the RailCom cutout
 * window after each DCC packet. Each state loads a one-shot duration; its
 * action fires when that duration EXPIRES. Cumulative spec event times in
 * parentheses (defaults shown):
 *   1. DELAY    (26us)  -> T_CS  = 26us:  tristate H-bridge / begin cutout
 *   2. SETTLING (54us)  -> T_TS1 = 80us:  enable UART Rx (Ch1 opens)
 *   3. CH1      (97us)  -> T_TC1 = 177us: disable UART Rx (Ch1 closes)
 *   4. GAP      (16us)  -> T_TS2 = 193us: enable UART Rx (Ch2 opens)
 *   5. CH2      (261us) -> T_CE  = 454us: disable UART Rx, restore H-bridge,
 *                                         set cutout_complete flag
 *
 * This module is independent of the DCC bit encoder. Communication is
 * through interface function pointers only (no direct includes).
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#ifndef __DCC_RAILCOM_CUTOUT__
#define __DCC_RAILCOM_CUTOUT__

#include "dcc_types.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_COMMAND_STATION)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief RailCom cutout state machine phases. */
typedef enum {

    DCC_RAILCOM_CUTOUT_IDLE,      /**< No cutout in progress */
    DCC_RAILCOM_CUTOUT_DELAY,     /**< Pre-cutout delay; expiry tristates H-bridge (T_CS) */
    DCC_RAILCOM_CUTOUT_SETTLING,  /**< Settling; expiry enables UART Rx (T_TS1) */
    DCC_RAILCOM_CUTOUT_CH1,       /**< Ch1 window; expiry disables UART Rx (T_TC1) */
    DCC_RAILCOM_CUTOUT_GAP,       /**< Ch1/Ch2 gap; expiry re-enables UART Rx (T_TS2) */
    DCC_RAILCOM_CUTOUT_CH2        /**< Ch2 window; expiry disables UART Rx, restores H-bridge (T_CE) */

} dcc_railcom_cutout_state_enum;

    /** @brief Interface struct — hardware dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Start the one-shot timer with the given period in microseconds. */
    void (*timer_one_shot_start)(uint16_t period_usec);

        /** @brief Stop the one-shot timer. */
    void (*timer_one_shot_stop)(void);

        /** @brief Begin the RailCom cutout (disable/tristate the H-bridge). */
    void (*begin_railcom_cutout)(void);

        /** @brief End the RailCom cutout (re-enable the H-bridge). */
    void (*end_railcom_cutout)(void);

        /** @brief Enable UART receive for the RailCom cutout window. */
    void (*uart_rx_enable)(void);

        /** @brief Disable UART receive after the RailCom cutout window. */
    void (*uart_rx_disable)(void);

        /** @brief Signal that the cutout is complete. Called from Timer 2 ISR.
         *  Typically sets a volatile bool in the bit encoder context. */
    void (*on_cutout_complete)(void);

} interface_dcc_railcom_cutout_t;

    /** @brief Instance context for the RailCom cutout module. */
typedef struct {

    const interface_dcc_railcom_cutout_t *interface;
    dcc_railcom_cutout_state_enum state;

        /** @brief DELAY duration (us) — fires tristate at T_CS on expiry. */
    uint16_t start_delay_us;

        /** @brief SETTLING duration (us) — fires UART Rx enable at T_TS1 on expiry. */
    uint16_t uart_rx_delay_us;

        /** @brief CH1 window duration (us) — fires UART Rx disable at T_TC1 on expiry. */
    uint16_t ch1_window_us;

        /** @brief GAP duration (us) — fires UART Rx enable at T_TS2 on expiry. */
    uint16_t ch1_ch2_gap_us;

        /** @brief CH2 window duration (us) — fires UART Rx disable + restore at T_CE on expiry. */
    uint16_t ch2_window_us;

} dcc_railcom_cutout_context_t;

    /**
     * @brief Initialize the RailCom cutout module.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     *  interface Pointer to populated  interface_dcc_railcom_cutout_t struct.
     *  start_delay  DELAY duration (us)    — tristate H-bridge at T_CS on expiry.
     *  uart_rx_delay SETTLING duration (us) — enable UART Rx at T_TS1 on expiry.
     *  ch1          CH1 window duration (us) — disable UART Rx at T_TC1 on expiry.
     *  gap          GAP duration (us)      — re-enable UART Rx at T_TS2 on expiry.
     *  ch2          CH2 window duration (us) — disable UART Rx + restore at T_CE on expiry.
     */
extern void DccRailcomCutout_initialize(dcc_railcom_cutout_context_t *context, const interface_dcc_railcom_cutout_t *interface,
                                        uint16_t start_delay, uint16_t uart_rx_delay, uint16_t ch1, uint16_t gap, uint16_t ch2);

    /**
     * @brief Begin the cutout sequence. Called when the end bit completes.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     *
     * @details Starts Timer 2 with a one-shot for the pre-cutout DELAY duration.
     */
extern void DccRailcomCutout_begin(dcc_railcom_cutout_context_t *context);

    /**
     * @brief Cancel the cutout sequence. Called on power-off or error.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     */
extern void DccRailcomCutout_cancel(dcc_railcom_cutout_context_t *context);

    /**
     * @brief Timer 2 ISR entry point. Call from the one-shot timer ISR.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     *
     * @details Advances the cutout state machine through
     *  DELAY -> SETTLING -> CH1 -> GAP -> CH2 -> IDLE.
     */
extern void DccRailcomCutout_timer_isr(dcc_railcom_cutout_context_t *context);

    /**
     * @brief Check if the cutout is idle (no cutout in progress).
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     * @return true if idle, false if a cutout is in progress.
     */
extern bool DccRailcomCutout_is_idle(const dcc_railcom_cutout_context_t *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_RAILCOM_CUTOUT__ */
