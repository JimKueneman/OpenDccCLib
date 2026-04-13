/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Packet Encoder (Phase 2 + Phase 3)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_packet_encoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Helper: verify XOR byte is correct for any packet
// ============================================================================

static void verify_xor(const dcc_packet_t *pkt) {

    uint8_t xor_check = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < pkt->byte_count - 1; byte_index++) {

        xor_check ^= pkt->data[byte_index];

    }

    EXPECT_EQ(xor_check, pkt->data[pkt->byte_count - 1]);

}

// ============================================================================
// Idle packet tests
// ============================================================================

TEST(DccPacketEncoder, idle_packet_bytes) {
    dcc_packet_t pkt;
    DccPacketEncoder_idle(&pkt);

    EXPECT_EQ(pkt.data[0], 0xFF);
    EXPECT_EQ(pkt.data[1], 0x00);
    EXPECT_EQ(pkt.data[2], 0xFF);
    EXPECT_EQ(pkt.byte_count, 3);
}

TEST(DccPacketEncoder, idle_packet_preamble) {
    dcc_packet_t pkt;
    DccPacketEncoder_idle(&pkt);

    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
}

TEST(DccPacketEncoder, idle_packet_xor_valid) {
    dcc_packet_t pkt;
    DccPacketEncoder_idle(&pkt);
    verify_xor(&pkt);
}

// ============================================================================
// Reset packet tests
// ============================================================================

TEST(DccPacketEncoder, reset_packet_bytes) {
    dcc_packet_t pkt;
    DccPacketEncoder_reset(&pkt);

    EXPECT_EQ(pkt.data[0], 0x00);
    EXPECT_EQ(pkt.data[1], 0x00);
    EXPECT_EQ(pkt.data[2], 0x00);
    EXPECT_EQ(pkt.byte_count, 3);
}

TEST(DccPacketEncoder, reset_packet_preamble) {
    dcc_packet_t pkt;
    DccPacketEncoder_reset(&pkt);

    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
}

// ============================================================================
// 128-step speed packet tests
// ============================================================================

TEST(DccPacketEncoder, speed128_short_addr_forward) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);           /* short address */
    EXPECT_EQ(pkt.data[1], 0x3F);        /* advanced ops 128-step */
    EXPECT_EQ(pkt.data[2], 0x80 | 50);   /* direction=forward, speed=50 */
    EXPECT_EQ(pkt.byte_count, 4);        /* 3 data + 1 XOR */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed128_short_addr_reverse) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, false);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 50);  /* direction=reverse (bit 7 = 0), speed=50 */
}

TEST(DccPacketEncoder, speed128_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 1234, DCC_ADDRESS_LONG, 100, true);

    EXPECT_TRUE(ok);
    /* Long address: 1234 = 0x04D2 -> byte0 = 0xC0 | 0x04 = 0xC4, byte1 = 0xD2 */
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    EXPECT_EQ(pkt.data[2], 0x3F);        /* advanced ops */
    EXPECT_EQ(pkt.data[3], 0x80 | 100);  /* forward, speed 100 */
    EXPECT_EQ(pkt.byte_count, 5);        /* 4 data + 1 XOR */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed128_broadcast_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 0, DCC_ADDRESS_BROADCAST, 0, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0);    /* broadcast address */
    EXPECT_EQ(pkt.data[1], 0x3F);
    EXPECT_EQ(pkt.data[2], 0x80); /* forward, speed=0 (stop) */
}

TEST(DccPacketEncoder, speed128_estop_value) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 1, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0x80 | 1);  /* forward, speed=1 (e-stop) */
}

TEST(DccPacketEncoder, speed128_max_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 127, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0x80 | 127);
}

TEST(DccPacketEncoder, speed128_rejects_invalid_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 128, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, speed128_rejects_invalid_address_type) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_IDLE, 50, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, speed128_rejects_accessory_address_type) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_ACCESSORY, 50, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, speed128_preamble_is_ops_mode) {
    dcc_packet_t pkt;
    DccPacketEncoder_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);

    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
}

// ============================================================================
// Emergency stop broadcast tests
// ============================================================================

