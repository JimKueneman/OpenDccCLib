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
 * @date 25 Sep 2026
 */

#include "dcc_bit_encoder.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /**
     * @brief Initialize the bit encoder module.
     *
     * @details Stores the interface and puts the encoder in IDLE, not running,
     * with no packet loaded and no cutout arm pending. The first bit after
     * DccBitEncoder_start() is treated as a one-bit.
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @param interface Pointer to populated interface_dcc_bit_encoder_t struct.
     * @endverbatim
     */
void DccBitEncoder_initialize(dcc_bit_encoder_context_t *context, const interface_dcc_bit_encoder_t *interface) {

    context->interface = interface;
    context->state = DCC_BIT_STATE_IDLE;
    context->packet_loaded = false;
    context->running = false;
    context->half_bit = 0;
    context->tick_counter = 0;
    context->current_bit_is_one = true;
    context->toggle_next = false;
    context->railcom_cutout_arm_pending = false;

}

    /**
     * @brief Load a new packet for transmission.
     *
     * @details Copies the data bytes (capped at DCC_PACKET_MAX_BYTES),
     * byte_count, preamble_bits and repeat_count into active_packet, then
     * sets packet_loaded behind a DCC_COMPILER_BARRIER() so the ISR can never
     * see the flag before every byte is stored. The IDLE state of the tick ISR
     * picks the packet up at the next full-bit boundary.
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @param packet Pointer to dcc_packet_t to transmit. Contents are copied.
     * @endverbatim
     *
     * @warning Call only while DccBitEncoder_is_idle() is true; there is no back buffer.
     */
void DccBitEncoder_load_packet(dcc_bit_encoder_context_t *context, const dcc_packet_t *packet) {

    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count && byte_index < DCC_PACKET_MAX_BYTES; byte_index++) {

        context->active_packet.data[byte_index] = packet->data[byte_index];

    }

    context->active_packet.byte_count = packet->byte_count;
    context->active_packet.preamble_bits = packet->preamble_bits;
    context->active_packet.repeat_count = packet->repeat_count;

    /* Publish only after every byte of the packet is stored. */
    DCC_COMPILER_BARRIER();
    context->packet_loaded = true;

}

    /**
     * @brief Check if the bit encoder has finished transmitting its current packet.
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @endverbatim
     *
     * @return true if the state machine is IDLE, false while any part of a packet is being sent.
     */
bool DccBitEncoder_is_idle(const dcc_bit_encoder_context_t *context) {

    return (context->state == DCC_BIT_STATE_IDLE);

}

    /**
     * @brief Start the bit encoder. Begins generating DCC signal.
     *
     * @details Sets running, resets to IDLE at the first half of a bit and
     * requests a toggle on the next tick, so the line clocks idle one-bits
     * until a packet is loaded. Any pending cutout arm is dropped.
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @endverbatim
     */
void DccBitEncoder_start(dcc_bit_encoder_context_t *context) {

    context->running = true;
    context->state = DCC_BIT_STATE_IDLE;
    context->half_bit = 0;
    context->toggle_next = true;
    context->railcom_cutout_arm_pending = false;

}

    /**
     * @brief Stop the bit encoder. Halts DCC signal generation.
     *
     * @details Clears running and forces IDLE; the next tick leaves the pin
     * where it is. A packet in flight is abandoned and packet_loaded is left
     * as is.
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @endverbatim
     */
void DccBitEncoder_stop(dcc_bit_encoder_context_t *context) {

    context->running = false;
    context->state = DCC_BIT_STATE_IDLE;
    context->railcom_cutout_arm_pending = false;

}

/* =========================================================================
 * Fixed-period tick ISR (shared-timer architecture)
 *
 * Called every 58us. One-bits toggle every tick, zero-bits skip one tick
 * and toggle on the second. The caller drives the pin through pin_toggle()
 * whenever toggle_next is set.
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
     * @brief Handle DATA state for the tick ISR.
     *
     * @details Called when a data bit has completed. Moves to the next lower
     * bit of the current byte, or when the byte is exhausted to the start bit
     * of the next byte, or to the end bit after the last byte.
     *
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
     *
     * @details Called when the end bit has completed on the encoder's side
     * (one half-bit ahead of the wire). Defers the RailCom cutout arm to the
     * next tick, releases the packet to the main loop (packet_loaded = false
     * behind a DCC_COMPILER_BARRIER()), returns to IDLE and fires
     * on_packet_complete.
     *
     * @param context Pointer to the instance context.
     */
