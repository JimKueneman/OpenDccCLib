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

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x01), (uint8_t)0xAB);

}

TEST(DccRailcomUtilities, encode_byte_value_0x3F) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x3F), (uint8_t)0xC4);

}

TEST(DccRailcomUtilities, encode_byte_out_of_range) {

    EXPECT_EQ(DccRailcomUtilities_encode_byte(0x40), (uint8_t)0x00);
    EXPECT_EQ(DccRailcomUtilities_encode_byte(0xFF), (uint8_t)0x00);

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

TEST(DccRailcomUtilities, decode_byte_value_0x01_codeword_0xAB) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xAB), (uint8_t)0x01);

}

TEST(DccRailcomUtilities, decode_byte_value_0x3F_codeword_0xC4) {

    EXPECT_EQ(DccRailcomUtilities_decode_byte(0xC4), (uint8_t)0x3F);

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
