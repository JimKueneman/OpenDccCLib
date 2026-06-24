/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Task Direct orchestrator.
 *
 * Tests the state machine that sequences direct mode primitives to implement
 * read_cv (8x verify_bit), write_cv (write + verify), read_bit (verify_bit),
 * and write_bit (write_bit + verify_bit). Drives the state machine via
 * on_ack() and on_primitive_complete() as the application/primitive layer would.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_task_direct.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

// ============================================================================
// Mock / tracking state
// ============================================================================

static interface_dcc_service_mode_task_direct_t _test_interface;

static uint16_t last_verify_bit_cv;
static uint8_t  last_verify_bit_position;
static bool     last_verify_bit_value;
static uint32_t verify_bit_count;
static bool     verify_bit_return;

static uint16_t last_verify_byte_cv;
static uint8_t  last_verify_byte_value;
static uint32_t verify_byte_count;
static bool     verify_byte_return;

static uint16_t last_write_byte_cv;
static uint8_t  last_write_byte_value;
static uint32_t write_byte_count;
static bool     write_byte_return;

static uint16_t last_write_bit_cv;
static uint8_t  last_write_bit_position;
static bool     last_write_bit_value;
static uint32_t write_bit_count;
static bool     write_bit_return;

static bool     is_idle_return;
static uint32_t on_start_ack_scan_count;

static dcc_service_mode_result_t on_complete_result;
static uint8_t                   on_complete_value;
static uint32_t                  on_complete_count;

static dcc_task_phase_enum on_progress_phase;
static uint8_t             on_progress_current_step;
static uint8_t             on_progress_estimated_steps;
static uint32_t            on_progress_count;

static bool mock_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    last_verify_bit_cv = cv_number;
    last_verify_bit_position = bit_position;
    last_verify_bit_value = bit_value;
    verify_bit_count++;

    return verify_bit_return;

}

static bool mock_verify_byte(uint16_t cv_number, uint8_t value) {

    last_verify_byte_cv = cv_number;
    last_verify_byte_value = value;
    verify_byte_count++;

    return verify_byte_return;

}

static bool mock_write_byte(uint16_t cv_number, uint8_t value) {

    last_write_byte_cv = cv_number;
    last_write_byte_value = value;
    write_byte_count++;

    return write_byte_return;

}

static bool mock_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    last_write_bit_cv = cv_number;
    last_write_bit_position = bit_position;
    last_write_bit_value = bit_value;
    write_bit_count++;

    return write_bit_return;

}

static bool mock_is_idle(void) {

    return is_idle_return;

}

static void mock_on_start_ack_scan(void) {

    on_start_ack_scan_count++;

}

static void mock_on_complete(dcc_service_mode_result_t result, uint8_t value) {

    on_complete_result = result;
    on_complete_value = value;
    on_complete_count++;

}

static void mock_on_progress(dcc_task_phase_enum phase, uint8_t current_step, uint8_t estimated_steps) {

    on_progress_phase = phase;
    on_progress_current_step = current_step;
    on_progress_estimated_steps = estimated_steps;
    on_progress_count++;

}

static void reset_mocks(void) {

    last_verify_bit_cv = 0;
    last_verify_bit_position = 0;
    last_verify_bit_value = false;
    verify_bit_count = 0;
    verify_bit_return = true;

    last_verify_byte_cv = 0;
    last_verify_byte_value = 0;
    verify_byte_count = 0;
    verify_byte_return = true;

    last_write_byte_cv = 0;
    last_write_byte_value = 0;
    write_byte_count = 0;
    write_byte_return = true;

    last_write_bit_cv = 0;
    last_write_bit_position = 0;
    last_write_bit_value = false;
    write_bit_count = 0;
    write_bit_return = true;

    is_idle_return = true;
    on_start_ack_scan_count = 0;

    on_complete_result = DCC_SERVICE_MODE_ERROR;
    on_complete_value = 0;
    on_complete_count = 0;

    on_progress_phase = DCC_TASK_PHASE_READ;
    on_progress_current_step = 0;
    on_progress_estimated_steps = 0;
    on_progress_count = 0;

}