TEST(DccPacketEncoder, estop_all_packet) {
    dcc_packet_t pkt;
    DccPacketEncoder_estop_all(&pkt);

    EXPECT_EQ(pkt.data[0], 0x00);  /* broadcast address */
    EXPECT_EQ(pkt.data[1], 0x3F);  /* advanced ops 128-step */
    EXPECT_EQ(pkt.data[2], 0x81);  /* forward + e-stop (speed=1) */
    EXPECT_EQ(pkt.byte_count, 4);  /* 3 data + 1 XOR */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, estop_all_preamble) {
    dcc_packet_t pkt;
    DccPacketEncoder_estop_all(&pkt);

    EXPECT_EQ(pkt.preamble_bits, DCC_PREAMBLE_BITS_OPS);
}

// ============================================================================
// 28-step speed packet tests
// ============================================================================

TEST(DccPacketEncoder, speed28_stop) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 0, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* Forward + stop: 011 00000 = 0x60 */
    EXPECT_EQ(pkt.data[1], 0x60);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_estop) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 1, true);

    EXPECT_TRUE(ok);
    /* Forward + e-stop: 011 00001 = 0x61 (C=0, SSSS=0001) */
    EXPECT_EQ(pkt.data[1], 0x61);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_step1_forward) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 2, true);

    EXPECT_TRUE(ok);
    /* Speed step 1: encoded = 2+2 = 4 = 00100, C=0, SSSS=0010 */
    /* Forward: 011 00010 = 0x62 */
    EXPECT_EQ(pkt.data[1], 0x62);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_step2_forward) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 3, true);

    EXPECT_TRUE(ok);
    /* Speed step 2: encoded = 3+2 = 5 = 00101, C=1, SSSS=0010 */
    /* Forward: 011 10010 = 0x72 */
    EXPECT_EQ(pkt.data[1], 0x72);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_max_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 29, true);

    EXPECT_TRUE(ok);
    /* Speed step 28: encoded = 29+2 = 31 = 11111, C=1, SSSS=1111 */
    /* Forward: 011 11111 = 0x7F */
    EXPECT_EQ(pkt.data[1], 0x7F);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_reverse) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 2, false);

    EXPECT_TRUE(ok);
    /* Reverse: 010 00010 = 0x42 */
    EXPECT_EQ(pkt.data[1], 0x42);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 1234, DCC_ADDRESS_LONG, 10, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    /* Speed step 9: encoded = 10+2 = 12 = 01100, C=0, SSSS=0110 */
    /* Forward: 011 00110 = 0x66 */
    EXPECT_EQ(pkt.data[2], 0x66);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed28_rejects_invalid_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 30, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, speed28_rejects_accessory_type) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_28(&pkt, 3, DCC_ADDRESS_ACCESSORY, 10, true);

    EXPECT_FALSE(ok);
}

// ============================================================================
// 14-step speed packet tests
// ============================================================================

TEST(DccPacketEncoder, speed14_stop_forward_headlight_on) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 0, true, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* Forward + FL on + stop: 011 1 0000 = 0x70 */
    EXPECT_EQ(pkt.data[1], 0x70);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed14_stop_forward_headlight_off) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 0, true, false);

    EXPECT_TRUE(ok);
    /* Forward + FL off + stop: 011 0 0000 = 0x60 */
    EXPECT_EQ(pkt.data[1], 0x60);
}

TEST(DccPacketEncoder, speed14_speed5_reverse_headlight_on) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 5, false, true);

    EXPECT_TRUE(ok);
    /* Reverse + FL on + speed 5: 010 1 0101 = 0x55 */
    EXPECT_EQ(pkt.data[1], 0x55);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed14_max_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 15, true, false);

    EXPECT_TRUE(ok);
    /* Forward + FL off + speed 15: 011 0 1111 = 0x6F */
    EXPECT_EQ(pkt.data[1], 0x6F);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed14_rejects_invalid_speed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 16, true, false);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Function Group 1 tests (FL, F1-F4)
// ============================================================================

