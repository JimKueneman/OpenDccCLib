/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Task Paged orchestrator.
 *
 * Paged mode has no native bit operations — read_cv scans paged_verify(cv, 0..255)
 * until ACK; read_bit does the same scan then extracts the target bit; write_bit
 * does scan + read-modify-write + verify. write_cv is write + targeted verify.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_task_paged.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

// ============================================================================
// Mock / tracking state
// ============================================================================

static interface_dcc_service_mode_task_paged_t _test_interface;

static uint16_t last_paged_verify_cv;
static uint8_t  last_paged_verify_value;
static uint32_t paged_verify_count;
static bool     paged_verify_return;

static uint16_t last_paged_write_cv;
static uint8_t  last_paged_write_value;
static uint32_t paged_write_count;
static bool     paged_write_return;

static bool     is_idle_return;
static uint32_t on_start_ack_scan_count;

static dcc_service_mode_result_t on_complete_result;
static uint8_t                   on_complete_value;
static uint32_t                  on_complete_count;

static dcc_task_phase_enum on_progress_phase;
static uint8_t             on_progress_current_step;
static uint8_t             on_progress_estimated_steps;
static uint32_t            on_progress_count;

static bool mock_paged_verify(uint16_t cv_number, uint8_t value) {

    last_paged_verify_cv = cv_number;
    last_paged_verify_value = value;
    paged_verify_count++;

    return paged_verify_return;

}

static bool mock_paged_write(uint16_t cv_number, uint8_t value) {

    last_paged_write_cv = cv_number;
    last_paged_write_value = value;
    paged_write_count++;

    return paged_write_return;

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

    last_paged_verify_cv = 0;
    last_paged_verify_value = 0;
    paged_verify_count = 0;
    paged_verify_return = true;

    last_paged_write_cv = 0;
    last_paged_write_value = 0;
    paged_write_count = 0;
    paged_write_return = true;

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

static interface_dcc_service_mode_task_paged_t make_interface(void) {

    interface_dcc_service_mode_task_paged_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.paged_verify      = mock_paged_verify;
    interface.paged_write       = mock_paged_write;
    interface.is_idle           = mock_is_idle;
    interface.on_start_ack_scan = mock_on_start_ack_scan;

    return interface;

}

static void setup(void) {

    reset_mocks();
    _test_interface = make_interface();
    DccServiceModeTaskPaged_initialize(&_test_interface);

}

// Drive one scan step with no ACK.
static void step_no_ack(void) {

    DccServiceModeTaskPaged_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

}

// Drive one scan step with ACK (the found value).
static void step_ack(void) {

    DccServiceModeTaskPaged_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

// Drive scan: no-ACK for values 0..target-1, then ACK on target.
static void drive_scan_to(uint8_t target) {

    for (uint16_t v = 0; v < (uint16_t)target; v++) {

        step_no_ack();

    }

    step_ack();

}

// Drive a complete read_cv, ACK fires when scan reaches target_value.
static void drive_read_cv(uint16_t cv, uint8_t target_value) {

    DccServiceModeTaskPaged_read_cv(cv, mock_on_complete, mock_on_progress);
    drive_scan_to(target_value);

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccServiceModeTaskPaged, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_paged_t interface = make_interface();
    DccServiceModeTaskPaged_initialize(&interface);

}

// ============================================================================
// read_cv — input validation
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_read_cv(0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, read_cv_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_read_cv(1025, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, read_cv_cv1024_accepted) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskPaged_read_cv(1024, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)1);

}

TEST(DccServiceModeTaskPaged, read_cv_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress));

}

// ============================================================================
// read_cv — first call issues verify with value 0
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_starts_with_paged_verify_value_0) {

    setup();
    DccServiceModeTaskPaged_read_cv(29, mock_on_complete, mock_on_progress);

    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(last_paged_verify_cv, (uint16_t)29);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0);

}

// ============================================================================
// read_cv — scan advances on each no-ack step
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_advances_scan_value_after_no_ack) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(paged_verify_count, (uint32_t)2);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)1);

}

// ============================================================================
// read_cv — completion on ACK
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_ack_on_value_0_returns_0) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

TEST(DccServiceModeTaskPaged, read_cv_ack_on_value_42_returns_42) {

    setup();
    drive_read_cv(1, 42);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)42);

}

TEST(DccServiceModeTaskPaged, read_cv_ack_on_value_255_returns_255) {

    setup();
    drive_read_cv(1, 255);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)255);

}

TEST(DccServiceModeTaskPaged, read_cv_ack_on_value_42_issues_43_verifies) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);

    // 42 no-ack steps (values 0-41) + 1 ack step (value 42) = 43 verify calls
    drive_scan_to(42);

    EXPECT_EQ(paged_verify_count, (uint32_t)43);

}

