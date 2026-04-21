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
 * @date 13 Apr 2026
 */

#include "dcc_railcom_cutout.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Ch2 window duration = total cutout - Ch1 window. */
#define RAILCOM_CH2_WINDOW_US \
    (DCC_RAILCOM_CUTOUT_TOTAL_US - DCC_RAILCOM_CH1_WINDOW_US)

void DccRailcomCutout_initialize(dcc_railcom_cutout_context_t *context, const interface_dcc_railcom_cutout_t *interface) {

    context->interface = interface;
    context->state = DCC_RAILCOM_CUTOUT_IDLE;

}

void DccRailcomCutout_begin(dcc_railcom_cutout_context_t *context) {

    if (!context->interface) {

        return;

    }

    context->state = DCC_RAILCOM_CUTOUT_DELAY;

    if (context->interface->timer_one_shot_start) {

        context->interface->timer_one_shot_start(DCC_RAILCOM_CUTOUT_START_US);

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

        /* Re-enable H-bridge and disable UART if cutout was active */
        if (context->state == DCC_RAILCOM_CUTOUT_CH1 ||
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

            /* 88us elapsed — begin cutout: disable H-bridge, enable UART rx */
            if (context->interface->begin_railcom_cutout) {

                context->interface->begin_railcom_cutout();

            }

            if (context->interface->uart_rx_enable) {

                context->interface->uart_rx_enable();

            }

            context->state = DCC_RAILCOM_CUTOUT_CH1;

            /* Reprogram timer for Ch1 window duration */
            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(DCC_RAILCOM_CH1_WINDOW_US);

            }

            break;

        case DCC_RAILCOM_CUTOUT_CH1:

            /* 464us elapsed — Ch1 window closed, Ch2 window opens.
             * UART rx stays enabled across both channels. */
            context->state = DCC_RAILCOM_CUTOUT_CH2;

            /* Reprogram timer for remaining cutout (Ch2 window) */
            if (context->interface->timer_one_shot_start) {

                context->interface->timer_one_shot_start(RAILCOM_CH2_WINDOW_US);

            }

            break;

        case DCC_RAILCOM_CUTOUT_CH2:

            /* 1544us total elapsed — cutout complete */
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

#endif /* DCC_COMPILE_COMMAND_STATION */
