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
 * @file dcc_railcom_utilities.c
 * @brief Pure RailCom helper functions (4/8 codec) shared across roles.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#include "dcc_railcom_utilities.h"

#if defined(DCC_COMPILE_RAILCOM)

#if defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER)

    /**
     * @brief 4/8 encode table: maps a 6-bit data value to an 8-bit codeword.
     *
     * @details Each valid 8-bit codeword has exactly 4 ones and 4 zeros for DC
     * balance. The inverse of _decode_table.
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

uint8_t DccRailcomUtilities_encode_byte(uint8_t value) {

    if (value > 0x3F) {

        return 0x00;

    }

    return _encode_table[value];

}

void DccRailcomUtilities_encode_ch1(uint8_t datagram_id, uint8_t data, uint8_t *out) {

    uint16_t combined = ((uint16_t)(datagram_id & 0x0F) << 8) | data;

    out[0] = DccRailcomUtilities_encode_byte((uint8_t)((combined >> 6) & 0x3F));
    out[1] = DccRailcomUtilities_encode_byte((uint8_t)(combined & 0x3F));

}

uint8_t DccRailcomUtilities_encode_ch2(const dcc_railcom_response_t *response, uint8_t *out) {

    uint16_t combined;
    uint8_t count = 0;
    uint8_t byte_index;

    if (response->count < 1) {

        return 0;

    }

    /* First two encoded bytes: 4-bit ID + 8-bit data[0] = 12-bit combined */
    combined = ((uint16_t)(response->datagram_id & 0x0F) << 8) | response->data[0];
    out[count++] = DccRailcomUtilities_encode_byte((uint8_t)((combined >> 6) & 0x3F));
    out[count++] = DccRailcomUtilities_encode_byte((uint8_t)(combined & 0x3F));

    /* Additional data bytes encoded individually as 6-bit values */
    for (byte_index = 1; byte_index < response->count; byte_index++) {

        out[count++] = DccRailcomUtilities_encode_byte(response->data[byte_index] & 0x3F);

    }

    return count;

}

#endif /* DCC_COMPILE_DECODER || DCC_COMPILE_ACCESSORY_DECODER */

#if defined(DCC_COMPILE_COMMAND_STATION)

#include <string.h>

    /**
     * @brief 4/8 decode table: maps an 8-bit codeword to its 6-bit value or token.
     *
     * @details 0xFF = invalid, 0xFE = ACK, 0xFD = NACK (2026 draft S-9.3.2). The
     * inverse of _encode_table.
     */
static const uint8_t _decode_table[256] = {
    /* 0x00-0x0F */
    /* 0x0F is the alternate ACK special code word (2026 draft S-9.3.2) */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE,
    /* 0x10-0x1F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x20-0x2F */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x37, 0x35, 0x26, 0xFF,
    /* 0x30-0x3F */
    /* 0x3C is the NACK special code word (2026 draft S-9.3.2) */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1C, 0x1D, 0xFF,
    0xFF, 0xFF, 0x38, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF,
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

uint8_t DccRailcomUtilities_decode_byte(uint8_t encoded) {

    return _decode_table[encoded];

}

bool DccRailcomUtilities_decode_ch1(uint8_t byte0, uint8_t byte1, dcc_railcom_datagram_t *out) {

    uint8_t decoded_0 = DccRailcomUtilities_decode_byte(byte0);
    uint8_t decoded_1 = DccRailcomUtilities_decode_byte(byte1);
    uint16_t combined;

    memset(out, 0, sizeof(*out));

    if (decoded_0 >= 0x40 || decoded_1 >= 0x40) {

        return false;

    }

    combined = ((uint16_t)decoded_0 << 6) | decoded_1;

    out->datagram_id = (uint8_t)((combined >> 8) & 0x0F);
    out->data[0] = (uint8_t)(combined & 0xFF);
    out->count = 1;
    out->valid = true;

    return true;

}

bool DccRailcomUtilities_decode_ch2(const uint8_t *raw_bytes, uint8_t raw_count, dcc_railcom_datagram_t *out) {

    uint8_t decoded_bytes[DCC_RAILCOM_CH2_MAX_BYTES];
    uint8_t valid_count = 0;
    uint8_t byte_index;
    uint8_t decoded_value;
    uint16_t combined;

    memset(out, 0, sizeof(*out));

    for (byte_index = 0; byte_index < raw_count && byte_index < DCC_RAILCOM_CH2_MAX_BYTES; byte_index++) {

        decoded_value = DccRailcomUtilities_decode_byte(raw_bytes[byte_index]);

        if (decoded_value >= 0x40) {

            return false;

        }

        decoded_bytes[valid_count] = decoded_value;
        valid_count++;

    }

    if (valid_count < 2) {

        return false;

    }

    combined = ((uint16_t)decoded_bytes[0] << 6) | decoded_bytes[1];

    out->datagram_id = (uint8_t)((combined >> 8) & 0x0F);
    out->data[0] = (uint8_t)(combined & 0xFF);
    out->count = 1;

    for (byte_index = 2; byte_index < valid_count && out->count < DCC_RAILCOM_DATAGRAM_MAX_BYTES; byte_index++) {

        out->data[out->count] = decoded_bytes[byte_index];
        out->count++;

    }

    out->valid = true;

    return true;

}

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* DCC_COMPILE_RAILCOM */