TEST(DccPacketEncoder, func_group1_all_off) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0x00);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* 100 00000 = 0x80 */
    EXPECT_EQ(pkt.data[1], 0x80);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_group1_all_on) {
    dcc_packet_t pkt;
    /* FL=1, F4=1, F3=1, F2=1, F1=1 → bits = 11111 = 0x1F */
    bool ok = DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0x1F);

    EXPECT_TRUE(ok);
    /* 100 11111 = 0x9F */
    EXPECT_EQ(pkt.data[1], 0x9F);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_group1_fl_only) {
    dcc_packet_t pkt;
    /* FL=1 only → bit4 = 0x10 */
    bool ok = DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0x10);

    EXPECT_TRUE(ok);
    /* 100 10000 = 0x90 */
    EXPECT_EQ(pkt.data[1], 0x90);
}

TEST(DccPacketEncoder, func_group1_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_1(&pkt, 1234, DCC_ADDRESS_LONG, 0x05);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    /* 100 00101 = 0x85 */
    EXPECT_EQ(pkt.data[2], 0x85);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_group1_rejects_accessory) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_1(&pkt, 3, DCC_ADDRESS_ACCESSORY, 0x00);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Function Group 2a tests (F5-F8)
// ============================================================================

TEST(DccPacketEncoder, func_group2a_all_off) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_2a(&pkt, 3, DCC_ADDRESS_SHORT, 0x00);

    EXPECT_TRUE(ok);
    /* 1011 0000 = 0xB0 */
    EXPECT_EQ(pkt.data[1], 0xB0);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_group2a_all_on) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_2a(&pkt, 3, DCC_ADDRESS_SHORT, 0x0F);

    EXPECT_TRUE(ok);
    /* 1011 1111 = 0xBF */
    EXPECT_EQ(pkt.data[1], 0xBF);
    verify_xor(&pkt);
}

// ============================================================================
// Function Group 2b tests (F9-F12)
// ============================================================================

TEST(DccPacketEncoder, func_group2b_all_off) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_2b(&pkt, 3, DCC_ADDRESS_SHORT, 0x00);

    EXPECT_TRUE(ok);
    /* 1010 0000 = 0xA0 */
    EXPECT_EQ(pkt.data[1], 0xA0);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_group2b_all_on) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_group_2b(&pkt, 3, DCC_ADDRESS_SHORT, 0x0F);

    EXPECT_TRUE(ok);
    /* 1010 1111 = 0xAF */
    EXPECT_EQ(pkt.data[1], 0xAF);
    verify_xor(&pkt);
}

// ============================================================================
// Function expansion tests (F13-F68)
// ============================================================================

TEST(DccPacketEncoder, func_f13_f20_short_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f13_f20(&pkt, 3, DCC_ADDRESS_SHORT, 0xA5);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F13_F20);  /* 0xDE */
    EXPECT_EQ(pkt.data[2], 0xA5);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f21_f28_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f21_f28(&pkt, 1234, DCC_ADDRESS_LONG, 0xFF);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    EXPECT_EQ(pkt.data[2], DCC_FEAT_F21_F28);  /* 0xDF */
    EXPECT_EQ(pkt.data[3], 0xFF);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f29_f36_instruction_byte) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f29_f36(&pkt, 3, DCC_ADDRESS_SHORT, 0x01);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F29_F36);  /* 0xD8 */
    EXPECT_EQ(pkt.data[2], 0x01);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f37_f44_instruction_byte) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f37_f44(&pkt, 3, DCC_ADDRESS_SHORT, 0x00);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F37_F44);  /* 0xD9 */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f45_f52_instruction_byte) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f45_f52(&pkt, 3, DCC_ADDRESS_SHORT, 0x80);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F45_F52);  /* 0xDA */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f53_f60_instruction_byte) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f53_f60(&pkt, 3, DCC_ADDRESS_SHORT, 0x55);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F53_F60);  /* 0xDB */
    EXPECT_EQ(pkt.data[2], 0x55);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_f61_f68_instruction_byte) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f61_f68(&pkt, 3, DCC_ADDRESS_SHORT, 0xAA);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_F61_F68);  /* 0xDC */
    EXPECT_EQ(pkt.data[2], 0xAA);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, func_expansion_rejects_accessory) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_func_f13_f20(&pkt, 3, DCC_ADDRESS_ACCESSORY, 0x00);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Basic accessory tests
// ============================================================================

