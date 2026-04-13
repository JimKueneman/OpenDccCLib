/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Paged (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_paged.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Instance context
// ============================================================================

static dcc_service_mode_paged_context_t test_ctx;

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_packet_t captured_packets[8];
static dcc_service_mode_step_callback_t captured_callbacks[8];
static uint32_t begin_operation_count;
static bool begin_operation_return;
static bool common_idle_value;

static dcc_service_mode_result_t complete_result;
static uint32_t complete_count;

static bool captured_is_write[8];
static uint8_t captured_recovery_count[8];

static bool mock_begin_operation(const dcc_packet_t *packet,
                                  dcc_service_mode_step_callback_t callback,
                                  bool is_write_operation,
                                  uint8_t recovery_count) {

    if (begin_operation_count < 8) {

        memcpy(&captured_packets[begin_operation_count], packet, sizeof(dcc_packet_t));
        captured_callbacks[begin_operation_count] = callback;
        captured_is_write[begin_operation_count] = is_write_operation;
        captured_recovery_count[begin_operation_count] = recovery_count;

    }

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

    memset(&test_ctx, 0, sizeof(test_ctx));
    memset(captured_packets, 0, sizeof(captured_packets));
    memset(captured_callbacks, 0, sizeof(captured_callbacks));
    begin_operation_count = 0;
    begin_operation_return = true;
    memset(captured_is_write, 0, sizeof(captured_is_write));
    memset(captured_recovery_count, 0, sizeof(captured_recovery_count));
    common_idle_value = true;

    complete_result = DCC_SERVICE_MODE_ERROR;
    complete_count = 0;

}

static interface_dcc_service_mode_paged_t make_interface(void) {

    interface_dcc_service_mode_paged_t interface;
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

TEST(DccServiceModePaged, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

}

// ============================================================================
// Write tests
// ============================================================================

TEST(DccServiceModePaged, write_cv1_page_select_packet) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    /* CV 1: page = ((1-1)/4)+1 = 1, register = ((1-1)%4)+1 = 1 */
    EXPECT_TRUE(DccServiceModePaged_write(&test_ctx, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

    /* Page select: write register 6 with page 1 */
    /* 0111 1 RRR where RRR = 6-1 = 5 = 0b101 */
    /* prefix = DCC_SERVICE_REGISTER_WRITE_PREFIX | 5 = 0x78 | 0x05 = 0x7D */
    EXPECT_EQ(captured_packets[0].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 5));
    EXPECT_EQ(captured_packets[0].data[1], (uint8_t)1);  /* page = 1 */
    EXPECT_EQ(captured_packets[0].byte_count, (uint8_t)3);
    verify_xor(&captured_packets[0]);
    EXPECT_EQ(captured_packets[0].preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

TEST(DccServiceModePaged, write_cv1_data_access_after_page_select_success) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 1, 0x55);

    /* Simulate page select completing successfully */
    ASSERT_NE(captured_callbacks[0], (dcc_service_mode_step_callback_t)NULL);
    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Data access step should now be in progress */
    EXPECT_EQ(begin_operation_count, (uint32_t)2);

    /* Write register 1 with value 0x55 */
    /* prefix = DCC_SERVICE_REGISTER_WRITE_PREFIX | 0 = 0x78 */
    EXPECT_EQ(captured_packets[1].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0));
    EXPECT_EQ(captured_packets[1].data[1], (uint8_t)0x55);
    EXPECT_EQ(captured_packets[1].byte_count, (uint8_t)3);
    verify_xor(&captured_packets[1]);

}

TEST(DccServiceModePaged, write_cv1_complete_callback_fires) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 1, 0x55);

    /* Page select success */
    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Data access success */
    ASSERT_NE(captured_callbacks[1], (dcc_service_mode_step_callback_t)NULL);
    captured_callbacks[1](DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_SUCCESS);

}

TEST(DccServiceModePaged, write_page_select_failure_aborts) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 1, 0x55);

    /* Page select fails */
    captured_callbacks[0](DCC_SERVICE_MODE_NO_ACK);

    /* Should NOT start data access step */
    EXPECT_EQ(begin_operation_count, (uint32_t)1);
    EXPECT_EQ(complete_count, (uint32_t)1);
    EXPECT_EQ(complete_result, DCC_SERVICE_MODE_NO_ACK);

}

TEST(DccServiceModePaged, write_cv5_correct_page_and_register) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    /* CV 5: page = ((5-1)/4)+1 = 2, register = ((5-1)%4)+1 = 1 */
    EXPECT_TRUE(DccServiceModePaged_write(&test_ctx, 5, 0xAA));

    /* Page select: write register 6 with page 2 */
    EXPECT_EQ(captured_packets[0].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 5));
    EXPECT_EQ(captured_packets[0].data[1], (uint8_t)2);

    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Data access: write register 1 with value 0xAA */
    EXPECT_EQ(captured_packets[1].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 0));
    EXPECT_EQ(captured_packets[1].data[1], (uint8_t)0xAA);

}