// ============================================================================
// read_cv — exhaustion
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_all_256_no_ack_returns_error) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

TEST(DccServiceModeTaskPaged, read_cv_256_verifies_before_error) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(paged_verify_count, (uint32_t)256);

}

// ============================================================================
// read_cv — progress callback
// ============================================================================

TEST(DccServiceModeTaskPaged, read_cv_progress_called_each_step) {

    setup();
    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress);

    step_no_ack();
    step_no_ack();
    step_ack();

    EXPECT_EQ(on_progress_count, (uint32_t)3);

}

TEST(DccServiceModeTaskPaged, read_cv_progress_uses_read_phase) {

    setup();
    drive_read_cv(1, 0);

    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_READ);

}

TEST(DccServiceModeTaskPaged, read_cv_returns_to_idle_after_complete) {

    setup();
    drive_read_cv(1, 0);

    EXPECT_TRUE(DccServiceModeTaskPaged_read_cv(1, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_cv — input validation
// ============================================================================

TEST(DccServiceModeTaskPaged, write_cv_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_write_cv(0, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_write_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, write_cv_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_write_cv(1025, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_write_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, write_cv_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_cv — step 1: write
// ============================================================================

TEST(DccServiceModeTaskPaged, write_cv_calls_paged_write_with_correct_args) {

    setup();
    DccServiceModeTaskPaged_write_cv(29, 0xA5, mock_on_complete, mock_on_progress);

    EXPECT_EQ(paged_write_count, (uint32_t)1);
    EXPECT_EQ(last_paged_write_cv, (uint16_t)29);
    EXPECT_EQ(last_paged_write_value, (uint8_t)0xA5);

}

TEST(DccServiceModeTaskPaged, write_cv_does_not_verify_before_primitive_complete) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    /* Write issued; verify must not fire until the primitive completes. */

    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

// ============================================================================
// write_cv — step 2: verify
// ============================================================================

TEST(DccServiceModeTaskPaged, write_cv_calls_paged_verify_after_primitive_complete) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(last_paged_verify_cv, (uint16_t)1);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskPaged, write_cv_write_ack_optional_still_verifies) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    DccServiceModeTaskPaged_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

    EXPECT_EQ(paged_verify_count, (uint32_t)1);

}

// ============================================================================
// write_cv — completion
// ============================================================================

TEST(DccServiceModeTaskPaged, write_cv_verify_ack_calls_complete_success) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskPaged, write_cv_verify_no_ack_calls_complete_verify_fail) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_no_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);
    EXPECT_EQ(on_complete_value, (uint8_t)0x55);

}

// ============================================================================
// write_cv — progress phases
// ============================================================================

TEST(DccServiceModeTaskPaged, write_cv_first_progress_is_write_phase) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(on_progress_count, (uint32_t)1);
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_WRITE);

}

TEST(DccServiceModeTaskPaged, write_cv_second_progress_is_verify_phase) {

    setup();
    DccServiceModeTaskPaged_write_cv(1, 0x55, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_ack();

    EXPECT_EQ(on_progress_count, (uint32_t)2);
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_VERIFY);

}

// ============================================================================
// read_bit — input validation
// ============================================================================

