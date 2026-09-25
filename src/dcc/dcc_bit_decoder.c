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
 * @file dcc_bit_decoder.c
 * @brief Edge timestamp to bit classification, preamble detection, and byte
 * assembly for DCC decoder.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_bit_decoder.h"

#ifdef DCC_COMPILE_DECODER

// =============================================================================
// Internal types
// =============================================================================

    /** @brief Classification of one half-period (the time between two edges). */
typedef enum {

    DCC_HALF_NONE,      /**< No half-period pending: the next edge starts a new pair */
    DCC_HALF_SHORT,     /**< Under DCC_DECODER_HALF_BIT_THRESHOLD_US: one half of a one-bit */
    DCC_HALF_LONG       /**< At or above the threshold (and under the maximum): one half of a zero-bit */

} half_type_enum;

    /** @brief Packet assembler state. */
typedef enum {

    DCC_DECODE_SEEKING_PREAMBLE,    /**< Counting consecutive one-bits until DCC_PREAMBLE_BITS_DECODER_MIN are seen */
    DCC_DECODE_ACCUMULATING,        /**< Shifting the 8 data bits of a byte in, MSB first */
    DCC_DECODE_SEPARATOR            /**< Byte stored: a zero is the next start bit, a one is the packet end bit */

} decode_state_enum;

// =============================================================================
// Static state
// =============================================================================

    /** @brief Injected callbacks; set by DccBitDecoder_initialize. */
static const interface_dcc_bit_decoder_t *_interface;

    /** @brief Timestamp of the previous edge: the start of the half-period being measured. */
static uint32_t _last_edge_usec;
    /** @brief True until an edge has been captured; that edge only sets the baseline and yields no half-period. */
static bool _first_edge;
    /** @brief Classification of the first half of the bit being paired, DCC_HALF_NONE when no half is pending. */
static half_type_enum _first_half_type;

    /** @brief Packet assembler state. */
static decode_state_enum _state;
    /** @brief Consecutive one-bits seen while seeking the preamble. */
static uint8_t _preamble_count;
    /** @brief Bytes of the packet being assembled, including the XOR byte once received. */
static uint8_t _packet_buffer[DCC_PACKET_MAX_BYTES];
    /** @brief Byte being shifted in, MSB first. */
static uint8_t _current_byte;
    /** @brief Bits shifted into _current_byte so far (0-8). */
static uint8_t _bit_count;
    /** @brief Bytes stored in _packet_buffer so far. */
static uint8_t _byte_count;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Reset the packet assembler to seek a new preamble.
     */
static void _reset_to_preamble(void) {

    _state = DCC_DECODE_SEEKING_PREAMBLE;
    _preamble_count = 0;
    _bit_count = 0;
    _byte_count = 0;
    _current_byte = 0;

}

    /**
     * @brief Handle a decoded bit while seeking a preamble.
     *
     * @details Counts consecutive one-bits. A zero-bit after at least
     * DCC_PREAMBLE_BITS_DECODER_MIN ones is the start bit of the first byte and switches
     * the assembler to accumulating; a zero-bit before that restarts the count.
     *
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit_seeking_preamble(bool is_one) {

    if (is_one) {

        _preamble_count++;

    } else {

        if (_preamble_count >= DCC_PREAMBLE_BITS_DECODER_MIN) {

            /* Zero after valid preamble = start bit of first byte */
            _state = DCC_DECODE_ACCUMULATING;
            _bit_count = 0;
            _byte_count = 0;
            _current_byte = 0;

        } else {

            _preamble_count = 0;

        }

    }

}

    /**
     * @brief Packet end bit: hand the assembled packet to the interface and re-arm the
     *  preamble search.
     *
     * @details A packet of fewer than 2 bytes is discarded. With DCC_COMPILE_RAILCOM the
     * assembler is reset and the next edge is marked as a fresh baseline BEFORE
     * on_packet_received fires, because that callback masks and unmasks the edge IRQ
     * for the cutout and a stale edge after the unmask must not land mid-packet; the end
     * bit is not counted as a preamble bit. Without RailCom the callback fires first and
     * the end bit counts as the first one-bit of the next preamble.
     */
static void _on_end_bit(void) {

#if defined(DCC_COMPILE_RAILCOM)
    /* RailCom: on_packet_received fires the Tx, which masks/unmasks the DCC edge IRQ
     * for the cutout. Reset the assembler to a clean preamble search with a skipped
     * first edge BEFORE that callback, so a queued/stale edge after unmask lands as the
     * discarded baseline -- not a mid-packet event. The end bit is NOT counted as a
     * preamble bit (the cutout gaps it from the next packet's preamble). */
    uint8_t saved_count = _byte_count;

    _reset_to_preamble();
    _first_edge = true;

    if (saved_count >= 2 && _interface->on_packet_received) {

        _interface->on_packet_received(_packet_buffer, saved_count);

    }
#else
    if (_byte_count >= 2 && _interface->on_packet_received) {

        _interface->on_packet_received(_packet_buffer, _byte_count);

    }

    /* The end bit also counts as first preamble bit */
    _state = DCC_DECODE_SEEKING_PREAMBLE;
    _preamble_count = 1;
#endif /* DCC_COMPILE_RAILCOM */

}

    /**
     * @brief Handle a decoded bit while waiting for a separator or end bit.
     *
     * @details A one-bit is the packet end bit and completes the packet. A zero-bit is
     * the start bit of the next byte, unless DCC_PACKET_MAX_BYTES bytes are already
     * stored, in which case the packet is too long and is discarded.
     *
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit_separator(bool is_one) {

    if (is_one) {

        /* End bit — packet complete */
        _on_end_bit();

    } else {

        /* Zero = start bit of next byte */
        if (_byte_count >= DCC_PACKET_MAX_BYTES) {

            /* Packet too long, discard */
            _reset_to_preamble();

        } else {

            _state = DCC_DECODE_ACCUMULATING;
            _bit_count = 0;
            _current_byte = 0;

        }

    }

}

    /**
     * @brief A full byte has been shifted in: store it and move to the separator-bit state.
     *
     * @details With DCC_COMPILE_RAILCOM the optional on_byte_received callback fires the
     * instant the byte is stored, before the next byte arrives, so the RailCom Tx command
     * recognizer sees the last data byte before the XOR byte.
     */
static void _on_byte_complete(void) {

    _packet_buffer[_byte_count] = _current_byte;
    _byte_count++;

#if defined(DCC_COMPILE_RAILCOM)
    /* Emit the byte the instant it completes -- before the next byte arrives (so the
     * last data byte fires before the XOR) for the RailCom Tx command-recognizer. */
    if (_interface->on_byte_received) {

        _interface->on_byte_received(_packet_buffer, _byte_count);

    }
#endif /* DCC_COMPILE_RAILCOM */

    _state = DCC_DECODE_SEPARATOR;

}

    /**
     * @brief Process a complete decoded bit (one or zero).
     *
     * @details Routes the bit by assembler state: preamble seeking, shifting into the
     * current byte (MSB first; the byte completes at 8 bits), or separator/end-bit handling.
     *
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit(bool is_one) {

    if (_state == DCC_DECODE_SEEKING_PREAMBLE) {

        _on_bit_seeking_preamble(is_one);

    } else if (_state == DCC_DECODE_ACCUMULATING) {

        /* Shift bit into current byte, MSB first */
        _current_byte = (_current_byte << 1) | (is_one ? 1 : 0);
        _bit_count++;

        if (_bit_count >= 8) {

            _on_byte_complete();

        }

    } else {

        _on_bit_separator(is_one);

    }

}

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the bit decoder module.
     *
     * @details Stores the interface, marks the next edge as the timing baseline, clears
     * the half-bit pairing and resets the assembler to a preamble search.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_bit_decoder_t.
     * @endverbatim
     */
void DccBitDecoder_initialize(const interface_dcc_bit_decoder_t *interface) {

    _interface = interface;
    _last_edge_usec = 0;
    _first_edge = true;
    _first_half_type = DCC_HALF_NONE;
    _reset_to_preamble();

}

    /**
     * @brief Process a signal edge from the input-capture ISR.
     *
     * @details Algorithm:
     * -# First edge after initialize (or after a RailCom end bit): record it as the
     *    baseline and return; it yields no half-period.
     * -# Compute the time elapsed since the previous edge (unsigned subtraction, so
     *    counter wraparound is tolerated) and record this edge as the new baseline.
     * -# Elapsed at or above DCC_DECODER_HALF_BIT_MAX_US: invalid; drop any pending
     *    half and restart the preamble search.
     * -# Classify the half-period: under DCC_DECODER_HALF_BIT_THRESHOLD_US is SHORT
     *    (one-bit), otherwise LONG (zero-bit).
     * -# Pair with the pending half: none pending stores this one; a matching pair emits
     *    a bit through _on_bit and clears the pending half; a mismatch does NOT restart
     *    the preamble search -- this half simply becomes the first half of a new pair.
     *
     * @verbatim
     * @param timestamp_usec Microsecond timestamp of the edge.
     * @endverbatim
     */
void DccBitDecoder_edge(uint32_t timestamp_usec) {

    uint32_t elapsed;
    half_type_enum this_half;

    if (_first_edge) {

        _last_edge_usec = timestamp_usec;
        _first_edge = false;
        return;

    }

    elapsed = timestamp_usec - _last_edge_usec;
    _last_edge_usec = timestamp_usec;

    /* Classify this half-period */
    if (elapsed >= DCC_DECODER_HALF_BIT_MAX_US) {

        /* Invalid — too long */
        _first_half_type = DCC_HALF_NONE;
        _reset_to_preamble();
        return;

    }

    if (elapsed < DCC_DECODER_HALF_BIT_THRESHOLD_US) {

        this_half = DCC_HALF_SHORT;

    } else {

        this_half = DCC_HALF_LONG;

    }

    /* Pair half-bits */
    if (_first_half_type == DCC_HALF_NONE) {

        _first_half_type = this_half;

    } else if (_first_half_type == this_half) {

        /* Matching pair — emit a bit */
        _on_bit(this_half == DCC_HALF_SHORT);
        _first_half_type = DCC_HALF_NONE;

    } else {

        /* Mismatch — treat this half as start of new pair */
        _first_half_type = this_half;

    }

}

#endif /* DCC_COMPILE_DECODER */
