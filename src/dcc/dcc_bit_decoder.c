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
 * @date 27 Jun 2026
 */

#include "dcc_bit_decoder.h"

#ifdef DCC_COMPILE_DECODER

// =============================================================================
// Internal types
// =============================================================================

typedef enum {

    HALF_NONE,
    HALF_SHORT,
    HALF_LONG

} half_type_enum;

typedef enum {

    DECODE_SEEKING_PREAMBLE,
    DECODE_ACCUMULATING,
    DECODE_SEPARATOR

} decode_state_enum;

// =============================================================================
// Static state
// =============================================================================

static const interface_dcc_bit_decoder_t *_interface;

static uint32_t _last_edge_usec;
static bool _first_edge;
static half_type_enum _first_half_type;

static decode_state_enum _state;
static uint8_t _preamble_count;
static uint8_t _packet_buffer[DCC_PACKET_MAX_BYTES];
static uint8_t _current_byte;
static uint8_t _bit_count;
static uint8_t _byte_count;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Reset the packet assembler to seek a new preamble.
     */
static void _reset_to_preamble(void) {

    _state = DECODE_SEEKING_PREAMBLE;
    _preamble_count = 0;
    _bit_count = 0;
    _byte_count = 0;
    _current_byte = 0;

}

    /**
     * @brief Handle a decoded bit while seeking a preamble.
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit_seeking_preamble(bool is_one) {

    if (is_one) {

        _preamble_count++;

    } else {

        if (_preamble_count >= DCC_PREAMBLE_BITS_DECODER_MIN) {

            /* Zero after valid preamble = start bit of first byte */
            _state = DECODE_ACCUMULATING;
            _bit_count = 0;
            _byte_count = 0;
            _current_byte = 0;

        } else {

            _preamble_count = 0;

        }

    }

}

    /**
     * @brief Handle a decoded bit while waiting for a separator or end bit.
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit_separator(bool is_one) {

    if (is_one) {

        /* End bit — packet complete */
        if (_byte_count >= 2 && _interface->on_packet_received) {

            _interface->on_packet_received(_packet_buffer, _byte_count);

        }

        /* The end bit also counts as first preamble bit */
        _state = DECODE_SEEKING_PREAMBLE;
        _preamble_count = 1;

    } else {

        /* Zero = start bit of next byte */
        if (_byte_count >= DCC_PACKET_MAX_BYTES) {

            /* Packet too long, discard */
            _reset_to_preamble();

        } else {

            _state = DECODE_ACCUMULATING;
            _bit_count = 0;
            _current_byte = 0;

        }

    }

}

    /**
     * @brief Process a complete decoded bit (one or zero).
     * @param is_one true if the bit is a one-bit, false if zero-bit.
     */
static void _on_bit(bool is_one) {

    if (_state == DECODE_SEEKING_PREAMBLE) {

        _on_bit_seeking_preamble(is_one);

    } else if (_state == DECODE_ACCUMULATING) {

        /* Shift bit into current byte, MSB first */
        _current_byte = (_current_byte << 1) | (is_one ? 1 : 0);
        _bit_count++;

        if (_bit_count >= 8) {

            _packet_buffer[_byte_count] = _current_byte;
            _byte_count++;

#if defined(DCC_COMPILE_RAILCOM)
            /* Emit the byte the instant it completes -- before the next byte arrives (so the
             * last data byte fires before the XOR) for the RailCom Tx command-recognizer. */
            if (_interface->on_byte_received) {

                _interface->on_byte_received(_packet_buffer, _byte_count);

            }
#endif /* DCC_COMPILE_RAILCOM */

            _state = DECODE_SEPARATOR;

        }

    } else {

        _on_bit_separator(is_one);

    }

}

// =============================================================================
// Public API
// =============================================================================

void DccBitDecoder_initialize(const interface_dcc_bit_decoder_t *interface) {

    _interface = interface;
    _last_edge_usec = 0;
    _first_edge = true;
    _first_half_type = HALF_NONE;
    _reset_to_preamble();

}

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
        _first_half_type = HALF_NONE;
        _reset_to_preamble();
        return;

    }

    if (elapsed < DCC_DECODER_HALF_BIT_THRESHOLD_US) {

        this_half = HALF_SHORT;

    } else {

        this_half = HALF_LONG;

    }

    /* Pair half-bits */
    if (_first_half_type == HALF_NONE) {

        _first_half_type = this_half;

    } else if (_first_half_type == this_half) {

        /* Matching pair — emit a bit */
        _on_bit(this_half == HALF_SHORT);
        _first_half_type = HALF_NONE;

    } else {

        /* Mismatch — treat this half as start of new pair */
        _first_half_type = this_half;

    }

}

#endif /* DCC_COMPILE_DECODER */
