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
 * @file dcc_bit_encoder.c
 * @brief ISR-level bit encoder for DCC packet transmission.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "dcc_bit_encoder.h"

#ifdef DCC_COMPILE_COMMAND_STATION

void DccBitEncoder_initialize(dcc_bit_encoder_context_t *context, const interface_dcc_bit_encoder_t *interface) {

    context->interface = interface;
    context->state = DCC_BIT_STATE_IDLE;
    context->packet_loaded = false;
    context->running = false;
    context->half_bit = 0;
    context->tick_counter = 0;
    context->current_bit_is_one = true;
    context->cutout_complete = false;
    context->toggle_next = false;

}

void DccBitEncoder_load_packet(dcc_bit_encoder_context_t *context, const dcc_packet_t *packet) {

    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count && byte_index < DCC_PACKET_MAX_BYTES; byte_index++) {

        context->active_packet.data[byte_index] = packet->data[byte_index];

    }

    context->active_packet.byte_count = packet->byte_count;
    context->active_packet.preamble_bits = packet->preamble_bits;
    context->active_packet.repeat_count = packet->repeat_count;

    context->packet_loaded = true;

}

bool DccBitEncoder_is_idle(const dcc_bit_encoder_context_t *context) {

    return (context->state == DCC_BIT_STATE_IDLE);

}

void DccBitEncoder_start(dcc_bit_encoder_context_t *context) {

    context->running = true;
    context->state = DCC_BIT_STATE_IDLE;
    context->half_bit = 0;
    context->toggle_next = true;

}

void DccBitEncoder_stop(dcc_bit_encoder_context_t *context) {

    context->running = false;
    context->state = DCC_BIT_STATE_IDLE;

}

/* =========================================================================
 * Fixed-period tick ISR (shared-timer architecture)
 *
 * Called every 58us. One-bits toggle every tick, zero-bits skip one tick
 * and toggle on the second. Uses pin_toggle() instead of timer_set_period().
 * ========================================================================= */

    /**
     * @brief Set the ticks-per-half for the next bit.
     * @param context Pointer to the instance context.
     * @param is_one_bit true for one-bit (1 tick/half), false for zero-bit (2 ticks/half).
     */
static void _set_bit_type(dcc_bit_encoder_context_t *context, bool is_one_bit) {

    context->current_bit_is_one = is_one_bit;

}

    /**
     * @brief Handle cutout completion for the tick ISR.
     * @param context Pointer to the instance context.
     */
static void _tick_handle_cutout_complete(dcc_bit_encoder_context_t *context) {

    context->cutout_complete = false;
    context->packet_loaded = false;
    context->state = DCC_BIT_STATE_IDLE;
    context->tick_counter = 0;
    context->half_bit = 0;
    _set_bit_type(context, true);

    if (context->interface->on_packet_complete) {

        context->interface->on_packet_complete();

    }

}

    /**
     * @brief Handle DATA state for the tick ISR.
     * @param context Pointer to the instance context.
     */
static void _tick_handle_data(dcc_bit_encoder_context_t *context) {

    if (context->bit_index == 0) {

        context->byte_index++;
        if (context->byte_index >= context->active_packet.byte_count) {

            context->state = DCC_BIT_STATE_END_BIT;
            _set_bit_type(context, true);

        } else {

            context->state = DCC_BIT_STATE_START_BIT;
            _set_bit_type(context, false);

        }

    } else {

        context->bit_index--;
        _set_bit_type(context, (context->active_packet.data[context->byte_index] >> context->bit_index) & 0x01);

    }

}

    /**
     * @brief Handle END_BIT state for the tick ISR.
     * @param context Pointer to the instance context.
     */
static void _tick_handle_end_bit(dcc_bit_encoder_context_t *context) {

    if (context->interface->railcom_cutout_begin) {

        context->state = DCC_BIT_STATE_RAILCOM_CUTOUT;
        context->interface->railcom_cutout_begin();

    } else {

        context->packet_loaded = false;
        context->state = DCC_BIT_STATE_IDLE;

        if (context->interface->on_packet_complete) {

            context->interface->on_packet_complete();

        }
        _set_bit_type(context, true);

    }

}

void DccBitEncoder_tick_isr(dcc_bit_encoder_context_t *context) {

    /* Pin toggle has already been performed by the caller using
     * toggle_next. This function only advances the state machine
     * and computes toggle_next for the next tick. */

    if (!context->interface || !context->running) {

        context->toggle_next = false;
        return;

    }

    /* ---- RailCom cutout: wait for Timer 2 to signal completion ---- */
    if (context->state == DCC_BIT_STATE_RAILCOM_CUTOUT) {

        if (context->cutout_complete) {

            _tick_handle_cutout_complete(context);

        }

        /* Do not toggle pin during cutout — H-bridge is off. */
        context->toggle_next = false;
        return;

    }

    /* ---- Tick counting: decide whether this was a half-bit boundary ---- */
    context->tick_counter++;

    if (context->current_bit_is_one) {

        /* One-bit: toggle every tick (1 tick per half-bit) */
        if (context->tick_counter < 1) {

            context->toggle_next = false;
            return;

        }

    } else {

        /* Zero-bit: toggle every 2 ticks (2 ticks per half-bit) */
        if (context->tick_counter < 2) {

            context->toggle_next = false;
            return;

        }

    }

    /* Half-bit boundary reached — reset tick counter */
    context->tick_counter = 0;

    /* ---- Half-bit counting: advance state on second half ---- */
    context->half_bit++;

    if (context->half_bit < 2) {

        /* First half of bit — next tick needs a toggle for second half */
        context->toggle_next = true;
        return;

    }

    /* Full bit complete — advance state machine */
    context->half_bit = 0;

    switch (context->state) {

        case DCC_BIT_STATE_IDLE:

            if (context->packet_loaded) {

                context->preamble_count = context->active_packet.preamble_bits;
                context->state = DCC_BIT_STATE_PREAMBLE;
                _set_bit_type(context, true);

            } else {

                _set_bit_type(context, true);

            }

            break;

        case DCC_BIT_STATE_PREAMBLE:

            context->preamble_count--;
            if (context->preamble_count == 0) {

                context->state = DCC_BIT_STATE_START_BIT;
                context->byte_index = 0;
                _set_bit_type(context, false);

            } else {

                _set_bit_type(context, true);

            }

            break;

        case DCC_BIT_STATE_START_BIT:

            context->state = DCC_BIT_STATE_DATA;
            context->bit_index = 7;
            _set_bit_type(context, (context->active_packet.data[context->byte_index] >> context->bit_index) & 0x01);

            break;

        case DCC_BIT_STATE_DATA:

            _tick_handle_data(context);

            break;

        case DCC_BIT_STATE_END_BIT:

            _tick_handle_end_bit(context);

            break;

        default:

            context->state = DCC_BIT_STATE_IDLE;
            _set_bit_type(context, true);

            break;

    }

    /* After every full-bit state transition, the next bit always
     * starts with a first-half toggle — whether it is idle one-bits,
     * preamble, start bit, data, or end bit. */
    context->toggle_next = true;

}

#endif /* DCC_COMPILE_COMMAND_STATION */
