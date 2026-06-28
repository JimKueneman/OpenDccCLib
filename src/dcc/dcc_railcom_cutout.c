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
 * @file dcc_railcom_cutout.c
 * @brief RailCom cutout timer state machine for the command station.
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#include "dcc_railcom_cutout.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_COMMAND_STATION)

void DccRailcomCutout_initialize(dcc_railcom_cutout_context_t *context, const interface_dcc_railcom_cutout_t *interface,
                                 uint16_t start_delay, uint16_t uart_rx_delay, uint16_t ch1, uint16_t gap, uint16_t ch2) {

    context->interface = interface;
    context->state = DCC_RAILCOM_CUTOUT_IDLE;
    context->start_delay_us = start_delay;
    context->uart_rx_delay_us = uart_rx_delay;
    context->ch1_window_us = ch1;
    context->ch1_ch2_gap_us = gap;
    context->ch2_window_us = ch2;

}

void DccRailcomCutout_begin(dcc_railcom_cutout_context_t *context) {

    if (!context->interface) {

        return;

    }

    context->state = DCC_RAILCOM_CUTOUT_DELAY;

    /* Load the DELAY duration; tristate fires when it expires (T_CS). */
    if (context->interface->timer_one_shot_start) {

        context->interface->timer_one_shot_start(context->start_delay_us);

    }

}

void DccRailcomCutout_cancel(dcc_railcom_cutout_context_t *context) {

    if (!context->interface) {

        return;

    }

    if (context->state != DCC_RAILCOM_CUTOUT_IDLE) {

        if (context->interface->timer_one_shot_stop) {

            context->interface->timer_one_shot_stop();

        }

        /* Re-enable H-bridge and disable UART if the cutout was active.
         * The H-bridge is tristated once DELAY expires, i.e. in any of
         * SETTLING / CH1 / GAP / CH2. */
        if (context->state == DCC_RAILCOM_CUTOUT_SETTLING ||
            context->state == DCC_RAILCOM_CUTOUT_CH1 ||
            context->state == DCC_RAILCOM_CUTOUT_GAP ||
            context->state == DCC_RAILCOM_CUTOUT_CH2) {

            if (context->interface->uart_rx_disable) {

                context->interface->uart_rx_disable();

            }

            if (context->interface->end_railcom_cutout) {

                context->interface->end_railcom_cutout();

            }

        }

        context->state = DCC_RAILCOM_CUTOUT_IDLE;

    }

}

void DccRailcomCutout_timer_isr(dcc_railcom_cutout_context_t *context) {

    if (!context->interface) {

        return;

    }

    switch (context->state) {

        case DCC_RAILCOM_CUTOUT_DELAY:

            /* DELAY expired (T_CS) — tristate H-bridge / begin cutout.
             * Load SETTLING duration. */
            if (context->interface->begin_railcom_cutout) {

                context->interface->begin_railcom_cutout();

            }

            context->state = DCC_RAILCOM_CUTOUT_SETTLING;

            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(context->uart_rx_delay_us);

            }

            break;

        case DCC_RAILCOM_CUTOUT_SETTLING:

            /* SETTLING expired (T_TS1) — enable UART Rx (Ch1 opens).
             * Load CH1 window duration. */
            if (context->interface->uart_rx_enable) {

                context->interface->uart_rx_enable();

            }

            context->state = DCC_RAILCOM_CUTOUT_CH1;

            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(context->ch1_window_us);

            }

            break;

        case DCC_RAILCOM_CUTOUT_CH1:

            /* CH1 expired (T_TC1) — disable UART Rx (Ch1 closes).
             * Load GAP duration. */
            if (context->interface->uart_rx_disable) {

                context->interface->uart_rx_disable();

            }

            context->state = DCC_RAILCOM_CUTOUT_GAP;

            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(context->ch1_ch2_gap_us);

            }

            break;

        case DCC_RAILCOM_CUTOUT_GAP:

            /* GAP expired (T_TS2) — re-enable UART Rx (Ch2 opens).
             * Load CH2 window duration. */
            if (context->interface->uart_rx_enable) {

                context->interface->uart_rx_enable();

            }

            context->state = DCC_RAILCOM_CUTOUT_CH2;

            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(context->ch2_window_us);

            }

            break;

        case DCC_RAILCOM_CUTOUT_CH2:

            /* CH2 expired (T_CE) — disable UART Rx, restore H-bridge,
             * cutout complete. */
            if (context->interface->uart_rx_disable) {

                context->interface->uart_rx_disable();

            }

            if (context->interface->end_railcom_cutout) {

                context->interface->end_railcom_cutout();

            }

            context->state = DCC_RAILCOM_CUTOUT_IDLE;

            if (context->interface->on_cutout_complete) {

                context->interface->on_cutout_complete();

            }

            break;

        case DCC_RAILCOM_CUTOUT_IDLE:
        default:

            /* Spurious ISR — ignore */
            break;

    }

}

bool DccRailcomCutout_is_idle(const dcc_railcom_cutout_context_t *context) {

    return (context->state == DCC_RAILCOM_CUTOUT_IDLE);

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_COMMAND_STATION */