TEST(DccServiceModeTaskPaged, read_bit_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_read_bit(0, 0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_read_bit(1025, 0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_read_bit(1, 8, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskPaged_read_bit(1, 0, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskPaged_read_bit(1, 0, mock_on_complete, mock_on_progress));

}

// ============================================================================
// read_bit — scans same as read_cv, then extracts bit
// ============================================================================

TEST(DccServiceModeTaskPaged, read_bit_starts_with_paged_verify_value_0) {

    setup();
    DccServiceModeTaskPaged_read_bit(1, 3, mock_on_complete, mock_on_progress);

    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_extracts_bit0_from_found_byte) {

    setup();
    // CV holds 0xA5 = 0b10100101, bit 0 = 1
    DccServiceModeTaskPaged_read_bit(1, 0, mock_on_complete, mock_on_progress);
    drive_scan_to(0xA5);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskPaged, read_bit_extracts_bit1_from_found_byte) {

    setup();
    // CV holds 0xA5 = 0b10100101, bit 1 = 0
    DccServiceModeTaskPaged_read_bit(1, 1, mock_on_complete, mock_on_progress);
    drive_scan_to(0xA5);

    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_extracts_bit7_from_found_byte) {

    setup();
    // CV holds 0xA5 = 0b10100101, bit 7 = 1
    DccServiceModeTaskPaged_read_bit(1, 7, mock_on_complete, mock_on_progress);
    drive_scan_to(0xA5);

    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskPaged, read_bit_byte_0_all_bits_are_0) {

    setup();
    DccServiceModeTaskPaged_read_bit(1, 5, mock_on_complete, mock_on_progress);
    step_ack(); // ACK on value 0

    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

TEST(DccServiceModeTaskPaged, read_bit_scan_exhausted_returns_error) {

    setup();
    DccServiceModeTaskPaged_read_bit(1, 3, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

// ============================================================================
// write_bit — input validation
// ============================================================================

TEST(DccServiceModeTaskPaged, write_bit_cv0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_write_bit(0, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, write_bit_cv1025_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_write_bit(1025, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, write_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskPaged_write_bit(1, 8, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(paged_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskPaged, write_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskPaged_write_bit(1, 0, true, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskPaged_write_bit(1, 0, true, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_bit — scan phase
// ============================================================================

TEST(DccServiceModeTaskPaged, write_bit_starts_with_paged_verify_value_0) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, true, mock_on_complete, mock_on_progress);

    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0);

}

TEST(DccServiceModeTaskPaged, write_bit_scan_exhausted_returns_error) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, true, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

// ============================================================================
// write_bit — scan → write phase
// ============================================================================

TEST(DccServiceModeTaskPaged, write_bit_calls_paged_write_after_scan_with_bit_set) {

    setup();
    // Current byte found = 0x00, bit 3 set to 1 → new_value = 0x08
    DccServiceModeTaskPaged_write_bit(29, 3, true, mock_on_complete, mock_on_progress);
    step_ack(); // ACK on value 0x00

    EXPECT_EQ(paged_write_count, (uint32_t)1);
    EXPECT_EQ(last_paged_write_cv, (uint16_t)29);
    EXPECT_EQ(last_paged_write_value, (uint8_t)0x08);

}

TEST(DccServiceModeTaskPaged, write_bit_calls_paged_write_with_bit_cleared) {

    setup();
    // Current byte found = 0xFF, bit 3 cleared → new_value = 0xF7
    DccServiceModeTaskPaged_write_bit(1, 3, false, mock_on_complete, mock_on_progress);
    drive_scan_to(0xFF);

    EXPECT_EQ(paged_write_count, (uint32_t)1);
    EXPECT_EQ(last_paged_write_value, (uint8_t)0xF7);

}

TEST(DccServiceModeTaskPaged, write_bit_does_not_complete_after_scan_ack) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, true, mock_on_complete, mock_on_progress);
    step_ack(); // scan found byte

    EXPECT_EQ(on_complete_count, (uint32_t)0);

}

// ============================================================================
// write_bit — write → verify phase
// ============================================================================

TEST(DccServiceModeTaskPaged, write_bit_calls_paged_verify_after_write) {

    setup();
    // byte 0x00, set bit 3 → new = 0x08
    DccServiceModeTaskPaged_write_bit(29, 3, true, mock_on_complete, mock_on_progress);
    step_ack();  // scan done, paged_write called
    step_no_ack(); // write complete, paged_verify called

    EXPECT_EQ(paged_verify_count, (uint32_t)2); // 1 from scan (value 0) + 1 from verify
    EXPECT_EQ(last_paged_verify_cv, (uint16_t)29);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0x08);

}

// ============================================================================
// write_bit — completion
// ============================================================================

TEST(DccServiceModeTaskPaged, write_bit_verify_ack_calls_complete_success) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, true, mock_on_complete, mock_on_progress);
    step_ack();    // scan done
    step_no_ack(); // write complete
    step_ack();    // verify ACK

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskPaged, write_bit_verify_no_ack_calls_complete_verify_fail) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, true, mock_on_complete, mock_on_progress);
    step_ack();    // scan done
    step_no_ack(); // write complete
    step_no_ack(); // verify no ACK

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

TEST(DccServiceModeTaskPaged, write_bit_false_result_value_is_0) {

    setup();
    DccServiceModeTaskPaged_write_bit(1, 3, false, mock_on_complete, mock_on_progress);
    drive_scan_to(0xFF);
    step_no_ack(); // write complete
    step_ack();    // verify ACK

    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccServiceModeTaskPaged, null_on_complete_read_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_paged_t interface = make_interface();
    DccServiceModeTaskPaged_initialize(&interface);

    DccServiceModeTaskPaged_read_cv(1, NULL, mock_on_progress);
    step_ack();

}

TEST(DccServiceModeTaskPaged, null_on_progress_read_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_paged_t interface = make_interface();
    DccServiceModeTaskPaged_initialize(&interface);

    DccServiceModeTaskPaged_read_cv(1, mock_on_complete, NULL);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);

}

// ============================================================================
// on_primitive_complete when IDLE — no crash
// ============================================================================

TEST(DccServiceModeTaskPaged, on_primitive_complete_when_idle_no_crash) {

    setup();
    DccServiceModeTaskPaged_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */
