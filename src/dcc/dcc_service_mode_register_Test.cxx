/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Register (Phase 4)
 *
 * Per S-9.2.3 each register operation is a two-step sequence: a page-preset
 * (page register -> page 1) followed by the register verify/write. The register
 * command is sent unconditionally after the preset. Register VERIFY uses 7+
 * command packets; a write to Register 1 uses the longer 10-packet recovery.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_register.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_service_mode_register_context_t test_context;
static dcc_packet_t last_begin_packet;
static dcc_service_mode_step_callback_t last_begin_callback;
static uint32_t begin_operation_count;
static bool begin_operation_return;
static bool common_idle_value;

static dcc_service_mode_result_t complete_result;
static uint32_t complete_count;

static bool last_is_write_operation;
static uint8_t last_command_repeat;
static uint8_t last_recovery_count;

static bool mock_begin_operation(const dcc_packet_t *packet,
                                  dcc_service_mode_step_callback_t callback,
                                  bool is_write_operation,
                                  uint8_t command_repeat,
                                  uint8_t recovery_count) {

    memcpy(&last_begin_packet, packet, sizeof(dcc_packet_t));
    last_begin_callback = callback;
    last_is_write_operation = is_write_operation;
    last_command_repeat = command_repeat;
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
    last_command_repeat = 0;
    last_recovery_count = 0;
    common_idle_value = true;

    complete_result = DCC_SERVICE_MODE_ERROR;
    complete_count = 0;

}

static interface_dcc_service_mode_register_t make_interface(void) {

    interface_dcc_service_mode_register_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.begin_operation = mock_begin_operation;
    interface.is_common_idle = mock_is_common_idle;
    interface.on_complete = mock_on_complete;

    return interface;

}

// ============================================================================
// Helpers
// ============================================================================

static void verify_xor(const dcc_packet_t *packet) {

    uint8_t xor_byte = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < packet->byte_count - 1; byte_index++) {

        xor_byte ^= packet->data[byte_index];

    }

    EXPECT_EQ(packet->data[packet->byte_count - 1], xor_byte);

}

/* The page-preset is a write of page 1 to the page register (6): 0x7D 0x01 0x7C. */
static void expect_page_preset_packet(void) {

    EXPECT_EQ(last_begin_packet.data[0],
              (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | ((DCC_SERVICE_MODE_PAGE_REGISTER - 1) & 0x07)));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)DCC_SERVICE_MODE_PAGE_PRESET_PAGE);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)3);
    verify_xor(&last_begin_packet);

}

/* Fire the page-preset's completion so the register command step begins. */
static void advance_to_command(dcc_service_mode_result_t preset_result) {

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(preset_result);

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccServiceModeRegister, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

}

// ============================================================================
// Page-preset precedes every operation (S-9.2.3)
// ============================================================================

// @compliance DCC-S9.2.3-CS-016
TEST(DccServiceModeRegister, write_starts_with_page_preset) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_write(&test_context, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);
    expect_page_preset_packet();

    /* preset is a write: 5 packets, 6 recovery */
    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_command_repeat, (uint8_t)DCC_SERVICE_MODE_COMMAND_REPEAT);
    EXPECT_EQ(last_recovery_count, (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

TEST(DccServiceModeRegister, verify_starts_with_page_preset) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_verify(&test_context, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);
    expect_page_preset_packet();
    EXPECT_TRUE(last_is_write_operation);   /* the preset itself is a write */

}

// ============================================================================
// Register command packet (after the preset)
// ============================================================================

TEST(DccServiceModeRegister, write_register1_value_0x55) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_write(&test_context, 1, 0x55));
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(begin_operation_count, (uint32_t)2);

    /* 0111 1 RRR where RRR = 1-1 = 0 -> 0x78 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x55);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)3);
    verify_xor(&last_begin_packet);
    EXPECT_EQ(last_begin_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

TEST(DccServiceModeRegister, write_register8_value_0xAA) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_write(&test_context, 8, 0xAA));
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    /* RRR = 8-1 = 7 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 7));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0xAA);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeRegister, write_register0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeRegister_write(&test_context, 0, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeRegister, write_register9_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeRegister_write(&test_context, 9, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeRegister, write_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeRegister_write(&test_context, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeRegister, verify_register1_value_0x55) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_verify(&test_context, 1, 0x55));
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(begin_operation_count, (uint32_t)2);

    /* 0111 0 RRR where RRR = 1-1 = 0 -> 0x70 */
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_VERIFY_PREFIX | 0));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x55);
    EXPECT_EQ(last_begin_packet.byte_count, (uint8_t)3);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeRegister, verify_register8_value_0xFF) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_verify(&test_context, 8, 0xFF));
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_VERIFY_PREFIX | 7));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0xFF);
    verify_xor(&last_begin_packet);

}

TEST(DccServiceModeRegister, verify_register0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeRegister_verify(&test_context, 0, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeRegister, verify_register9_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_FALSE(DccServiceModeRegister_verify(&test_context, 9, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModeRegister, verify_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModeRegister_verify(&test_context, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

// ============================================================================
// Command proceeds after the preset regardless of the preset's ACK result
// ============================================================================

// @compliance DCC-S9.2.3-CS-017
TEST(DccServiceModeRegister, command_proceeds_even_if_preset_no_ack) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeRegister_write(&test_context, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* Preset returns NO_ACK -- the register command must still be sent. */
    advance_to_command(DCC_SERVICE_MODE_NO_ACK);
    EXPECT_EQ(begin_operation_count, (uint32_t)2);
    EXPECT_EQ(last_begin_packet.data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0));
    EXPECT_EQ(last_begin_packet.data[1], (uint8_t)0x55);

}

// ============================================================================
// Repeat / recovery counts (S-9.2.3)
// ============================================================================

// @compliance DCC-S9.2.3-CS-012
TEST(DccServiceModeRegister, write_register1_uses_long_recovery) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_write(&test_context, 1, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_command_repeat, (uint8_t)DCC_SERVICE_MODE_COMMAND_REPEAT);
    EXPECT_EQ(last_recovery_count, (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT_LONG);

}

// @compliance DCC-S9.2.3-CS-013
TEST(DccServiceModeRegister, write_register2_uses_standard_recovery) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_write(&test_context, 2, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_TRUE(last_is_write_operation);
    EXPECT_EQ(last_recovery_count, (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

// @compliance DCC-S9.2.3-CS-011
TEST(DccServiceModeRegister, verify_uses_seven_repeats_zero_recovery) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_verify(&test_context, 1, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_FALSE(last_is_write_operation);
    EXPECT_EQ(last_command_repeat, (uint8_t)DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT);
    EXPECT_EQ(last_recovery_count, (uint8_t)0);

}

// ============================================================================
// Callback forwarding (the command step's completion is what forwards)
// ============================================================================

TEST(DccServiceModeRegister, callback_forwards_success) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_write(&test_context, 1, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_SUCCESS);

}

TEST(DccServiceModeRegister, callback_forwards_no_ack) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_verify(&test_context, 1, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    last_begin_callback(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_NO_ACK);

}

TEST(DccServiceModeRegister, callback_null_on_complete_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_register_t interface = make_interface();
    interface.on_complete = NULL;
    DccServiceModeRegister_initialize(&test_context, &interface);

    DccServiceModeRegister_write(&test_context, 1, 0x55);
    advance_to_command(DCC_SERVICE_MODE_SUCCESS);

    ASSERT_NE(last_begin_callback, (dcc_service_mode_step_callback_t)NULL);
    last_begin_callback(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)0);

}