TEST(DccPacketEncoder, accessory_basic_addr5_output2_activate) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 5, 2, true);

    EXPECT_TRUE(ok);
    /* Byte 1: 10 000101 = 0x85 (address low 6 = 5) */
    EXPECT_EQ(pkt.data[0], 0x85);
    /* Byte 2: 1 111 1 010 = 0xFA (high 3 inverted = ~0 = 7, activate, output 2) */
    EXPECT_EQ(pkt.data[1], 0xFA);
    EXPECT_EQ(pkt.byte_count, 3);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_basic_addr5_output2_deactivate) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 5, 2, false);

    EXPECT_TRUE(ok);
    /* Byte 2: 1 111 0 010 = 0xF2 (deactivate) */
    EXPECT_EQ(pkt.data[1], 0xF2);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_basic_high_address) {
    dcc_packet_t pkt;
    /* Address 100: low 6 = 100 & 0x3F = 36 = 0x24, high 3 = 100 >> 6 = 1 */
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 100, 0, true);

    EXPECT_TRUE(ok);
    /* Byte 1: 10 100100 = 0xA4 */
    EXPECT_EQ(pkt.data[0], 0xA4);
    /* Byte 2: 1 110 1 000 = 0xE8 (high inverted = ~1 & 7 = 6, activate, output 0) */
    EXPECT_EQ(pkt.data[1], 0xE8);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_basic_max_address) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 511, 7, true);

    EXPECT_TRUE(ok);
    /* Address 511: low 6 = 0x3F, high 3 = 7 */
    EXPECT_EQ(pkt.data[0], 0x80 | 0x3F);   /* 0xBF */
    /* High inverted = ~7 & 7 = 0, activate, output 7 */
    EXPECT_EQ(pkt.data[1], 0x80 | 0x08 | 0x07);  /* 0x8F */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_basic_rejects_invalid_address) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 512, 0, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, accessory_basic_rejects_invalid_output) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic(&pkt, 5, 8, true);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Extended accessory tests
// ============================================================================

TEST(DccPacketEncoder, accessory_extended_addr0_aspect5) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended(&pkt, 0, 5);

    EXPECT_TRUE(ok);
    /* Byte 1: 10 000000 = 0x80 */
    EXPECT_EQ(pkt.data[0], 0x80);
    /* Byte 2: 0 111 0 00 1 = 0x71 (high inverted = ~0 & 7 = 7, bits 9-10 = 0) */
    EXPECT_EQ(pkt.data[1], 0x71);
    /* Byte 3: aspect = 5 */
    EXPECT_EQ(pkt.data[2], 0x05);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_extended_addr100_aspect0) {
    dcc_packet_t pkt;
    /* Address 100: low 6 = 0x24, bits 6-8 = 1, bits 9-10 = 0 */
    bool ok = DccPacketEncoder_accessory_extended(&pkt, 100, 0);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xA4);
    /* Byte 2: 0 110 0 00 1 = 0x61 (high inverted = ~1 & 7 = 6) */
    EXPECT_EQ(pkt.data[1], 0x61);
    EXPECT_EQ(pkt.data[2], 0x00);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, accessory_extended_rejects_invalid_address) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended(&pkt, 2048, 0);

    EXPECT_FALSE(ok);
}

// ============================================================================
// CV ops-mode write tests
// ============================================================================

TEST(DccPacketEncoder, cv_write_ops_short_addr) {
    dcc_packet_t pkt;
    /* Write value 200 to CV 1 of decoder at short address 3 */
    bool ok = DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1, 200);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* CV 1 → wire 0 = 0x000. Instruction: 1110 1100 | 0x00 = 0xEC */
    EXPECT_EQ(pkt.data[1], 0xEC);
    /* CV low byte = 0x00 */
    EXPECT_EQ(pkt.data[2], 0x00);
    /* Value = 200 */
    EXPECT_EQ(pkt.data[3], 200);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, cv_write_ops_cv1024_long_addr) {
    dcc_packet_t pkt;
    /* Write value 0xFF to CV 1024 of long address 5000 */
    bool ok = DccPacketEncoder_cv_write_ops(&pkt, 5000, DCC_ADDRESS_LONG, 1024, 0xFF);

    EXPECT_TRUE(ok);
    /* Long addr 5000 = 0x1388 → byte0 = 0xC0 | 0x13 = 0xD3, byte1 = 0x88 */
    EXPECT_EQ(pkt.data[0], 0xD3);
    EXPECT_EQ(pkt.data[1], 0x88);
    /* CV 1024 → wire 1023 = 0x3FF. Instruction: 0xEC | 0x03 = 0xEF */
    EXPECT_EQ(pkt.data[2], 0xEF);
    /* CV low byte = 0xFF */
    EXPECT_EQ(pkt.data[3], 0xFF);
    /* Value = 0xFF */
    EXPECT_EQ(pkt.data[4], 0xFF);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, cv_write_ops_rejects_cv0) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, 0, 100);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, cv_write_ops_rejects_cv1025) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1025, 100);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, cv_write_ops_rejects_accessory) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_write_ops(&pkt, 3, DCC_ADDRESS_ACCESSORY, 1, 100);

    EXPECT_FALSE(ok);
}