static interface_dcc_service_mode_task_direct_t make_interface(void) {

    interface_dcc_service_mode_task_direct_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.verify_bit       = mock_verify_bit;
    interface.verify_byte      = mock_verify_byte;
    interface.write_byte       = mock_write_byte;
    interface.write_bit        = mock_write_bit;
    interface.is_idle          = mock_is_idle;
    interface.on_start_ack_scan = mock_on_start_ack_scan;

    return interface;

}

static void setup(void) {

    reset_mocks();
    _test_interface = make_interface();
    DccServiceModeTaskDirect_initialize(&_test_interface);

}

// Helper: drive a full read_cv through all 8 bits.
// ack_mask bit N set = ACK on bit N (bit is 1).
static uint8_t drive_read_cv(uint16_t cv, uint8_t ack_mask) {

    DccServiceModeTaskDirect_read_cv(cv, mock_on_complete, mock_on_progress);

    for (uint8_t bit_index = 0; bit_index < 8; bit_index++) {

        bool ack = (ack_mask & (1 << bit_index)) != 0;
        DccServiceModeTaskDirect_on_primitive_complete(ack ? DCC_SERVICE_MODE_SUCCESS : DCC_SERVICE_MODE_NO_ACK);

    }

    return on_complete_value;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccServiceModeTaskDirect, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_direct_t interface = make_interface();
    DccServiceModeTaskDirect_initialize(&interface);

}

// ============================================================================
// read_cv — input validation
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_read_cv(0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, read_cv_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_read_cv(1025, mock_on_complete, mock_on_progress));
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, read_cv_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDirect_read_cv(1, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskDirect_read_cv(1, mock_on_complete, mock_on_progress));

}

// ============================================================================
// read_cv — first primitive call
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_calls_verify_bit_0_first) {

    setup();
    DccServiceModeTaskDirect_read_cv(29, mock_on_complete, mock_on_progress);

    EXPECT_EQ(verify_bit_count, (uint32_t)1);
    EXPECT_EQ(last_verify_bit_cv, (uint16_t)29);
    EXPECT_EQ(last_verify_bit_position, (uint8_t)0);
    EXPECT_TRUE(last_verify_bit_value);

}

// ============================================================================
// read_cv — state machine advances
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_advances_to_next_bit_after_primitive_complete) {

    setup();
    DccServiceModeTaskDirect_read_cv(1, mock_on_complete, mock_on_progress);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(verify_bit_count, (uint32_t)2);
    EXPECT_EQ(last_verify_bit_position, (uint8_t)1);

}

TEST(DccServiceModeTaskDirect, read_cv_issues_8_verify_bit_calls) {

    setup();
    drive_read_cv(1, 0x00);

    EXPECT_EQ(verify_bit_count, (uint32_t)8);

}

// ============================================================================
// read_cv — value assembly
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_all_acks_returns_0xFF) {

    setup();
    uint8_t result = drive_read_cv(1, 0xFF);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(result, (uint8_t)0xFF);

}

TEST(DccServiceModeTaskDirect, read_cv_no_acks_returns_0x00) {

    setup();
    uint8_t result = drive_read_cv(1, 0x00);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(result, (uint8_t)0x00);

}

TEST(DccServiceModeTaskDirect, read_cv_alternating_acks_returns_0x55) {

    setup();
    // bits 0,2,4,6 ACK → value = 0b01010101 = 0x55
    uint8_t result = drive_read_cv(1, 0x55);

    EXPECT_EQ(result, (uint8_t)0x55);

}

TEST(DccServiceModeTaskDirect, read_cv_alternating_acks_returns_0xAA) {

    setup();
    // bits 1,3,5,7 ACK → value = 0b10101010 = 0xAA
    uint8_t result = drive_read_cv(1, 0xAA);

    EXPECT_EQ(result, (uint8_t)0xAA);

}

TEST(DccServiceModeTaskDirect, read_cv_cv29_value_assembled_correctly) {

    setup();
    // CV29 typical value: 0b00000110 = 0x06 (bits 1 and 2 set)
    uint8_t result = drive_read_cv(29, 0x06);

    EXPECT_EQ(result, (uint8_t)0x06);

}

// ============================================================================
// read_cv — progress callback
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_progress_called_8_times) {

    setup();
    drive_read_cv(1, 0x00);

    EXPECT_EQ(on_progress_count, (uint32_t)8);

}

TEST(DccServiceModeTaskDirect, read_cv_progress_uses_read_phase) {

    setup();
    drive_read_cv(1, 0x00);

    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_READ);

}

