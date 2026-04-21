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
 * @file dcc_packet_encoder_raw_Test.cxx
 * @brief GoogleTest suite for dcc_packet_encoder.c — raw encoder functions.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_packet_encoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

// ============================================================================
// Helper: verify XOR byte is correct for any packet
// ============================================================================

static void verify_xor(const dcc_packet_t *pkt) {

    uint8_t xor_check = 0;

    for (uint8_t i = 0; i < pkt->byte_count - 1; i++) {

        xor_check ^= pkt->data[i];

    }

    EXPECT_EQ(xor_check, pkt->data[pkt->byte_count - 1]);

}

// ============================================================================
// Idle packet
// ============================================================================

TEST(DccPacketEncoderRaw, idle_bytes) {

    dcc_packet_t pkt;
    DccPacketEncoder_idle(&pkt);

    EXPECT_EQ(pkt.data[0], DCC_IDLE_ADDR_BYTE);
    EXPECT_EQ(pkt.data[1], DCC_IDLE_DATA_BYTE);
    EXPECT_EQ(pkt.data[2], DCC_IDLE_XOR_BYTE);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Reset packet
// ============================================================================

TEST(DccPacketEncoderRaw, reset_bytes) {

    dcc_packet_t pkt;
    DccPacketEncoder_reset(&pkt);

    EXPECT_EQ(pkt.data[0], DCC_RESET_BYTE);
    EXPECT_EQ(pkt.data[1], DCC_RESET_BYTE);
    EXPECT_EQ(pkt.data[2], DCC_RESET_BYTE);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Emergency stop all
// ============================================================================

TEST(DccPacketEncoderRaw, estop_all_bytes) {

    dcc_packet_t pkt;
    DccPacketEncoder_estop_all(&pkt);

    EXPECT_EQ(pkt.data[0], DCC_ADDRESS_BROADCAST_VALUE);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_128_SPEED);
    EXPECT_EQ(pkt.data[2], 0x81);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Speed 128
// ============================================================================

TEST(DccPacketEncoderRaw, speed_128_invalid_address_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_128(&pkt, 1, DCC_ADDRESS_ACCESSORY, 50, true));

}

TEST(DccPacketEncoderRaw, speed_128_invalid_address_type_extended) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_128(&pkt, 1, DCC_ADDRESS_ACCESSORY_EXTENDED, 50, true));

}

TEST(DccPacketEncoderRaw, speed_128_invalid_address_type_idle) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_128(&pkt, 255, DCC_ADDRESS_IDLE, 50, true));

}

TEST(DccPacketEncoderRaw, speed_128_speed_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 128, true));

}

TEST(DccPacketEncoderRaw, speed_128_short_addr_forward) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_128_SPEED);
    EXPECT_EQ(pkt.data[2], 0x80 | 50);  /* direction=forward, speed=50 */
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_128_long_addr_reverse) {

    dcc_packet_t pkt;
    /* Long addr 300 = 0xC0 | (300>>8 & 0x3F) = 0xC1, low = 0x2C */
    EXPECT_TRUE(DccPacketEncoder_speed_128(&pkt, 300, DCC_ADDRESS_LONG, 0, false));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_ADV_OPS_128_SPEED);
    EXPECT_EQ(pkt.data[3], 0x00);  /* direction=reverse, speed=0 */
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_128_broadcast) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_128(&pkt, 0, DCC_ADDRESS_BROADCAST, 127, true));

    EXPECT_EQ(pkt.data[0], 0x00);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_128_SPEED);
    EXPECT_EQ(pkt.data[2], 0x80 | 127);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Speed 28
// ============================================================================

TEST(DccPacketEncoderRaw, speed_28_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_28(&pkt, 1, DCC_ADDRESS_ACCESSORY, 10, true));

}

TEST(DccPacketEncoderRaw, speed_28_speed_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_28(&pkt, 1, DCC_ADDRESS_SHORT, 30, true));

}

TEST(DccPacketEncoderRaw, speed_28_stop_forward) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 0, true));

    /* speed 0 = stop, lookup = 0x00, forward = 0x60 */
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x00);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_28_estop_forward) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 1, true));

    /* speed 1 = estop, lookup = 0x01, forward = 0x60 */
    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x01);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_28_mid_range) {

    dcc_packet_t pkt;
    /* speed 15 = lookup[15] = 0x18 (interleaved pattern) */
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 15, true));

    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x18);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_28_reverse) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 10, false));

    /* speed 10 = lookup[10] = 0x06, reverse = 0x40 */
    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_REVERSE | 0x06);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_28_max_speed) {

    dcc_packet_t pkt;
    /* speed 29 = lookup[29] = 0x1F */
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 29, true));

    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x1F);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_28_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_28(&pkt, 300, DCC_ADDRESS_LONG, 5, true));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_INST_SPEED_FORWARD | 0x13);  /* lookup[5] = 0x13 */
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

// ============================================================================
// Speed 14
// ============================================================================

TEST(DccPacketEncoderRaw, speed_14_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_14(&pkt, 1, DCC_ADDRESS_ACCESSORY, 5, true, true));

}

TEST(DccPacketEncoderRaw, speed_14_speed_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_14(&pkt, 1, DCC_ADDRESS_SHORT, 16, true, true));

}

TEST(DccPacketEncoderRaw, speed_14_headlight_on) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 5, true, true));

    /* forward=0x60, headlight=0x10, speed=5 => 0x75 */
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x10 | 5);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_14_headlight_off) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 5, true, false));

    /* forward=0x60, headlight=0x00, speed=5 => 0x65 */
    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_FORWARD | 0x00 | 5);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_14_reverse_no_headlight) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 10, false, false));

    EXPECT_EQ(pkt.data[1], DCC_INST_SPEED_REVERSE | 0x00 | 10);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_14_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_14(&pkt, 300, DCC_ADDRESS_LONG, 7, true, true));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_INST_SPEED_FORWARD | 0x10 | 7);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

// ============================================================================
// Function Group 1
// ============================================================================

TEST(DccPacketEncoderRaw, func_group_1_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_group_1(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x1F));

}

TEST(DccPacketEncoderRaw, func_group_1_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0x1F));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_INST_FUNC_GROUP_1 | 0x1F);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_group_1_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_1(&pkt, 300, DCC_ADDRESS_LONG, 0x0A));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_INST_FUNC_GROUP_1 | 0x0A);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_group_1_masks_bits) {

    dcc_packet_t pkt;
    /* Pass 0xFF, only lower 5 bits should be used */
    EXPECT_TRUE(DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0xFF));

    EXPECT_EQ(pkt.data[1], DCC_INST_FUNC_GROUP_1 | 0x1F);
    verify_xor(&pkt);

}

// ============================================================================
// Function Group 2a (F5-F8)
// ============================================================================

TEST(DccPacketEncoderRaw, func_group_2a_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_group_2a(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x0F));

}

TEST(DccPacketEncoderRaw, func_group_2a_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_2a(&pkt, 3, DCC_ADDRESS_SHORT, 0x0F));

    EXPECT_EQ(pkt.data[1], DCC_INST_FUNC_GROUP_2A | 0x0F);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_group_2a_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_2a(&pkt, 300, DCC_ADDRESS_LONG, 0x05));

    EXPECT_EQ(pkt.data[2], DCC_INST_FUNC_GROUP_2A | 0x05);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

// ============================================================================
// Function Group 2b (F9-F12)
// ============================================================================

TEST(DccPacketEncoderRaw, func_group_2b_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_group_2b(&pkt, 1, DCC_ADDRESS_ACCESSORY_EXTENDED, 0x0F));

}

TEST(DccPacketEncoderRaw, func_group_2b_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_2b(&pkt, 3, DCC_ADDRESS_SHORT, 0x0F));

    EXPECT_EQ(pkt.data[1], DCC_INST_FUNC_GROUP_2B | 0x0F);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_group_2b_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_group_2b(&pkt, 300, DCC_ADDRESS_LONG, 0x0A));

    EXPECT_EQ(pkt.data[2], DCC_INST_FUNC_GROUP_2B | 0x0A);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F13-F20
// ============================================================================

TEST(DccPacketEncoderRaw, func_f13_f20_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f13_f20(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0xAA));

}

TEST(DccPacketEncoderRaw, func_f13_f20_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f13_f20(&pkt, 3, DCC_ADDRESS_SHORT, 0xAA));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F13_F20);
    EXPECT_EQ(pkt.data[2], 0xAA);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f13_f20_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f13_f20(&pkt, 300, DCC_ADDRESS_LONG, 0x55));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_FEAT_F13_F20);
    EXPECT_EQ(pkt.data[3], 0x55);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F21-F28
// ============================================================================

TEST(DccPacketEncoderRaw, func_f21_f28_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f21_f28(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0xFF));

}

TEST(DccPacketEncoderRaw, func_f21_f28_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f21_f28(&pkt, 3, DCC_ADDRESS_SHORT, 0xBB));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F21_F28);
    EXPECT_EQ(pkt.data[2], 0xBB);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f21_f28_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f21_f28(&pkt, 300, DCC_ADDRESS_LONG, 0x11));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F21_F28);
    EXPECT_EQ(pkt.data[3], 0x11);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F29-F36
// ============================================================================

TEST(DccPacketEncoderRaw, func_f29_f36_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f29_f36(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x01));

}

TEST(DccPacketEncoderRaw, func_f29_f36_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f29_f36(&pkt, 3, DCC_ADDRESS_SHORT, 0xCC));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F29_F36);
    EXPECT_EQ(pkt.data[2], 0xCC);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f29_f36_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f29_f36(&pkt, 300, DCC_ADDRESS_LONG, 0x22));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F29_F36);
    EXPECT_EQ(pkt.data[3], 0x22);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F37-F44
// ============================================================================

TEST(DccPacketEncoderRaw, func_f37_f44_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f37_f44(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x01));

}

TEST(DccPacketEncoderRaw, func_f37_f44_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f37_f44(&pkt, 3, DCC_ADDRESS_SHORT, 0xDD));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F37_F44);
    EXPECT_EQ(pkt.data[2], 0xDD);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f37_f44_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f37_f44(&pkt, 300, DCC_ADDRESS_LONG, 0x33));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F37_F44);
    EXPECT_EQ(pkt.data[3], 0x33);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F45-F52
// ============================================================================

TEST(DccPacketEncoderRaw, func_f45_f52_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f45_f52(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x01));

}

TEST(DccPacketEncoderRaw, func_f45_f52_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f45_f52(&pkt, 3, DCC_ADDRESS_SHORT, 0xEE));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F45_F52);
    EXPECT_EQ(pkt.data[2], 0xEE);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f45_f52_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f45_f52(&pkt, 300, DCC_ADDRESS_LONG, 0x44));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F45_F52);
    EXPECT_EQ(pkt.data[3], 0x44);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F53-F60
// ============================================================================

TEST(DccPacketEncoderRaw, func_f53_f60_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f53_f60(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x01));

}

TEST(DccPacketEncoderRaw, func_f53_f60_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f53_f60(&pkt, 3, DCC_ADDRESS_SHORT, 0x77));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F53_F60);
    EXPECT_EQ(pkt.data[2], 0x77);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f53_f60_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f53_f60(&pkt, 300, DCC_ADDRESS_LONG, 0x88));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F53_F60);
    EXPECT_EQ(pkt.data[3], 0x88);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Function expansion F61-F68
// ============================================================================

TEST(DccPacketEncoderRaw, func_f61_f68_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_f61_f68(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0x01));

}

TEST(DccPacketEncoderRaw, func_f61_f68_short_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f61_f68(&pkt, 3, DCC_ADDRESS_SHORT, 0x99));

    EXPECT_EQ(pkt.data[1], DCC_FEAT_F61_F68);
    EXPECT_EQ(pkt.data[2], 0x99);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, func_f61_f68_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_func_f61_f68(&pkt, 300, DCC_ADDRESS_LONG, 0x66));

    EXPECT_EQ(pkt.data[2], DCC_FEAT_F61_F68);
    EXPECT_EQ(pkt.data[3], 0x66);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory basic
// ============================================================================

TEST(DccPacketEncoderRaw, accessory_basic_board_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic(&pkt, 512, 0, true));

}

TEST(DccPacketEncoderRaw, accessory_basic_output_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic(&pkt, 0, 8, true));

}

TEST(DccPacketEncoderRaw, accessory_basic_valid_activate) {

    dcc_packet_t pkt;
    uint16_t addr = 5;
    uint8_t output = 3;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic(&pkt, addr, output, true));

    uint8_t byte0 = DCC_ACCESSORY_BASIC_PREFIX | (uint8_t)(addr & 0x3F);
    uint8_t byte1 = 0x80
                    | (uint8_t)((~(addr >> 6) & 0x07) << 4)
                    | 0x08
                    | (output & 0x07);

    EXPECT_EQ(pkt.data[0], byte0);
    EXPECT_EQ(pkt.data[1], byte1);
    EXPECT_EQ(pkt.byte_count, 3);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, accessory_basic_valid_deactivate) {

    dcc_packet_t pkt;
    uint16_t addr = 100;
    uint8_t output = 2;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic(&pkt, addr, output, false));

    uint8_t byte0 = DCC_ACCESSORY_BASIC_PREFIX | (uint8_t)(addr & 0x3F);
    uint8_t byte1 = 0x80
                    | (uint8_t)((~(addr >> 6) & 0x07) << 4)
                    | 0x00
                    | (output & 0x07);

    EXPECT_EQ(pkt.data[0], byte0);
    EXPECT_EQ(pkt.data[1], byte1);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, accessory_basic_max_address) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_accessory_basic(&pkt, 511, 7, true));

    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory extended
// ============================================================================

TEST(DccPacketEncoderRaw, accessory_extended_address_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended(&pkt, 2048, 0));

}

TEST(DccPacketEncoderRaw, accessory_extended_valid) {

    dcc_packet_t pkt;
    uint16_t addr = 100;
    uint8_t aspect = 0x1A;

    EXPECT_TRUE(DccPacketEncoder_accessory_extended(&pkt, addr, aspect));

    uint8_t byte0 = DCC_ACCESSORY_BASIC_PREFIX | (uint8_t)(addr & 0x3F);
    uint8_t byte1 = DCC_ACCESSORY_EXTENDED_PREFIX
                    | (uint8_t)((~(addr >> 6) & 0x07) << 4)
                    | (uint8_t)(((addr >> 9) & 0x03) << 1);

    EXPECT_EQ(pkt.data[0], byte0);
    EXPECT_EQ(pkt.data[1], byte1);
    EXPECT_EQ(pkt.data[2], aspect);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, accessory_extended_max_address) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_accessory_extended(&pkt, 2047, 0xFF));

    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory basic CV write
// ============================================================================

TEST(DccPacketEncoderRaw, acc_basic_cv_write_board_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 512, 0, 1, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_write_output_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 0, 4, 1, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_write_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 0, 0, 0, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_write_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 0, 0, 1025, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_write_valid) {

    dcc_packet_t pkt;
    uint16_t board = 5;
    uint8_t output = 2;
    uint16_t cv = 29;
    uint8_t val = 0x55;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic_cv_write(&pkt, board, output, cv, val));

    uint16_t wire_cv = cv - 1;

    /* Byte 0: address */
    EXPECT_EQ(pkt.data[0], DCC_ACCESSORY_BASIC_PREFIX | (uint8_t)(board & 0x3F));

    /* Byte 1: 1AAA1AA0 */
    uint8_t byte1 = 0x80
                    | (uint8_t)((~(board >> 6) & 0x07) << 4)
                    | 0x08
                    | (uint8_t)((output & 0x03) << 1);
    EXPECT_EQ(pkt.data[1], byte1);

    /* Byte 2: CV instruction */
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_WRITE | (uint8_t)((wire_cv >> 8) & 0x03));

    /* Byte 3: CV low */
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));

    /* Byte 4: value */
    EXPECT_EQ(pkt.data[4], val);

    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory basic CV verify
// ============================================================================

TEST(DccPacketEncoderRaw, acc_basic_cv_verify_board_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_verify(&pkt, 512, 0, 1, 0));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_verify_output_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_verify(&pkt, 0, 4, 1, 0));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_verify_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_verify(&pkt, 0, 0, 0, 0));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_verify_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_verify(&pkt, 0, 0, 1025, 0));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_verify_valid) {

    dcc_packet_t pkt;
    uint16_t board = 10;
    uint8_t output = 1;
    uint16_t cv = 1;
    uint8_t val = 0x03;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic_cv_verify(&pkt, board, output, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_VERIFY | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[4], val);
    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory basic CV bit
// ============================================================================

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_board_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 512, 0, 1, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_output_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 0, 4, 1, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 0, 0, 0, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 0, 0, 1025, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_position_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 0, 0, 1, 8, true, true));

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_write_set) {

    dcc_packet_t pkt;
    uint8_t bit_pos = 3;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 5, 1, 29, bit_pos, true, true));

    /* Bit byte: 0xE0 | write(0x10) | bit_value(0x08) | pos(3) = 0xFB */
    uint8_t expected_bit_byte = 0xE0 | 0x10 | 0x08 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[4], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, acc_basic_cv_bit_verify_clear) {

    dcc_packet_t pkt;
    uint8_t bit_pos = 5;

    EXPECT_TRUE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 5, 1, 29, bit_pos, false, false));

    /* Bit byte: 0xE0 | 0x00 | 0x00 | 5 = 0xE5 */
    uint8_t expected_bit_byte = 0xE0 | 0x00 | 0x00 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[4], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory extended CV write
// ============================================================================

TEST(DccPacketEncoderRaw, acc_ext_cv_write_addr_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 2048, 1, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_write_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 100, 0, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_write_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 100, 1025, 0xAA));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_write_valid) {

    dcc_packet_t pkt;
    uint16_t addr = 100;
    uint16_t cv = 29;
    uint8_t val = 0x55;

    EXPECT_TRUE(DccPacketEncoder_accessory_extended_cv_write(&pkt, addr, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_WRITE | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[4], val);
    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory extended CV verify
// ============================================================================

TEST(DccPacketEncoderRaw, acc_ext_cv_verify_addr_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_verify(&pkt, 2048, 1, 0));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_verify_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_verify(&pkt, 100, 0, 0));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_verify_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_verify(&pkt, 100, 1025, 0));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_verify_valid) {

    dcc_packet_t pkt;
    uint16_t addr = 200;
    uint16_t cv = 1024;
    uint8_t val = 0xFF;

    EXPECT_TRUE(DccPacketEncoder_accessory_extended_cv_verify(&pkt, addr, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_VERIFY | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[4], val);
    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Accessory extended CV bit
// ============================================================================

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_addr_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 2048, 1, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 100, 0, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 100, 1025, 0, true, true));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_position_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 100, 1, 8, true, true));

}

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_write_set) {

    dcc_packet_t pkt;
    uint8_t bit_pos = 7;

    EXPECT_TRUE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 100, 29, bit_pos, true, true));

    uint8_t expected_bit_byte = 0xE0 | 0x10 | 0x08 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[4], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 6);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, acc_ext_cv_bit_verify_clear) {

    dcc_packet_t pkt;
    uint8_t bit_pos = 0;

    EXPECT_TRUE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 100, 29, bit_pos, false, false));

    uint8_t expected_bit_byte = 0xE0 | 0x00 | 0x00 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[4], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Mobile CV write ops
// ============================================================================

TEST(DccPacketEncoderRaw, cv_write_ops_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_write_ops(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, 0xAA));

}

TEST(DccPacketEncoderRaw, cv_write_ops_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, 0, 0xAA));

}

TEST(DccPacketEncoderRaw, cv_write_ops_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1025, 0xAA));

}

TEST(DccPacketEncoderRaw, cv_write_ops_short_addr) {

    dcc_packet_t pkt;
    uint16_t cv = 29;
    uint8_t val = 0x55;

    EXPECT_TRUE(DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_CV_LONG_WRITE | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[2], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[3], val);
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, cv_write_ops_long_addr) {

    dcc_packet_t pkt;
    uint16_t cv = 1024;
    uint8_t val = 0xFF;

    EXPECT_TRUE(DccPacketEncoder_cv_write_ops(&pkt, 300, DCC_ADDRESS_LONG, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_WRITE | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[4], val);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Mobile CV verify ops
// ============================================================================

TEST(DccPacketEncoderRaw, cv_verify_ops_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_verify_ops(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, 0));

}

TEST(DccPacketEncoderRaw, cv_verify_ops_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_verify_ops(&pkt, 3, DCC_ADDRESS_SHORT, 0, 0));

}

TEST(DccPacketEncoderRaw, cv_verify_ops_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_verify_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1025, 0));

}

TEST(DccPacketEncoderRaw, cv_verify_ops_short_addr) {

    dcc_packet_t pkt;
    uint16_t cv = 8;
    uint8_t val = 0x10;

    EXPECT_TRUE(DccPacketEncoder_cv_verify_ops(&pkt, 3, DCC_ADDRESS_SHORT, cv, val));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[1], DCC_CV_LONG_VERIFY | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[2], (uint8_t)(wire_cv & 0xFF));
    EXPECT_EQ(pkt.data[3], val);
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, cv_verify_ops_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_cv_verify_ops(&pkt, 300, DCC_ADDRESS_LONG, 1, 0x00));

    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_VERIFY | 0x00);
    EXPECT_EQ(pkt.data[3], 0x00);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Mobile CV bit ops
// ============================================================================