// ============================================================================
// CV ops-mode verify tests
// ============================================================================

TEST(DccPacketEncoder, cv_verify_ops_short_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_verify_ops(&pkt, 3, DCC_ADDRESS_SHORT, 29, 0x25);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* CV 29 → wire 28 = 0x01C. Instruction: 0xE4 | 0x00 = 0xE4 */
    EXPECT_EQ(pkt.data[1], 0xE4);
    /* CV low byte = 28 */
    EXPECT_EQ(pkt.data[2], 28);
    /* Value = 0x25 */
    EXPECT_EQ(pkt.data[3], 0x25);
    verify_xor(&pkt);
}

// ============================================================================
// CV ops-mode bit manipulation tests
// ============================================================================

TEST(DccPacketEncoder, cv_bit_ops_write_bit3_high) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 29, 3, true, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    /* CV 29 → wire 28. Instruction: 0xE8 | 0x00 = 0xE8 */
    EXPECT_EQ(pkt.data[1], 0xE8);
    /* CV low byte = 28 */
    EXPECT_EQ(pkt.data[2], 28);
    /* Bit byte: 111 1 1 011 = 0xFB (write=1, value=1, position=3) */
    EXPECT_EQ(pkt.data[3], 0xFB);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, cv_bit_ops_verify_bit0_low) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1, 0, false, false);

    EXPECT_TRUE(ok);
    /* Bit byte: 111 0 0 000 = 0xE0 (verify, value=0, position=0) */
    EXPECT_EQ(pkt.data[3], 0xE0);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, cv_bit_ops_rejects_bit_position_8) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1, 8, true, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, cv_bit_ops_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_cv_bit_ops(&pkt, 1234, DCC_ADDRESS_LONG, 512, 7, true, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    /* CV 512 → wire 511 = 0x1FF. Instruction: 0xE8 | 0x01 = 0xE9 */
    EXPECT_EQ(pkt.data[2], 0xE9);
    /* CV low byte = 0xFF */
    EXPECT_EQ(pkt.data[3], 0xFF);
    /* Bit byte: 111 1 1 111 = 0xFF (write, value=1, position=7) */
    EXPECT_EQ(pkt.data[4], 0xFF);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);
}

// ============================================================================
// Consist tests
// ============================================================================

TEST(DccPacketEncoder, consist_set_normal) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_NORMAL);  /* 0x12 */
    EXPECT_EQ(pkt.data[2], 50);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, consist_set_reversed) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 50, false);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_REVERSED);  /* 0x13 */
    EXPECT_EQ(pkt.data[2], 50);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, consist_set_max_address) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 127, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 127);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, consist_set_rejects_invalid_consist_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 128, true);

    EXPECT_FALSE(ok);
}

TEST(DccPacketEncoder, consist_clear) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_clear(&pkt, 3, DCC_ADDRESS_SHORT);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[1], DCC_CONSIST_SET_NORMAL);  /* 0x12 */
    EXPECT_EQ(pkt.data[2], 0);  /* address 0 = clear */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, consist_set_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_consist_set(&pkt, 1234, DCC_ADDRESS_LONG, 10, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    EXPECT_EQ(pkt.data[2], DCC_CONSIST_SET_NORMAL);
    EXPECT_EQ(pkt.data[3], 10);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

// ============================================================================
// Binary state short form tests
// ============================================================================

TEST(DccPacketEncoder, binary_state_short_activate) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, 42, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_BINARY_STATE_SHORT);  /* 0xDD */
    /* Data: 1 0101010 = 0xAA (active + state 42) */
    EXPECT_EQ(pkt.data[2], 0x80 | 42);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, binary_state_short_deactivate) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, 42, false);

    EXPECT_TRUE(ok);
    /* Data: 0 0101010 = 42 (inactive + state 42) */
    EXPECT_EQ(pkt.data[2], 42);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, binary_state_short_rejects_invalid_state) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, 128, true);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Binary state long form tests