static void _tick_handle_end_bit(dcc_bit_encoder_context_t *context) {

    /* Continuous-clock RailCom: when the output stage is RailCom-capable
     * (railcom_cutout_begin wired), arm the cutout timer, which emits the
     * cutout-active strobe (T_CS..T_CE). The encoder does NOT stall -- it completes
     * the packet and keeps clocking the next preamble, so the DCC bit stream is
     * continuous and identical with or without RailCom. The application/hardware
     * decides whether to act on the strobe (mux the H-bridge into the cutout). The
     * 16-bit ops preamble (DCC_PREAMBLE_BITS_OPS) covers the cutout window.
     *
     * NOT armed here: this handler runs on the tick that STARTS the end bit's
     * second half, one half-bit before the end bit's last edge. S-9.3.2 Table 1
     * measures T_CS (26-32 us) from the zero crossing of that last edge, so
     * arming from here would tri-state the H-bridge inside the end bit and
     * truncate its second half (issue #3). The arm is deferred to the next
     * tick, whose pin toggle IS the last edge -- see DccBitEncoder_tick_isr(). */
    if (context->interface->railcom_cutout_begin) {

        context->railcom_cutout_arm_pending = true;

    }

    /* All reads of the finished packet are done; release it to the main loop. */
    DCC_COMPILER_BARRIER();
    context->packet_loaded = false;
    context->state = DCC_BIT_STATE_IDLE;

    if (context->interface->on_packet_complete) {

        context->interface->on_packet_complete();

    }
    _set_bit_type(context, true);

}

    /**
     * @brief Fixed-period tick ISR entry point — call every 58us from shared timer.
     *
     * @details Algorithm:
     * -# If no interface or not running, clear toggle_next and return
     * -# If a cutout arm is pending, clear it and call railcom_cutout_begin(): the
     *    toggle the caller just performed was the end bit's last edge, so T_CS is
     *    measured from that edge (S-9.3.2 Table 1)
     * -# Count the tick; a one-bit half is 1 tick, a zero-bit half is 2. If the
     *    half-bit is not over yet, clear toggle_next and return
     * -# Half-bit over: reset the tick counter. If this was the first half, set
     *    toggle_next and return
     * -# Full bit over: advance the state machine
     *    - IDLE: if packet_loaded (read after a DCC_COMPILER_BARRIER()), load
     *      preamble_count and go to PREAMBLE; either way the next bit is a one
     *    - PREAMBLE: count down; at 0 go to START_BIT (a zero) with byte_index 0
     *    - START_BIT: go to DATA at bit 7 of the current byte
     *    - DATA / END_BIT: see _tick_handle_data() / _tick_handle_end_bit()
     * -# Set toggle_next: every new bit starts with a first-half toggle
     *
     * @verbatim
     * @param context Pointer to dcc_bit_encoder_context_t instance.
     * @endverbatim
     */
void DccBitEncoder_tick_isr(dcc_bit_encoder_context_t *context) {

    /* Pin toggle has already been performed by the caller using
     * toggle_next. This function only advances the state machine
     * and computes toggle_next for the next tick. */

    if (!context->interface || !context->running) {

        context->toggle_next = false;
        return;

    }

    /* Continuous-clock RailCom: there is no cutout WAIT state. The encoder keeps
     * clocking through the cutout window; the driver blanks its own output between
     * the begin/end hooks (T_CS..T_CE). So the bit stream never pauses here. */

    /* ---- Deferred cutout arm (see _tick_handle_end_bit) ----
     * The pin toggle the caller performed just before this call drove the
     * packet end bit's last edge. Arm the cutout timer now so its DELAY
     * (T_CS) is measured from that edge, as S-9.3.2 Table 1 requires. */
    if (context->railcom_cutout_arm_pending) {

        context->railcom_cutout_arm_pending = false;

        if (context->interface->railcom_cutout_begin) {

            context->interface->railcom_cutout_begin();

        }

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

                /* Read the packet only after the flag has been seen. */
                DCC_COMPILER_BARRIER();
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
