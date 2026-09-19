/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for the pure RailCom utilities (4/8 codec).
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_utilities.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Encode (4/8 forward): 6-bit value -> 8-bit codeword
// ============================================================================

// @compliance DCC-S9.3.2-DEC-008
TEST(DccRailcomUtilities, encode_byte_value_0x00) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x00), (uint8_t)0xAC);

}

TEST(DccRailcomUtilities, encode_byte_value_0x01) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x01), (uint8_t)0xAA);

}

TEST(DccRailcomUtilities, encode_byte_value_0x3F) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x3F), (uint8_t)0x33);

}

TEST(DccRailcomUtilities, encode_byte_out_of_range) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x40), (uint8_t)0x00);
    EXPECT_EQ(DccRailcomUtilities_encode_byte(0xFF), (uint8_t)0x00);

}

// ============================================================================
// Channel 1 / Channel 2 datagram encode
// ============================================================================

TEST(DccRailcomUtilities, encode_ch1_basic) {

    uint8_t out[DCC_RAILCOM_CH1_MAX_BYTES];

    /* combined 0x001 -> high six bits 0x00 (0xAC), low six bits 0x01 (0xAA) */
    DccRailcomUtilities_encode_ch1(0, 0x01, out);

    EXPECT_EQ(out[0], (uint8_t)0xAC);
    EXPECT_EQ(out[1], (uint8_t)0xAA);

}

TEST(DccRailcomUtilities, encode_ch2_single_byte) {

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.count = 1;

    uint8_t out[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1];

    EXPECT_EQ(DccRailcomUtilities_encode_ch2(&response, out), (uint8_t)2);
    EXPECT_EQ(out[0], (uint8_t)0xAC);
    EXPECT_EQ(out[1], (uint8_t)0xAA);

}

TEST(DccRailcomUtilities, encode_ch2_multi_byte_count) {

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.data[1] = 0x00;
    response.data[2] = 0x3F;
    response.count = 3;

    uint8_t out[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1];

    /* 2 bytes for ID+data[0], then 1 each for data[1] and data[2] = 4 */
    EXPECT_EQ(DccRailcomUtilities_encode_ch2(&response, out), (uint8_t)4);
    EXPECT_EQ(out[2], DccRailcomUtilities_encode_byte(0x00));
    EXPECT_EQ(out[3], DccRailcomUtilities_encode_byte(0x3F));

}

TEST(DccRailcomUtilities, encode_ch2_empty_returns_zero) {

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.count = 0;

    uint8_t out[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1];

    EXPECT_EQ(DccRailcomUtilities_encode_ch2(&response, out), (uint8_t)0);

}

// ============================================================================
// Decode (4/8 reverse): 8-bit codeword -> 6-bit value or special token
// ============================================================================

// @compliance DCC-S9.3.2-CS-010
TEST(DccRailcomUtilities, decode_byte_invalid_0x00) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0x00), DCC_RAILCOM_DECODE_INVALID);

}

// @compliance DCC-S9.3.2-CS-011
TEST(DccRailcomUtilities, decode_byte_ack_0xF0) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xF0), DCC_RAILCOM_DECODE_ACK);

}

TEST(DccRailcomUtilities, decode_byte_ack_0x0F) {

    /* 0x0F is the alternate ACK special code word (2026 draft S-9.3.2) */
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0x0F), DCC_RAILCOM_DECODE_ACK);

}

// @compliance DCC-S9.3.2-CS-012
TEST(DccRailcomUtilities, decode_byte_nack_0x3C) {

    /* 0x3C is the NACK special code word (2026 draft S-9.3.2) */
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0x3C), DCC_RAILCOM_DECODE_NACK);

}

// @compliance DCC-S9.3.2-CS-010
TEST(DccRailcomUtilities, decode_byte_value_0x00_codeword_0xAC) {

    /* 6-bit value 0x00 maps to codeword 0xAC */
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xAC), (uint8_t)0x00);

}

TEST(DccRailcomUtilities, decode_byte_value_0x01_codeword_0xAA) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xAA), (uint8_t)0x01);

}

TEST(DccRailcomUtilities, decode_byte_value_0x3F_codeword_0x33) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0x33), (uint8_t)0x3F);

}

// ============================================================================
// Channel 1 / Channel 2 datagram decode
// ============================================================================

TEST(DccRailcomUtilities, decode_ch1_basic) {

    dcc_railcom_datagram_t datagram;

    /* 0xAC->0x00, 0xAA->0x01 -> combined 0x001 -> id 0, data[0] 0x01 */
    EXPECT_TRUE(DccRailcomUtilities_decode_ch1(0xAC, 0xAA, &datagram));
    EXPECT_EQ(datagram.datagram_id, (uint8_t)0);
    EXPECT_EQ(datagram.data[0], (uint8_t)0x01);
    EXPECT_EQ(datagram.count, (uint8_t)1);
    EXPECT_TRUE(datagram.valid);

}