TEST(DccServiceModeTaskDirect, read_cv_progress_estimated_steps_is_8) {

    setup();
    drive_read_cv(1, 0x00);

    EXPECT_EQ(on_progress_estimated_steps, (uint8_t)8);

}

TEST(DccServiceModeTaskDirect, read_cv_null_progress_no_crash) {

    setup();
    drive_read_cv(1, 0xFF);

    // no crash is the test — drive_read_cv uses mock_on_progress, do null variant
    reset_mocks();
    interface_dcc_service_mode_task_direct_t interface = make_interface();
    DccServiceModeTaskDirect_initialize(&interface);

    DccServiceModeTaskDirect_read_cv(1, mock_on_complete, NULL);

    for (uint8_t bit_index = 0; bit_index < 8; bit_index++) {

        DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    }

    EXPECT_EQ(on_complete_count, (uint32_t)1);

}

// ============================================================================
// read_cv — returns to IDLE after completion
// ============================================================================

TEST(DccServiceModeTaskDirect, read_cv_returns_to_idle_and_accepts_second_read) {

    setup();
    drive_read_cv(1, 0x00);

    EXPECT_EQ(on_complete_count, (uint32_t)1);

    // Should be IDLE again — accept a second read
    EXPECT_TRUE(DccServiceModeTaskDirect_read_cv(1, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_cv — input validation
// ============================================================================

TEST(DccServiceModeTaskDirect, write_cv_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_write_cv(0, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_byte_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, write_cv_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_write_cv(1025, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_byte_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, write_cv_cv1024_accepted) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDirect_write_cv(1024, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_byte_count, (uint32_t)1);

}

TEST(DccServiceModeTaskDirect, write_cv_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_cv — step 1: write
// ============================================================================

TEST(DccServiceModeTaskDirect, write_cv_calls_write_byte_with_correct_args) {

    setup();
    DccServiceModeTaskDirect_write_cv(29, 0xA5, mock_on_complete, mock_on_progress);

    EXPECT_EQ(write_byte_count, (uint32_t)1);
    EXPECT_EQ(last_write_byte_cv, (uint16_t)29);
    EXPECT_EQ(last_write_byte_value, (uint8_t)0xA5);

}

TEST(DccServiceModeTaskDirect, write_cv_does_not_verify_before_primitive_complete) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);

    // Write issued; verify must not fire until the primitive completes.
    EXPECT_EQ(verify_byte_count, (uint32_t)0);

}

// ============================================================================
// write_cv — step 2: verify
// ============================================================================

TEST(DccServiceModeTaskDirect, write_cv_calls_verify_after_primitive_complete) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(verify_byte_count, (uint32_t)1);
    EXPECT_EQ(last_verify_byte_cv, (uint16_t)1);
    EXPECT_EQ(last_verify_byte_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskDirect, write_cv_write_ack_optional_still_verifies) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    // write ACK is optional per spec — still must proceed to verify
    EXPECT_EQ(verify_byte_count, (uint32_t)1);

}

// ============================================================================
// write_cv — completion
// ============================================================================

TEST(DccServiceModeTaskDirect, write_cv_verify_ack_calls_complete_success) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskDirect, write_cv_verify_no_ack_calls_complete_verify_fail) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);
    EXPECT_EQ(on_complete_value, (uint8_t)0x55);

}

// ============================================================================
// write_cv — progress phases
// ============================================================================

TEST(DccServiceModeTaskDirect, write_cv_progress_write_phase_then_verify_phase) {

    setup();
    DccServiceModeTaskDirect_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(on_progress_count, (uint32_t)1);
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_WRITE);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(on_progress_count, (uint32_t)2);
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_VERIFY);

}

// ============================================================================
// read_bit — input validation
// ============================================================================

