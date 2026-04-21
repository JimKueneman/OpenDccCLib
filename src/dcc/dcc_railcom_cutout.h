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
 * window after each DCC packet. The cutout sequence is:
 *   1. 88us delay after end bit (pre-cutout)
 *   2. H-bridge disabled, Ch1 UART enabled (464us window)
 *   3. Ch2 UART enabled (1080us window)
 *   4. H-bridge re-enabled, cutout_complete flag set
 *
 * This module is independent of the DCC bit encoder. Communication is
 * through interface function pointers only (no direct includes).
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#ifndef __DCC_RAILCOM_CUTOUT__
#define __DCC_RAILCOM_CUTOUT__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief RailCom cutout state machine phases. */
typedef enum {

    DCC_RAILCOM_CUTOUT_IDLE,      /**< No cutout in progress */
    DCC_RAILCOM_CUTOUT_DELAY,     /**< 88us pre-cutout delay */
    DCC_RAILCOM_CUTOUT_CH1,       /**< Ch1 window (464us) */
    DCC_RAILCOM_CUTOUT_CH2        /**< Ch2 window (1080us) */

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

} dcc_railcom_cutout_context_t;

    /**
     * @brief Initialize the RailCom cutout module.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     *  interface Pointer to populated  interface_dcc_railcom_cutout_t struct.
     */
extern void DccRailcomCutout_initialize(dcc_railcom_cutout_context_t *context, const interface_dcc_railcom_cutout_t *interface);

    /**
     * @brief Begin the cutout sequence. Called when the end bit completes.
     *  context Pointer to  dcc_railcom_cutout_context_t instance.
     *
     * @details Starts Timer 2 with an 88us one-shot for the pre-cutout delay.
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
     * @details Advances the cutout state machine through DELAY -> CH1 -> CH2 -> IDLE.
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

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_RAILCOM_CUTOUT__ */