TEST(DccRailcomUtilities, decode_ch1_invalid_codeword) {

    dcc_railcom_datagram_t datagram;

    /* 0x00 is not a valid codeword */
    EXPECT_FALSE(DccRailcomUtilities_decode_ch1(0x00, 0xAA, &datagram));

}

TEST(DccRailcomUtilities, decode_ch2_multi_byte) {

    dcc_railcom_datagram_t datagram;
    uint8_t raw[] = { 0xAC, 0xAA, 0xAC, 0x33 };   /* 0x00, 0x01, 0x00, 0x3F */

    EXPECT_TRUE(DccRailcomUtilities_decode_ch2(raw, 4, &datagram));
    EXPECT_EQ(datagram.datagram_id, (uint8_t)0);
    EXPECT_EQ(datagram.data[0], (uint8_t)0x01);
    EXPECT_EQ(datagram.count, (uint8_t)3);
    EXPECT_EQ(datagram.data[1], (uint8_t)0x00);
    EXPECT_EQ(datagram.data[2], (uint8_t)0x3F);

}

TEST(DccRailcomUtilities, decode_ch2_too_few_bytes) {

    dcc_railcom_datagram_t datagram;
    uint8_t raw[] = { 0xAC };

    EXPECT_FALSE(DccRailcomUtilities_decode_ch2(raw, 1, &datagram));

}

TEST(DccRailcomUtilities, decode_ch2_invalid_byte) {

    dcc_railcom_datagram_t datagram;
    uint8_t raw[] = { 0xAC, 0x00 };   /* 0x00 is an invalid codeword */

    EXPECT_FALSE(DccRailcomUtilities_decode_ch2(raw, 2, &datagram));

}

// ============================================================================
// Encode table matches NMRA S-9.3.2 Table 2
// ============================================================================

/* The 64 six-bit values -> 4/8 code words, transcribed from NMRA S-9.3.2
 * Table 2 (RCN-217 section 2.5 lists the same values). */
static const uint8_t k_spec_encode_table[64] = {
    0xAC, 0xAA, 0xA9, 0xA5, 0xA3, 0xA6, 0x9C, 0x9A,
    0x99, 0x95, 0x93, 0x96, 0x8E, 0x8D, 0x8B, 0xB1,
    0xB2, 0xB4, 0xB8, 0x74, 0x72, 0x6C, 0x6A, 0x69,
    0x65, 0x63, 0x66, 0x5C, 0x5A, 0x59, 0x55, 0x53,
    0x56, 0x4E, 0x4D, 0x4B, 0x47, 0x71, 0xE8, 0xE4,
    0xE2, 0xD1, 0xC9, 0xC5, 0xD8, 0xD4, 0xD2, 0xCA,
    0xC6, 0xCC, 0x78, 0x17, 0x1B, 0x1D, 0x1E, 0x2E,
    0x36, 0x3A, 0x27, 0x2B, 0x2D, 0x35, 0x39, 0x33
};

// @compliance DCC-S9.3.2-DEC-008
TEST(DccRailcomUtilities, encode_byte_matches_spec_table) {

    uint8_t value;

    for (value = 0; value < 64; value++) {

        EXPECT_EQ(DccRailcomUtilities_encode_byte(value), k_spec_encode_table[value])
            << "Wrong code word for value " << (int)value;

    }

}

TEST(DccRailcomUtilities, encode_byte_codewords_have_four_ones) {

    uint8_t value;

    for (value = 0; value < 64; value++) {

        uint8_t codeword = DccRailcomUtilities_encode_byte(value);
        int ones = 0;
        int bit;

        for (bit = 0; bit < 8; bit++) {

            ones += (codeword >> bit) & 1;

        }

        EXPECT_EQ(ones, 4) << "Code word for value " << (int)value << " is not 4-of-8";

    }

}

// @compliance DCC-S9.3.2-CS-010
TEST(DccRailcomUtilities, decode_byte_reserved_codewords_invalid) {

    /* S-9.3.2 Table 2 reserves these three four-ones words */
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xE1), DCC_RAILCOM_DECODE_INVALID);
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xC3), DCC_RAILCOM_DECODE_INVALID);
    EXPECT_EQ(DccRailcomUtilities_decode_byte(0x87), DCC_RAILCOM_DECODE_INVALID);

}

// ============================================================================
// Encode/decode round-trip: the encode table is the inverse of the decode table
// ============================================================================

// @compliance DCC-S9.3.2-DEC-008
TEST(DccRailcomUtilities, round_trip_all_values) {

    uint8_t value;

    for (value = 0; value < 64; value++) {

        uint8_t encoded = DccRailcomUtilities_encode_byte(value);
        uint8_t decoded = DccRailcomUtilities_decode_byte(encoded);
        EXPECT_EQ(decoded, value) << "Round-trip failed for value " << (int)value;

    }

}