TEST(DccServiceModeTaskDirect, read_bit_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_read_bit(0, 0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, read_bit_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_read_bit(1025, 0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, read_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_read_bit(1, 8, mock_on_complete, mock_on_progress));
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, read_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDirect_read_bit(1, 0, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskDirect_read_bit(1, 0, mock_on_complete, mock_on_progress));

}

// ============================================================================
// read_bit — primitive call
// ============================================================================

TEST(DccServiceModeTaskDirect, read_bit_calls_verify_bit_with_value_1) {

    setup();
    DccServiceModeTaskDirect_read_bit(29, 5, mock_on_complete, mock_on_progress);

    EXPECT_EQ(verify_bit_count, (uint32_t)1);
    EXPECT_EQ(last_verify_bit_cv, (uint16_t)29);
    EXPECT_EQ(last_verify_bit_position, (uint8_t)5);
    EXPECT_TRUE(last_verify_bit_value);

}

// ============================================================================
// read_bit — result
// ============================================================================

TEST(DccServiceModeTaskDirect, read_bit_ack_returns_value_1) {

    setup();
    DccServiceModeTaskDirect_read_bit(1, 3, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskDirect, read_bit_no_ack_returns_value_0) {

    setup();
    DccServiceModeTaskDirect_read_bit(1, 3, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

// ============================================================================
// write_bit — input validation
// ============================================================================

TEST(DccServiceModeTaskDirect, write_bit_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_write_bit(0, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, write_bit_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_write_bit(1025, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, write_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskDirect_write_bit(1, 8, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(write_bit_count, (uint32_t)0);

}

TEST(DccServiceModeTaskDirect, write_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDirect_write_bit(1, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskDirect_write_bit(1, 0, true, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_bit — step 1: write
// ============================================================================

TEST(DccServiceModeTaskDirect, write_bit_calls_write_bit_with_correct_args) {

    setup();
    DccServiceModeTaskDirect_write_bit(29, 5, true, mock_on_complete, mock_on_progress);

    EXPECT_EQ(write_bit_count, (uint32_t)1);
    EXPECT_EQ(last_write_bit_cv, (uint16_t)29);
    EXPECT_EQ(last_write_bit_position, (uint8_t)5);
    EXPECT_TRUE(last_write_bit_value);

}

TEST(DccServiceModeTaskDirect, write_bit_low_value_correct_args) {

    setup();
    DccServiceModeTaskDirect_write_bit(1, 3, false, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_write_bit_position, (uint8_t)3);
    EXPECT_FALSE(last_write_bit_value);

}

TEST(DccServiceModeTaskDirect, write_bit_does_not_verify_before_primitive_complete) {

    setup();
    DccServiceModeTaskDirect_write_bit(1, 0, true, mock_on_complete, mock_on_progress);

    // Write issued; verify must not fire until the primitive completes.
    EXPECT_EQ(verify_bit_count, (uint32_t)0);

}

// ============================================================================
// write_bit — step 2: verify
// ============================================================================

TEST(DccServiceModeTaskDirect, write_bit_calls_verify_bit_after_primitive_complete) {

    setup();
    DccServiceModeTaskDirect_write_bit(29, 5, true, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(verify_bit_count, (uint32_t)1);
    EXPECT_EQ(last_verify_bit_cv, (uint16_t)29);
    EXPECT_EQ(last_verify_bit_position, (uint8_t)5);
    EXPECT_TRUE(last_verify_bit_value);

}

TEST(DccServiceModeTaskDirect, write_bit_verify_uses_written_bit_value) {

    setup();
    DccServiceModeTaskDirect_write_bit(1, 3, false, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_FALSE(last_verify_bit_value);

}

// ============================================================================
// write_bit — completion
// ============================================================================

TEST(DccServiceModeTaskDirect, write_bit_verify_ack_calls_complete_success) {

    setup();
    DccServiceModeTaskDirect_write_bit(1, 3, true, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskDirect, write_bit_verify_no_ack_calls_complete_verify_fail) {

    setup();
    DccServiceModeTaskDirect_write_bit(1, 3, true, mock_on_complete, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccServiceModeTaskDirect, null_on_complete_read_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_direct_t interface = make_interface();
    DccServiceModeTaskDirect_initialize(&interface);

    DccServiceModeTaskDirect_read_cv(1, NULL, mock_on_progress);

    for (uint8_t bit_index = 0; bit_index < 8; bit_index++) {

        DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    }

}

TEST(DccServiceModeTaskDirect, null_on_complete_write_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_direct_t interface = make_interface();
    DccServiceModeTaskDirect_initialize(&interface);

    DccServiceModeTaskDirect_write_cv(1, 0x55, NULL, mock_on_progress);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);
    DccServiceModeTaskDirect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */
