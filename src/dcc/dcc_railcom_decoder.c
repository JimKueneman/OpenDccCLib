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
 * @file dcc_railcom_decoder.c
 * @brief RailCom 4/8 decoding, cutout management, and receive buffer.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#include "dcc_railcom_decoder.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#include <string.h>

// =============================================================================
// RailCom 4/8 Decode Table (NMRA RP-9.3.2)
// =============================================================================

static const uint8_t _decode_table[256] = {
    /* 0x00-0x0F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD,
    /* 0x10-0x1F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x20-0x2F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x37, 0x35, 0x26, 0xFF,
    /* 0x30-0x3F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1C, 0x1D, 0xFF,
    0xFF, 0xFF, 0x38, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x40-0x4F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x1A, 0x36, 0x30, 0xFF, 0x25, 0x18, 0xFF,
    /* 0x50-0x5F */
    0xFF, 0xFF, 0xFF, 0x2F, 0xFF, 0x16, 0x1E, 0xFF,
    0xFF, 0x19, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x60-0x6F */
    0xFF, 0xFF, 0xFF, 0x39, 0xFF, 0x06, 0x1F, 0xFF,
    0xFF, 0x10, 0x1B, 0xFF, 0x2E, 0xFF, 0xFF, 0xFF,
    /* 0x70-0x7F */
    0xFF, 0x29, 0x2C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x80-0x8F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x28, 0xFF, 0x23, 0x32, 0xFF,
    /* 0x90-0x9F */
    0xFF, 0xFF, 0xFF, 0x17, 0xFF, 0x08, 0x0F, 0xFF,
    0xFF, 0x0D, 0x2B, 0xFF, 0x3B, 0xFF, 0xFF, 0xFF,
    /* 0xA0-0xAF */
    0xFF, 0xFF, 0xFF, 0x03, 0xFF, 0x13, 0x0E, 0xFF,
    0xFF, 0x0B, 0x15, 0x01, 0x00, 0xFF, 0xFF, 0xFF,
    /* 0xB0-0xBF */
    0xFF, 0x2D, 0x33, 0x07, 0x14, 0x27, 0x3D, 0xFF,
    0x3C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0xC0-0xCF */
    0xFF, 0xFF, 0xFF, 0x12, 0x3F, 0x04, 0x2A, 0xFF,
    0xFF, 0x0A, 0x05, 0x24, 0x22, 0xFF, 0xFF, 0xFF,
    /* 0xD0-0xDF */
    0xFF, 0xFF, 0x3E, 0x02, 0x34, 0x09, 0x0C, 0xFF,
    0xFF, 0x21, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0xE0-0xEF */
    0xFF, 0xFF, 0xFF, 0x31, 0xFF, 0x11, 0x3A, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0xF0-0xFF */
    0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// =============================================================================
// Static helpers
// =============================================================================

static void _buffer_push(dcc_railcom_decoder_context_t *context, const dcc_railcom_datagram_t *datagram) {

    if (context->buffer_count >= USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH) {

        context->buffer_tail = (context->buffer_tail + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
        context->buffer_count--;

    }

    memcpy(&context->buffer[context->buffer_head], datagram, sizeof(dcc_railcom_datagram_t));
    context->buffer_head = (context->buffer_head + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
    context->buffer_count++;

}

    /**
     * @brief Decode RailCom Channel 1 (first 2 bytes → 12 data bits).
     * @param context Decoder context.
     * @param raw_bytes Raw UART bytes from cutout.
     * @param raw_count Number of raw bytes received.
     */
static void _decode_channel_1(dcc_railcom_decoder_context_t *context, const uint8_t *raw_bytes, uint8_t raw_count) {

    uint8_t decoded_0;
    uint8_t decoded_1;
    uint16_t combined;
    dcc_railcom_datagram_t datagram;

    if (raw_count < DCC_RAILCOM_CH1_MAX_BYTES) {

        return;

    }

    decoded_0 = _decode_table[raw_bytes[0]];
    decoded_1 = _decode_table[raw_bytes[1]];

    if (decoded_0 >= 0x40 || decoded_1 >= 0x40) {

        return;

    }

    combined = ((uint16_t)decoded_0 << 6) | decoded_1;

    memset(&datagram, 0, sizeof(dcc_railcom_datagram_t));
    datagram.datagram_id = (uint8_t)((combined >> 8) & 0x0F);
    datagram.data[0] = (uint8_t)(combined & 0xFF);
    datagram.count = 1;
    datagram.valid = true;

    _buffer_push(context, &datagram);

    if (context->interface->on_datagram) {

        context->interface->on_datagram(context->cutout_address, DCC_RAILCOM_CH1, &datagram);

    }

}

    /**
     * @brief Decode RailCom Channel 2 (remaining bytes, up to 6 → up to 36 data bits).
     * @param context Decoder context.
     * @param raw_bytes Raw UART bytes from cutout.
     * @param raw_count Number of raw bytes received.
     */
static void _decode_channel_2(dcc_railcom_decoder_context_t *context, const uint8_t *raw_bytes, uint8_t raw_count) {

    uint8_t ch2_start = DCC_RAILCOM_CH1_MAX_BYTES;
    uint8_t ch2_count;
    uint8_t decoded_bytes[DCC_RAILCOM_CH2_MAX_BYTES];
    uint8_t valid_count = 0;
    uint8_t byte_index;
    uint8_t decoded_value;
    bool all_valid = true;
    uint16_t combined;
    dcc_railcom_datagram_t datagram;

    if (raw_count <= DCC_RAILCOM_CH1_MAX_BYTES) {

        return;

    }

    ch2_count = raw_count - ch2_start;

    for (byte_index = 0; byte_index < ch2_count && byte_index < DCC_RAILCOM_CH2_MAX_BYTES; byte_index++) {

        decoded_value = _decode_table[raw_bytes[ch2_start + byte_index]];

        if (decoded_value >= 0x40) {

            all_valid = false;
            break;

        }

        decoded_bytes[valid_count] = decoded_value;
        valid_count++;

    }

    if (!all_valid || valid_count < 2) {

        return;

    }

    combined = ((uint16_t)decoded_bytes[0] << 6) | decoded_bytes[1];

    memset(&datagram, 0, sizeof(dcc_railcom_datagram_t));
    datagram.datagram_id = (uint8_t)((combined >> 8) & 0x0F);
    datagram.data[0] = (uint8_t)(combined & 0xFF);
    datagram.count = 1;

    for (byte_index = 2; byte_index < valid_count && datagram.count < DCC_RAILCOM_DATAGRAM_MAX_BYTES; byte_index++) {

        datagram.data[datagram.count] = decoded_bytes[byte_index];
        datagram.count++;

    }

    datagram.valid = true;

    _buffer_push(context, &datagram);

    if (context->interface->on_datagram) {

        context->interface->on_datagram(context->cutout_address, DCC_RAILCOM_CH2, &datagram);

    }

}

static void _process_cutout(dcc_railcom_decoder_context_t *context) {

    uint8_t raw_bytes[DCC_RAILCOM_CH1_MAX_BYTES + DCC_RAILCOM_CH2_MAX_BYTES];
    uint8_t raw_count = 0;

    while (raw_count < (DCC_RAILCOM_CH1_MAX_BYTES + DCC_RAILCOM_CH2_MAX_BYTES)) {

        if (!context->interface->uart_read(&raw_bytes[raw_count])) {

            break;

        }

        raw_count++;

    }

    if (raw_count == 0) {

        return;

    }

    _decode_channel_1(context, raw_bytes, raw_count);
    _decode_channel_2(context, raw_bytes, raw_count);

}

// =============================================================================
// Public API
// =============================================================================

void DccRailcomDecoder_initialize(dcc_railcom_decoder_context_t *context, const interface_dcc_railcom_decoder_t *interface) {

    context->interface = interface;
    context->buffer_head = 0;
    context->buffer_tail = 0;
    context->buffer_count = 0;
    context->cutout_address = 0;
    context->cutout_pending = false;

}

void DccRailcomDecoder_run(dcc_railcom_decoder_context_t *context) {

    if (!context->interface->uart_read) {

        return;

    }

    if (!context->cutout_pending) {

        return;

    }

    context->cutout_pending = false;
    _process_cutout(context);

}

void DccRailcomDecoder_begin_cutout(dcc_railcom_decoder_context_t *context, dcc_address_t address) {

    context->cutout_address = address;
    context->cutout_pending = true;

}

void DccRailcomDecoder_end_cutout(dcc_railcom_decoder_context_t *context) {

    /* Intentionally empty — processing happens in run() after cutout ends. */
    (void)context;

}

bool DccRailcomDecoder_read(dcc_railcom_decoder_context_t *context, dcc_railcom_datagram_t *datagram) {

    if (context->buffer_count == 0) {

        return false;

    }

    memcpy(datagram, &context->buffer[context->buffer_tail], sizeof(dcc_railcom_datagram_t));
    context->buffer_tail = (context->buffer_tail + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
    context->buffer_count--;

    return true;

}

uint8_t DccRailcomDecoder_available(const dcc_railcom_decoder_context_t *context) {

    return context->buffer_count;

}

uint8_t DccRailcomDecoder_decode_byte(uint8_t encoded) {

    return _decode_table[encoded];

}

#endif /* DCC_COMPILE_COMMAND_STATION */