// ============================================================================

TEST(DccPacketEncoder, binary_state_long_activate) {
    dcc_packet_t pkt;
    /* State 1000: low 7 = 1000 & 0x7F = 104, high 8 = 1000 >> 7 = 7 */
    bool ok = DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, 1000, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_FEAT_BINARY_STATE_LONG);  /* 0xDC */
    /* Low byte: 1 1101000 = 0x80 | 104 = 0xE8 */
    EXPECT_EQ(pkt.data[2], 0x80 | 104);
    /* High byte: 7 */
    EXPECT_EQ(pkt.data[3], 7);
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, binary_state_long_max_state) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, 32767, true);

    EXPECT_TRUE(ok);
    /* 32767 = 0x7FFF: low 7 = 0x7F, high 8 = 0xFF */
    EXPECT_EQ(pkt.data[2], 0x80 | 0x7F);  /* 0xFF */
    EXPECT_EQ(pkt.data[3], 0xFF);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, binary_state_long_deactivate) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, 0, false);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0x00);  /* inactive, state 0 */
    EXPECT_EQ(pkt.data[3], 0x00);
    verify_xor(&pkt);
}

// ============================================================================
// Analog function tests
// ============================================================================

TEST(DccPacketEncoder, analog_function_volume) {
    dcc_packet_t pkt;
    /* Output 1 = volume, value = 128 */
    bool ok = DccPacketEncoder_analog_function(&pkt, 3, DCC_ADDRESS_SHORT, 1, 128);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_ANALOG_FUNCTION);  /* 0x3D */
    EXPECT_EQ(pkt.data[2], 1);    /* output number */
    EXPECT_EQ(pkt.data[3], 128);  /* value */
    EXPECT_EQ(pkt.byte_count, 5);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, analog_function_long_addr) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_analog_function(&pkt, 1234, DCC_ADDRESS_LONG, 0, 255);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 0xC4);
    EXPECT_EQ(pkt.data[1], 0xD2);
    EXPECT_EQ(pkt.data[2], DCC_ADV_OPS_ANALOG_FUNCTION);
    EXPECT_EQ(pkt.data[3], 0);
    EXPECT_EQ(pkt.data[4], 255);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);
}

// ============================================================================
// Speed restriction tests
// ============================================================================

TEST(DccPacketEncoder, speed_restriction_enabled) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, true, 64);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[0], 3);
    EXPECT_EQ(pkt.data[1], DCC_ADV_OPS_SPEED_RESTRICTION);  /* 0x3E */
    /* Data: 1 1000000 = 0xC0 (enabled + limit 64) */
    EXPECT_EQ(pkt.data[2], 0x80 | 64);
    EXPECT_EQ(pkt.byte_count, 4);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed_restriction_disabled) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, false, 0);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0x00);  /* disabled + limit 0 */
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, speed_restriction_rejects_invalid_limit) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_speed_restriction(&pkt, 3, DCC_ADDRESS_SHORT, true, 128);

    EXPECT_FALSE(ok);
}

// ============================================================================
// Branch coverage: rejects accessory address type
// ============================================================================

TEST(DccPacketEncoder, speed14_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_14(&pkt, 1, DCC_ADDRESS_ACCESSORY, 5, true, false));
}

TEST(DccPacketEncoder, func_group2a_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_group_2a(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0));
}

TEST(DccPacketEncoder, func_group2b_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_func_group_2b(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0));
}

TEST(DccPacketEncoder, cv_bit_ops_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, 0, true, true));
}

TEST(DccPacketEncoder, cv_verify_ops_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_verify_ops(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, 0));
}

TEST(DccPacketEncoder, consist_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_consist_set(&pkt, 1, DCC_ADDRESS_ACCESSORY, 10, true));
}

