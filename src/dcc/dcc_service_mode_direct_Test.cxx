/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Direct (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_direct.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_service_mode_direct_context_t test_context;

static dcc_packet_t last_begin_packet;
static dcc_service_mode_step_callback_t last_begin_callback;
static uint32_t begin_operation_count;
static bool begin_operation_return;
static bool common_idle_value;

static dcc_service_mode_result_t complete_result;
static uint32_t complete_count;

static bool last_is_write_operation;
static uint8_t last_recovery_count;

static bool mock_begin_operation(const dcc_packet_t *packet,
                                  dcc_service_mode_step_callback_t callback,
                                  bool is_write_operation,
                                  uint8_t recovery_count) {

    memcpy(&last_begin_packet, packet, sizeof(dcc_packet_t));
    last_begin_callback = callback;
    last_is_write_operation = is_write_operation;
    last_recovery_count = recovery_count;
    begin_operation_count++;

    return begin_operation_return;

}

static bool mock_is_common_idle(void) {

    return common_idle_value;

}

static void mock_on_complete(dcc_service_mode_result_t result) {

    complete_result = result;
    complete_count++;

}

static void reset_mocks(void) {

    memset(&test_context, 0, sizeof(test_context));
    memset(&last_begin_packet, 0, sizeof(last_begin_packet));
    last_begin_callback = NULL;
    last_is_write_operation = false;
    last_recovery_count = 0;
    begin_operation_count = 0;
    begin_operation_return = true;
    common_idle_value = true;

    complete_result = DCC_SERVICE_MODE_ERROR;
    complete_count = 0;

}

static interface_dcc_service_mode_direct_t make_interface(void) {

    interface_dcc_service_mode_direct_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.begin_operation = mock_begin_operation;
    interface.is_common_idle = mock_is_common_idle;
    interface.on_complete = mock_on_complete;

    return interface;

}

// ============================================================================
// Helper: verify XOR byte
// ============================================================================

static void verify_xor(const dcc_packet_t *packet) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count - 1; byte_index++) {

        xor_byte ^= packet->data[byte_index];

    }

    EXPECT_EQ(packet->data[packet->byte_count - 1], xor_byte);

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccServiceModeDirect, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

}

// ============================================================================
// write_byte tests
// ============================================================================

TEST(DccServiceModeDirect, write_byte_cv1_value_0x55) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_write_byte(&test_context,1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* CV 1 -> wire CV 0 -> high bits = 0x00 */
    /* 0111 1100 = 0x7C | 0x00 = 0x7C */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)0x7C);
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x00);
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0x55);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)4);
    verify_xor(&last_begin_packet);
    EXPECT_EQ(last_begin_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

TEST(DccServiceModeDirect, write_byte_cv1024_value_0xAA) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_write_byte(&test_context,1024, 0xAA));

    /* CV 1024 -> wire CV 1023 = 0x03FF -> high bits = 0x03 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(0x7C | 0x03));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0xFF);
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0xAA);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, write_byte_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_write_byte(&test_context,0, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, write_byte_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_write_byte(&test_context,1025, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, write_byte_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeDirect_write_byte(&test_context,1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// verify_byte tests
// ============================================================================

TEST(DccServiceModeDirect, verify_byte_cv1_value_0x55) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_verify_byte(&test_context,1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* 0111 0100 = 0x74 | 0x00 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)0x74);
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x00);
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0x55);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)4);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, verify_byte_cv256) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_verify_byte(&test_context,256, 0x00));

    /* CV 256 -> wire CV 255 = 0x00FF -> high bits = 0x00 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)0x74);
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0xFF);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, verify_byte_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_verify_byte(&test_context,0, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, verify_byte_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_verify_byte(&test_context,1025, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, verify_byte_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeDirect_verify_byte(&test_context,1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// write_bit tests
// ============================================================================

TEST(DccServiceModeDirect, write_bit_cv1_bit3_high) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_write_bit(&test_context,1, 3, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* 0111 1000 = 0x78 | 0x00 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)0x78);
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x00);
    /* 111 K=1 D=1 BBB=011 = 1111 1011 = 0xFB */
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0xFB);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)4);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, write_bit_cv1_bit0_low) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_write_bit(&test_context,1, 0, false));

    /* 111 K=1 D=0 BBB=000 = 1111 0000 = 0xF0 */
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0xF0);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, write_bit_rejects_position_8) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_write_bit(&test_context,1, 8, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, write_bit_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_write_bit(&test_context,0, 3, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, write_bit_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_write_bit(&test_context,1025, 3, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, write_bit_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeDirect_write_bit(&test_context,1, 3, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// verify_bit tests
// ============================================================================

TEST(DccServiceModeDirect, verify_bit_cv1_bit5_high) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_verify_bit(&test_context,1, 5, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)0x78);
    /* 111 K=0 D=1 BBB=101 = 1110 1101 = 0xED */
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0xED);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, verify_bit_cv1_bit7_low) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeDirect_verify_bit(&test_context,1, 7, false));

    /* 111 K=0 D=0 BBB=111 = 1110 0111 = 0xE7 */
    EXPECT_EQ(last_begin_packet.data[2], (uint8_t)0xE7);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeDirect, verify_bit_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_verify_bit(&test_context,0, 5, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, verify_bit_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_verify_bit(&test_context,1025, 5, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, verify_bit_rejects_position_8) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeDirect_verify_bit(&test_context,1, 8, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeDirect, verify_bit_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeDirect_verify_bit(&test_context,1, 5, true));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccServiceModeDirect, null_on_complete_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    interface.on_complete = NULL;
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_write_byte(&test_context,1, 0x55);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)0);

}

// ============================================================================
// Callback forwarding tests
// ============================================================================

TEST(DccServiceModeDirect, callback_forwards_success) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_write_byte(&test_context,1, 0x55);

    /* Simulate common module completing with success */
    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_SUCCESS);

}

TEST(DccServiceModeDirect, callback_forwards_no_ack) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_write_byte(&test_context,1, 0x55);

    last_begin_callback(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// Recovery flags: write operations pass is_write=true, recovery=6
// ============================================================================

TEST(DccServiceModeDirect, write_byte_passes_write_flag_and_recovery_count) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_write_byte(&test_context, 1, 0x55);

    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

TEST(DccServiceModeDirect, write_bit_passes_write_flag_and_recovery_count) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_write_bit(&test_context, 1, 3, true);

    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

// ============================================================================
// Recovery flags: verify operations pass is_write=false, recovery=0
// ============================================================================

TEST(DccServiceModeDirect, verify_byte_passes_verify_flag_and_zero_recovery) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_verify_byte(&test_context, 1, 0x55);

    EXPECT_FALSE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)0);

}

TEST(DccServiceModeDirect, verify_bit_passes_verify_flag_and_zero_recovery) {

    reset_mocks();
    interface_dcc_service_mode_direct_t interface = make_interface();
    DccServiceModeDirect_initialize(&test_context, &interface);

    DccServiceModeDirect_verify_bit(&test_context, 1, 5, true);

    EXPECT_FALSE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)0);

}