TEST(DccPacketEncoderRaw, cv_bit_ops_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, 0, true, true));

}

TEST(DccPacketEncoderRaw, cv_bit_ops_cv_zero) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 0, 0, true, true));

}

TEST(DccPacketEncoderRaw, cv_bit_ops_cv_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1025, 0, true, true));

}

TEST(DccPacketEncoderRaw, cv_bit_ops_bit_position_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1, 8, true, true));

}

TEST(DccPacketEncoderRaw, cv_bit_ops_write_set_short) {

    dcc_packet_t pkt;
    uint16_t cv = 29;
    uint8_t bit_pos = 5;

    EXPECT_TRUE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, cv, bit_pos, true, true));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_CV_LONG_BIT | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[2], (uint8_t)(wire_cv & 0xFF));

    uint8_t expected_bit_byte = 0xE0 | 0x10 | 0x08 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[3], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, cv_bit_ops_verify_clear_long) {

    dcc_packet_t pkt;
    uint16_t cv = 1024;
    uint8_t bit_pos = 0;

    EXPECT_TRUE(DccPacketEncoder_cv_bit_ops(&pkt, 300, DCC_ADDRESS_LONG, cv, bit_pos, false, false));

    uint16_t wire_cv = cv - 1;
    EXPECT_EQ(pkt.data[2], DCC_CV_LONG_BIT | (uint8_t)((wire_cv >> 8) & 0x03));
    EXPECT_EQ(pkt.data[3], (uint8_t)(wire_cv & 0xFF));

    uint8_t expected_bit_byte = 0xE0 | 0x00 | 0x00 | (bit_pos & 0x07);
    EXPECT_EQ(pkt.data[4], expected_bit_byte);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Consist set
// ============================================================================

TEST(DccPacketEncoderRaw, consist_set_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_consist_set(&pkt, 1, DCC_ADDRESS_ACCESSORY, 10, true));

}

TEST(DccPacketEncoderRaw, consist_set_consist_addr_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 128, true));

}

TEST(DccPacketEncoderRaw, consist_set_normal_direction) {

    dcc_packet_t pkt;
    uint8_t consist_addr = 50;

    EXPECT_TRUE(DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, consist_addr, true));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_NORMAL);
    EXPECT_EQ(pkt.data[2], consist_addr & 0x7F);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, consist_set_reversed_direction) {

    dcc_packet_t pkt;
    uint8_t consist_addr = 100;

    EXPECT_TRUE(DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, consist_addr, false));

    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_REVERSED);
    EXPECT_EQ(pkt.data[2], consist_addr & 0x7F);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, consist_set_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_consist_set(&pkt, 300, DCC_ADDRESS_LONG, 127, true));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_CONSIST_SET_NORMAL);
    EXPECT_EQ(pkt.data[3], 127);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Consist clear
// ============================================================================

TEST(DccPacketEncoderRaw, consist_clear_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_consist_clear(&pkt, 1, DCC_ADDRESS_ACCESSORY));

}

TEST(DccPacketEncoderRaw, consist_clear_valid) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_consist_clear(&pkt, 3, DCC_ADDRESS_SHORT));

    /* consist_clear delegates to consist_set(addr, type, 0, true) */
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_NORMAL);
    EXPECT_EQ(pkt.data[2], 0);
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

// ============================================================================
// Binary state short
// ============================================================================

TEST(DccPacketEncoderRaw, binary_state_short_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_short(&pkt, 1, DCC_ADDRESS_ACCESSORY, 10, true));

}

TEST(DccPacketEncoderRaw, binary_state_short_state_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, 128, true));

}

TEST(DccPacketEncoderRaw, binary_state_short_active) {

    dcc_packet_t pkt;
    uint8_t state = 42;

    EXPECT_TRUE(DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, state, true));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_BINARY_STATE_SHORT);
    EXPECT_EQ(pkt.data[2], 0x80 | (state & 0x7F));
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, binary_state_short_inactive) {

    dcc_packet_t pkt;
    uint8_t state = 42;

    EXPECT_TRUE(DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, state, false));

    EXPECT_EQ(pkt.data[2], 0x00 | (state & 0x7F));
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, binary_state_short_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_binary_state_short(&pkt, 300, DCC_ADDRESS_LONG, 0, true));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_FEAT_BINARY_STATE_SHORT);
    EXPECT_EQ(pkt.data[3], 0x80);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

// ============================================================================
// Binary state long
// ============================================================================

TEST(DccPacketEncoderRaw, binary_state_long_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_long(&pkt, 1, DCC_ADDRESS_ACCESSORY, 10, true));

}

TEST(DccPacketEncoderRaw, binary_state_long_state_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, 32768, true));

}

TEST(DccPacketEncoderRaw, binary_state_long_active) {

    dcc_packet_t pkt;
    uint16_t state = 1000;

    EXPECT_TRUE(DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, state, true));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_BINARY_STATE_LONG);
    /* Low byte: active(0x80) | (1000 & 0x7F) = 0x80 | 0x68 = 0xE8 */
    EXPECT_EQ(pkt.data[2], (uint8_t)(0x80 | (state & 0x7F)));
    /* High byte: (1000 >> 7) & 0xFF = 7 */
    EXPECT_EQ(pkt.data[3], (uint8_t)((state >> 7) & 0xFF));
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, binary_state_long_inactive) {

    dcc_packet_t pkt;
    uint16_t state = 500;

    EXPECT_TRUE(DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, state, false));

    EXPECT_EQ(pkt.data[2], (uint8_t)(0x00 | (state & 0x7F)));
    EXPECT_EQ(pkt.data[3], (uint8_t)((state >> 7) & 0xFF));
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, binary_state_long_max_state) {

    dcc_packet_t pkt;
    uint16_t state = 32767;

    EXPECT_TRUE(DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, state, true));

    EXPECT_EQ(pkt.data[2], (uint8_t)(0x80 | (state & 0x7F)));
    EXPECT_EQ(pkt.data[3], (uint8_t)((state >> 7) & 0xFF));
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, binary_state_long_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_binary_state_long(&pkt, 300, DCC_ADDRESS_LONG, 100, true));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_FEAT_BINARY_STATE_LONG);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Analog function
// ============================================================================

TEST(DccPacketEncoderRaw, analog_function_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_analog_function(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0, 0));

}

TEST(DccPacketEncoderRaw, analog_function_short_addr) {

    dcc_packet_t pkt;
    uint8_t output = 0x10;
    uint8_t value = 0x80;

    EXPECT_TRUE(DccPacketEncoder_analog_function(&pkt, 3, DCC_ADDRESS_SHORT, output, value));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_ANALOG_FUNCTION);
    EXPECT_EQ(pkt.data[2], output);
    EXPECT_EQ(pkt.data[3], value);
    EXPECT_EQ(pkt.byte_count, 5);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, analog_function_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_analog_function(&pkt, 300, DCC_ADDRESS_LONG, 0xFF, 0x00));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_ADV_OPS_ANALOG_FUNCTION);
    EXPECT_EQ(pkt.data[3], 0xFF);
    EXPECT_EQ(pkt.data[4], 0x00);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);

}

// ============================================================================
// Speed restriction
// ============================================================================

TEST(DccPacketEncoderRaw, speed_restriction_invalid_type) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_restriction(&pkt, 1, DCC_ADDRESS_ACCESSORY, true, 50));

}

TEST(DccPacketEncoderRaw, speed_restriction_limit_too_high) {

    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, true, 128));

}

TEST(DccPacketEncoderRaw, speed_restriction_enabled) {

    dcc_packet_t pkt;
    uint8_t limit = 64;

    EXPECT_TRUE(DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, true, limit));

    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_SPEED_RESTRICTION);
    EXPECT_EQ(pkt.data[2], 0x80 | (limit & 0x7F));
    EXPECT_EQ(pkt.byte_count, 4);
    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_restriction_disabled) {

    dcc_packet_t pkt;
    uint8_t limit = 64;

    EXPECT_TRUE(DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, false, limit));

    EXPECT_EQ(pkt.data[2], 0x00 | (limit & 0x7F));
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);

}

TEST(DccPacketEncoderRaw, speed_restriction_long_addr) {

    dcc_packet_t pkt;
    EXPECT_TRUE(DccPacketEncoder_speed_restriction(&pkt, 300, DCC_ADDRESS_LONG, true, 127));

    EXPECT_EQ(pkt.data[0], 0xC1);
    EXPECT_EQ(pkt.data[1], 0x2C);
    EXPECT_EQ(pkt.data[2], DCC_ADV_OPS_SPEED_RESTRICTION);
    EXPECT_EQ(pkt.data[3], 0x80 | 127);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);

}

#endif /* DCC_COMPILE_COMMAND_STATION */