TEST(DccPacketEncoder, binary_state_long_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_long(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, true));
}

TEST(DccPacketEncoder, analog_function_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_analog_function(&pkt, 1, DCC_ADDRESS_ACCESSORY, 0, 0));
}

TEST(DccPacketEncoder, speed_restriction_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_speed_restriction(&pkt, 1, DCC_ADDRESS_ACCESSORY, true, 60));
}

// ============================================================================
// Branch coverage: CV bit ops validation edge cases
// ============================================================================

TEST(DccPacketEncoder, binary_state_short_rejects_accessory) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_short(&pkt, 1, DCC_ADDRESS_ACCESSORY, 1, true));
}

TEST(DccPacketEncoder, cv_bit_ops_rejects_cv0) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 0, 0, true, true));
}

TEST(DccPacketEncoder, cv_bit_ops_rejects_cv1025) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_cv_bit_ops(&pkt, 3, DCC_ADDRESS_SHORT, 1025, 0, true, true));
}

// ============================================================================
// Branch coverage: binary state long validation edge case
// ============================================================================

TEST(DccPacketEncoder, binary_state_long_rejects_state_too_large) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_binary_state_long(&pkt, 3, DCC_ADDRESS_SHORT, 32768, true));
}

// ============================================================================
// Basic accessory CV ops-mode write
// ============================================================================

TEST(DccPacketEncoder, acc_basic_cv_write_addr5_pair2_cv1) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic_cv_write(&pkt, 5, 2, 1, 200);

    EXPECT_TRUE(ok);
    /* Byte 0: 10 000101 = 0x85 (address low 6 = 5) */
    EXPECT_EQ(pkt.data[0], 0x85);
    /* Byte 1: 1 111 1 10 0 = 0xFC (high inv=~0&7=7, bit3=1, A1A0=10, bit0=0) */
    EXPECT_EQ(pkt.data[1], 0xFC);
    /* Byte 2: 0xEC (CV long write, CV high = 00) */
    EXPECT_EQ(pkt.data[2], 0xEC);
    /* Byte 3: CV low = 0x00 (wire CV 0 → CV 1) */
    EXPECT_EQ(pkt.data[3], 0x00);
    /* Byte 4: value = 200 */
    EXPECT_EQ(pkt.data[4], 200);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_basic_cv_write_cv1024) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic_cv_write(&pkt, 100, 0, 1024, 0xFF);

    EXPECT_TRUE(ok);
    /* CV 1024 → wire 1023 = 0x3FF → high = 0x03, low = 0xFF */
    EXPECT_EQ(pkt.data[2], 0xEC | 0x03);  /* 0xEF */
    EXPECT_EQ(pkt.data[3], 0xFF);
    EXPECT_EQ(pkt.data[4], 0xFF);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_basic_cv_write_rejects_bad_address) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 512, 0, 1, 0));
}

TEST(DccPacketEncoder, acc_basic_cv_write_rejects_bad_pair) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 5, 4, 1, 0));
}

TEST(DccPacketEncoder, acc_basic_cv_write_rejects_cv0) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 5, 0, 0, 0));
}

TEST(DccPacketEncoder, acc_basic_cv_write_rejects_cv1025) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_write(&pkt, 5, 0, 1025, 0));
}

// ============================================================================
// Basic accessory CV ops-mode verify
// ============================================================================

TEST(DccPacketEncoder, acc_basic_cv_verify_cv29) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic_cv_verify(&pkt, 5, 0, 29, 0x25);

    EXPECT_TRUE(ok);
    /* Byte 2: 0xE4 (CV long verify) */
    EXPECT_EQ(pkt.data[2], 0xE4);
    /* CV 29 → wire 28 → low = 28 */
    EXPECT_EQ(pkt.data[3], 28);
    EXPECT_EQ(pkt.data[4], 0x25);
    verify_xor(&pkt);
}

// ============================================================================
// Basic accessory CV ops-mode bit manipulation
// ============================================================================

