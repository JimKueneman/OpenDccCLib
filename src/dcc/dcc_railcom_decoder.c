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
 * @brief RailCom 4/8 encoding and datagram transmission for decoders.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#include "dcc_railcom_decoder.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

// =============================================================================
// RailCom 4/8 Encode Table (NMRA RP-9.3.2)
// =============================================================================

    /**
     * @brief 4/8 encode table: maps 6-bit data value to 8-bit codeword.
     *
     * @details Each valid 8-bit codeword has exactly 4 ones and 4 zeros for
     * DC balance. Derived as the inverse of the decode table in
     * dcc_railcom_decoder.c.
     */
static const uint8_t _encode_table[64] = {
    /* 0x00-0x07 */
    0xAC, 0xAB, 0xD3, 0xA3, 0xC5, 0xCA, 0x65, 0xB3,
    /* 0x08-0x0F */
    0x95, 0xD5, 0xC9, 0xA9, 0xD6, 0x99, 0xA6, 0x96,
    /* 0x10-0x17 */
    0x69, 0xE5, 0xC3, 0xA5, 0xB4, 0xAA, 0x55, 0x93,
    /* 0x18-0x1F */
    0x4E, 0x59, 0x49, 0x6A, 0x35, 0x36, 0x56, 0x66,
    /* 0x20-0x27 */
    0x5A, 0xD9, 0xCC, 0x8D, 0xCB, 0x4D, 0x2E, 0xB5,
    /* 0x28-0x2F */
    0x8B, 0x71, 0xC6, 0x9A, 0x72, 0xB1, 0x6C, 0x53,
    /* 0x30-0x37 */
    0x4B, 0xE3, 0x8E, 0xB2, 0xD4, 0x2D, 0x4A, 0x2C,
    /* 0x38-0x3F */
    0x3A, 0x63, 0xE6, 0x9C, 0xB8, 0xB6, 0xD2, 0xC4
};

// =============================================================================
// Static state
// =============================================================================

static const interface_dcc_railcom_decoder_t *_interface;

// =============================================================================
// Public API
// =============================================================================

void DccRailcomDecoder_initialize(const interface_dcc_railcom_decoder_t *interface) {

    _interface = interface;

}

uint8_t DccRailcomDecoder_encode_byte(uint8_t value) {

    if (value > 0x3F) {

        return 0x00;

    }

    return _encode_table[value];

}

void DccRailcomDecoder_send_code_word(uint8_t code_word) {

    if (!_interface->uart_write) {

        return;

    }

    /* Special code words (ACK 0xF0/0x0F, NACK 0x3C) are transmitted raw,
     * bypassing the 4/8 encode table (2026 draft S-9.3.2). */
    _interface->uart_write(code_word);

}

void DccRailcomDecoder_send_ch1(uint8_t datagram_id, uint8_t data) {

    uint16_t combined;
    uint8_t high_six_bits;
    uint8_t low_six_bits;

    if (!_interface->uart_write) {

        return;

    }

    combined = ((uint16_t)(datagram_id & 0x0F) << 8) | data;
    high_six_bits = (uint8_t)((combined >> 6) & 0x3F);
    low_six_bits = (uint8_t)(combined & 0x3F);

    _interface->uart_write(_encode_table[high_six_bits]);
    _interface->uart_write(_encode_table[low_six_bits]);

}

void DccRailcomDecoder_send_ch2(const dcc_railcom_response_t *response) {

    uint16_t combined;
    uint8_t high_six_bits;
    uint8_t low_six_bits;
    uint8_t byte_index;

    if (!_interface->uart_write) {

        return;

    }

    if (response->count < 1) {

        return;

    }

    /* First two encoded bytes: 4-bit ID + 8-bit data[0] = 12-bit combined */
    combined = ((uint16_t)(response->datagram_id & 0x0F) << 8) | response->data[0];
    high_six_bits = (uint8_t)((combined >> 6) & 0x3F);
    low_six_bits = (uint8_t)(combined & 0x3F);

    _interface->uart_write(_encode_table[high_six_bits]);
    _interface->uart_write(_encode_table[low_six_bits]);

    /* Additional data bytes encoded individually as 6-bit values */
    for (byte_index = 1; byte_index < response->count; byte_index++) {

        _interface->uart_write(_encode_table[response->data[byte_index] & 0x3F]);

    }

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