TEST(DccServiceModePaged, write_cv8_correct_page_and_register) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    /* CV 8: page = ((8-1)/4)+1 = 2, register = ((8-1)%4)+1 = 4 */
    EXPECT_TRUE(DccServiceModePaged_write(&test_ctx, 8, 0x08));

    EXPECT_EQ(captured_packets[0].data[1], (uint8_t)2);  /* page 2 */

    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Write register 4: RRR = 4-1 = 3 */
    EXPECT_EQ(captured_packets[1].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 3));
    EXPECT_EQ(captured_packets[1].data[1], (uint8_t)0x08);

}

// ============================================================================
// Verify tests
// ============================================================================

TEST(DccServiceModePaged, verify_cv1_uses_verify_prefix_for_data) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    EXPECT_TRUE(DccServiceModePaged_verify(&test_ctx, 1, 0x55));

    /* Page select is always a write */
    EXPECT_EQ(captured_packets[0].data[0], (uint8_t)(DCC_SERVICE_REGISTER_WRITE_PREFIX | 5));

    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Data access should be a verify */
    EXPECT_EQ(captured_packets[1].data[0], (uint8_t)(DCC_SERVICE_REGISTER_VERIFY_PREFIX | 0));
    EXPECT_EQ(captured_packets[1].data[1], (uint8_t)0x55);
    verify_xor(&captured_packets[1]);

}

// ============================================================================
// Guard tests
// ============================================================================

TEST(DccServiceModePaged, write_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    EXPECT_FALSE(DccServiceModePaged_write(&test_ctx, 0, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModePaged, write_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    EXPECT_FALSE(DccServiceModePaged_write(&test_ctx, 1025, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModePaged, write_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModePaged_write(&test_ctx, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModePaged, verify_cv0_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    EXPECT_FALSE(DccServiceModePaged_verify(&test_ctx, 0, 0x55));

}

TEST(DccServiceModePaged, verify_cv1025_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    EXPECT_FALSE(DccServiceModePaged_verify(&test_ctx, 1025, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModePaged, verify_busy_rejected) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    common_idle_value = false;
    EXPECT_FALSE(DccServiceModePaged_verify(&test_ctx, 1, 0x55));
    EXPECT_EQ(begin_operation_count, (uint32_t)0);

}

TEST(DccServiceModePaged, write_rejected_when_not_idle) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    /* Start a write — state moves to PAGE_SELECT */
    EXPECT_TRUE(DccServiceModePaged_write(&test_ctx, 1, 0x55));

    /* Second write while first is in-flight should be rejected */
    EXPECT_FALSE(DccServiceModePaged_write(&test_ctx, 2, 0xAA));
    EXPECT_EQ(begin_operation_count, (uint32_t)1);

}

TEST(DccServiceModePaged, verify_rejected_when_not_idle) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    /* Start a write — state moves to PAGE_SELECT */
    EXPECT_TRUE(DccServiceModePaged_write(&test_ctx, 1, 0x55));

    /* Verify while write is in-flight should be rejected */
    EXPECT_FALSE(DccServiceModePaged_verify(&test_ctx, 2, 0xAA));

}

TEST(DccServiceModePaged, page_select_failure_null_on_complete) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    interface.on_complete = NULL;
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 1, 0x55);

    /* Page select fails with on_complete = NULL — should not crash */
    captured_callbacks[0](DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(begin_operation_count, (uint32_t)1);
    EXPECT_EQ(complete_count, (uint32_t)0);

}

TEST(DccServiceModePaged, data_access_complete_null_on_complete) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    interface.on_complete = NULL;
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 1, 0x55);

    /* Page select succeeds */
    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Data access completes with on_complete = NULL — should not crash */
    captured_callbacks[1](DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(begin_operation_count, (uint32_t)2);
    EXPECT_EQ(complete_count, (uint32_t)0);

}

// ============================================================================
// Recovery flags: paged write — both steps are writes
// ============================================================================

TEST(DccServiceModePaged, write_page_select_passes_write_flag) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 5, 0x55);

    /* Step 0: page select — always a write */
    EXPECT_TRUE(captured_is_write[0]);
    EXPECT_EQ(captured_recovery_count[0], (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

TEST(DccServiceModePaged, write_data_access_passes_write_flag) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_write(&test_ctx, 5, 0x55);

    /* Complete page select → triggers data access */
    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Step 1: data access — write operation */
    EXPECT_TRUE(captured_is_write[1]);
    EXPECT_EQ(captured_recovery_count[1], (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

// ============================================================================
// Recovery flags: paged verify — page select is write, data access is verify
// ============================================================================

TEST(DccServiceModePaged, verify_page_select_passes_write_flag) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_verify(&test_ctx, 5, 0x55);

    /* Step 0: page select — always a write */
    EXPECT_TRUE(captured_is_write[0]);
    EXPECT_EQ(captured_recovery_count[0], (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}

TEST(DccServiceModePaged, verify_data_access_passes_verify_flag) {

    reset_mocks();
    interface_dcc_service_mode_paged_t interface = make_interface();
    DccServiceModePaged_initialize(&test_ctx, &interface);

    DccServiceModePaged_verify(&test_ctx, 5, 0x55);

    /* Complete page select → triggers data access */
    captured_callbacks[0](DCC_SERVICE_MODE_SUCCESS);

    /* Step 1: data access — verify operation */
    EXPECT_FALSE(captured_is_write[1]);
    EXPECT_EQ(captured_recovery_count[1], (uint8_t)DCC_SERVICE_MODE_RECOVERY_COUNT);

}