TEST(DccPacketEncoder, acc_basic_cv_bit_write_bit3_high) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic_cv_bit(&pkt, 5, 1, 29, 3, true, true);

    EXPECT_TRUE(ok);
    /* Byte 2: 0xE8 (CV long bit) */
    EXPECT_EQ(pkt.data[2], 0xE8);
    /* CV 29 → wire 28 → low = 28 */
    EXPECT_EQ(pkt.data[3], 28);
    /* Bit byte: 111 1 1 011 = 0xFB (write=1, value=1, position=3) */
    EXPECT_EQ(pkt.data[4], 0xFB);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_basic_cv_bit_verify_bit0_low) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_basic_cv_bit(&pkt, 5, 0, 1, 0, false, false);

    EXPECT_TRUE(ok);
    /* Bit byte: 111 0 0 000 = 0xE0 (verify, value=0, position=0) */
    EXPECT_EQ(pkt.data[4], 0xE0);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_basic_cv_bit_rejects_bit8) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_basic_cv_bit(&pkt, 5, 0, 1, 8, true, true));
}

// ============================================================================
// Extended accessory CV ops-mode write
// ============================================================================

TEST(DccPacketEncoder, acc_extended_cv_write_addr0_cv1) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended_cv_write(&pkt, 0, 1, 200);

    EXPECT_TRUE(ok);
    /* Byte 0: 10 000000 = 0x80 */
    EXPECT_EQ(pkt.data[0], 0x80);
    /* Byte 1: 0 111 0 00 1 = 0x71 (high inv=~0&7=7, bits 9-10=0) */
    EXPECT_EQ(pkt.data[1], 0x71);
    /* Byte 2: 0xEC (CV long write) */
    EXPECT_EQ(pkt.data[2], 0xEC);
    /* Byte 3: CV low = 0x00 */
    EXPECT_EQ(pkt.data[3], 0x00);
    /* Byte 4: value */
    EXPECT_EQ(pkt.data[4], 200);
    EXPECT_EQ(pkt.byte_count, 6);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_extended_cv_write_addr100_cv1024) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended_cv_write(&pkt, 100, 1024, 0xFF);

    EXPECT_TRUE(ok);
    /* Address 100: low 6 = 0x24, bits 6-8 = 1 */
    EXPECT_EQ(pkt.data[0], 0xA4);
    /* Byte 1: high inv=~1&7=6 → 0110, bits 9-10=0 */
    EXPECT_EQ(pkt.data[1], 0x61);
    /* CV 1024 → wire 1023 = 0x3FF */
    EXPECT_EQ(pkt.data[2], 0xEF);
    EXPECT_EQ(pkt.data[3], 0xFF);
    EXPECT_EQ(pkt.data[4], 0xFF);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_extended_cv_write_rejects_bad_address) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 2048, 1, 0));
}

TEST(DccPacketEncoder, acc_extended_cv_write_rejects_cv0) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 0, 0, 0));
}

TEST(DccPacketEncoder, acc_extended_cv_write_rejects_cv1025) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_write(&pkt, 0, 1025, 0));
}

// ============================================================================
// Extended accessory CV ops-mode verify
// ============================================================================

TEST(DccPacketEncoder, acc_extended_cv_verify_cv29) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended_cv_verify(&pkt, 0, 29, 0x25);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0xE4);
    EXPECT_EQ(pkt.data[3], 28);
    EXPECT_EQ(pkt.data[4], 0x25);
    verify_xor(&pkt);
}

// ============================================================================
// Extended accessory CV ops-mode bit manipulation
// ============================================================================

TEST(DccPacketEncoder, acc_extended_cv_bit_write_bit7_high) {
    dcc_packet_t pkt;
    bool ok = DccPacketEncoder_accessory_extended_cv_bit(&pkt, 0, 1, 7, true, true);

    EXPECT_TRUE(ok);
    EXPECT_EQ(pkt.data[2], 0xE8);
    EXPECT_EQ(pkt.data[3], 0x00);
    /* Bit byte: 111 1 1 111 = 0xFF (write=1, value=1, position=7) */
    EXPECT_EQ(pkt.data[4], 0xFF);
    verify_xor(&pkt);
}

TEST(DccPacketEncoder, acc_extended_cv_bit_rejects_bit8) {
    dcc_packet_t pkt;
    EXPECT_FALSE(DccPacketEncoder_accessory_extended_cv_bit(&pkt, 0, 1, 8, true, true));
}
