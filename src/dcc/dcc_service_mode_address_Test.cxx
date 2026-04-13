/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Address (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_address.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_service_mode_address_context_t test_context;

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
    begin_operation_count = 0;
    begin_operation_return = true;
    last_is_write_operation = false;
    last_recovery_count = 0;
    common_idle_value = true;

    complete_result = DCC_SERVICE_MODE_ERROR;
    complete_count = 0;

}

static interface_dcc_service_mode_address_t make_interface(void) {

    interface_dcc_service_mode_address_t interface;
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

TEST(DccServiceModeAddress, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

}

// ============================================================================
// Write tests
// ============================================================================

TEST(DccServiceModeAddress, write_address_1) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_write(&test_context, 1));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* Write register 1 (index 0): 0111 1 000 = 0x78 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0x00));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)1);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)3);
    verify_xor(&last_begin_packet);
    EXPECT_EQ(last_begin_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

TEST(DccServiceModeAddress, write_address_127) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_write(&test_context, 127));

    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0x00));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)127);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeAddress, write_address_42) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_write(&test_context, 42));

    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)42);
    verify_xor(&last_begin_packet);

}

// ============================================================================
// Guard tests
// ============================================================================

TEST(DccServiceModeAddress, write_address_0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeAddress_write(&test_context, 0));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeAddress, write_address_128_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeAddress_write(&test_context, 128));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeAddress, write_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeAddress_write(&test_context, 1));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// Callback forwarding tests
// ============================================================================

TEST(DccServiceModeAddress, callback_forwards_success) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_write(&test_context, 1);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_SUCCESS);

}

TEST(DccServiceModeAddress, callback_forwards_no_ack) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_write(&test_context, 1);

    last_begin_callback(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_NO_ACK);

}

TEST(DccServiceModeAddress, callback_with_null_on_complete_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    interface.on_complete = NULL;
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_write(&test_context, 1);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)0);

}

// ============================================================================
// Recovery flags: address write is always a write with 10-packet recovery
// ============================================================================

TEST(DccServiceModeAddress, write_passes_write_flag_and_10_recovery) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_write(&test_context, 3);

    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)10);

}

// ============================================================================
// Verify tests
// ============================================================================

TEST(DccServiceModeAddress, verify_address_1) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_verify(&test_context, 1));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* Verify register 1 (index 0): 0111 0 000 = 0x70 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_VERIFY_PREFIX | 0x00));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)1);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)3);
    verify_xor(&last_begin_packet);
    EXPECT_EQ(last_begin_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

TEST(DccServiceModeAddress, verify_address_127) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_verify(&test_context, 127));

    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_VERIFY_PREFIX | 0x00));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)127);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeAddress, verify_address_42) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeAddress_verify(&test_context, 42));

    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)42);
    verify_xor(&last_begin_packet);

}

// ============================================================================
// Verify guard tests
// ============================================================================

TEST(DccServiceModeAddress, verify_address_0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeAddress_verify(&test_context, 0));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeAddress, verify_address_128_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeAddress_verify(&test_context, 128));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeAddress, verify_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeAddress_verify(&test_context, 1));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// Verify callback forwarding tests
// ============================================================================

TEST(DccServiceModeAddress, verify_callback_forwards_success) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_verify(&test_context, 1);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_SUCCESS);

}

TEST(DccServiceModeAddress, verify_callback_forwards_no_ack) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_verify(&test_context, 1);

    last_begin_callback(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// Verify recovery flags: verify is not a write, 0 recovery packets
// ============================================================================

TEST(DccServiceModeAddress, verify_passes_verify_flag_and_0_recovery) {

    reset_mocks();
    interface_dcc_service_mode_address_t interface = make_interface();
    DccServiceModeAddress_initialize(&test_context, &interface);

    DccServiceModeAddress_verify(&test_context, 3);

    EXPECT_FALSE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)0);

}
