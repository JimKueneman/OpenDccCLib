/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Task Register orchestrator.
 *
 * Register (Physical Register) mode maps a CV number to one of 8 fixed registers
 * based on decoder_type (Mobile vs Accessory), then scans/writes via the register
 * primitive. read_cv scans register_verify(reg, 0..255) until ACK; read_bit scans
 * then extracts; write_bit does read-modify-write + verify; write_cv is write +
 * verify; factory_reset writes value 8 to register 8.
 *
 * Mobile mapping:    CV1→1 CV2→2 CV3→3 CV4→4 CV29→5 CV7→7 CV8→8
 * Accessory mapping: CV513→1 CV7→7 CV520→8
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_task_register.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

// ============================================================================
// Mock / tracking state
// ============================================================================

static interface_dcc_service_mode_task_register_t _test_interface;

static uint8_t  last_register_verify_reg;
static uint8_t  last_register_verify_value;
static uint32_t register_verify_count;
static bool     register_verify_return;

static uint8_t  last_register_write_reg;
static uint8_t  last_register_write_value;
static uint32_t register_write_count;
static bool     register_write_return;

static bool     is_idle_return;
static uint32_t on_start_ack_scan_count;

static dcc_service_mode_result_t on_complete_result;
static uint8_t                   on_complete_value;
static uint32_t                  on_complete_count;

static dcc_task_phase_enum on_progress_phase;
static uint8_t             on_progress_current_step;
static uint8_t             on_progress_estimated_steps;
static uint32_t            on_progress_count;

static bool mock_register_verify(uint8_t register_number, uint8_t value) {

    last_register_verify_reg = register_number;
    last_register_verify_value = value;
    register_verify_count++;

    return register_verify_return;

}

static bool mock_register_write(uint8_t register_number, uint8_t value) {

    last_register_write_reg = register_number;
    last_register_write_value = value;
    register_write_count++;

    return register_write_return;

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

    last_register_verify_reg = 0;
    last_register_verify_value = 0;
    register_verify_count = 0;
    register_verify_return = true;

    last_register_write_reg = 0;
    last_register_write_value = 0;
    register_write_count = 0;
    register_write_return = true;

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

static interface_dcc_service_mode_task_register_t make_interface(void) {

    interface_dcc_service_mode_task_register_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.register_verify   = mock_register_verify;
    interface.register_write    = mock_register_write;
    interface.is_idle           = mock_is_idle;
    interface.on_start_ack_scan = mock_on_start_ack_scan;

    return interface;

}

static void setup(void) {

    reset_mocks();
    _test_interface = make_interface();
    DccServiceModeTaskRegister_initialize(&_test_interface);

}

static void step_no_ack(void) {

    DccServiceModeTaskRegister_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

}

static void step_ack(void) {

    DccServiceModeTaskRegister_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

static void drive_scan_to(uint8_t target) {

    for (uint16_t v = 0; v < (uint16_t)target; v++) {

        step_no_ack();

    }

    step_ack();

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccServiceModeTaskRegister, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_register_t interface = make_interface();
    DccServiceModeTaskRegister_initialize(&interface);

}

// ============================================================================
// CV-to-register mapping — Mobile
// ============================================================================

// @compliance DCC-S9.2.3-CS-019
TEST(DccServiceModeTaskRegister, mobile_cv1_maps_to_register_1) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_reg, (uint8_t)1);

}

TEST(DccServiceModeTaskRegister, mobile_cv2_maps_to_register_2) {

    setup();
    DccServiceModeTaskRegister_read_cv(2, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)2);

}

TEST(DccServiceModeTaskRegister, mobile_cv3_maps_to_register_3) {

    setup();
    DccServiceModeTaskRegister_read_cv(3, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)3);

}

TEST(DccServiceModeTaskRegister, mobile_cv4_maps_to_register_4) {

    setup();
    DccServiceModeTaskRegister_read_cv(4, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)4);

}

TEST(DccServiceModeTaskRegister, mobile_cv29_maps_to_register_5) {

    setup();
    DccServiceModeTaskRegister_read_cv(29, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)5);

}

TEST(DccServiceModeTaskRegister, mobile_cv7_maps_to_register_7) {

    setup();
    DccServiceModeTaskRegister_read_cv(7, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)7);

}

TEST(DccServiceModeTaskRegister, mobile_cv8_maps_to_register_8) {

    setup();
    DccServiceModeTaskRegister_read_cv(8, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)8);

}

TEST(DccServiceModeTaskRegister, mobile_cv5_unmapped_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(5, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, mobile_cv6_unmapped_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(6, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, mobile_cv100_unmapped_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(100, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

// ============================================================================
// CV-to-register mapping — Accessory
// ============================================================================

TEST(DccServiceModeTaskRegister, accessory_cv513_maps_to_register_1) {

    setup();
    DccServiceModeTaskRegister_read_cv(513, DCC_DECODER_TYPE_ACCESSORY, mock_on_complete, mock_on_progress);

    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_reg, (uint8_t)1);

}

TEST(DccServiceModeTaskRegister, accessory_cv7_maps_to_register_7) {

    setup();
    DccServiceModeTaskRegister_read_cv(7, DCC_DECODER_TYPE_ACCESSORY, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)7);

}

// @compliance DCC-S9.2.3-CS-019
TEST(DccServiceModeTaskRegister, accessory_cv520_maps_to_register_8) {

    setup();
    DccServiceModeTaskRegister_read_cv(520, DCC_DECODER_TYPE_ACCESSORY, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_reg, (uint8_t)8);

}

TEST(DccServiceModeTaskRegister, accessory_cv1_unmapped_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_ACCESSORY, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, accessory_cv29_unmapped_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(29, DCC_DECODER_TYPE_ACCESSORY, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

// ============================================================================
// read_cv — scan behavior (Mobile CV1 / register 1)
// ============================================================================

TEST(DccServiceModeTaskRegister, read_cv_starts_with_value_0) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(last_register_verify_value, (uint8_t)0);

}

TEST(DccServiceModeTaskRegister, read_cv_advances_scan_after_no_ack) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(register_verify_count, (uint32_t)2);
    EXPECT_EQ(last_register_verify_value, (uint8_t)1);

}

// @compliance DCC-S9.2.3-CS-021, DCC-S9.2.3-CS-024
TEST(DccServiceModeTaskRegister, read_cv_ack_on_value_42_returns_42) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    drive_scan_to(42);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)42);

}

TEST(DccServiceModeTaskRegister, read_cv_all_256_no_ack_returns_error) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);
    EXPECT_EQ(register_verify_count, (uint32_t)256);

}

TEST(DccServiceModeTaskRegister, read_cv_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskRegister, read_cv_returns_to_idle_after_complete) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack();

    EXPECT_TRUE(DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskRegister, read_cv_progress_uses_read_phase) {

    setup();
    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack();

    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_READ);

}

// ============================================================================
// write_cv — write then verify
// ============================================================================

TEST(DccServiceModeTaskRegister, write_cv_calls_register_write_with_mapped_register) {

    setup();
    DccServiceModeTaskRegister_write_cv(29, 0xA5, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    EXPECT_EQ(register_write_count, (uint32_t)1);
    EXPECT_EQ(last_register_write_reg, (uint8_t)5);
    EXPECT_EQ(last_register_write_value, (uint8_t)0xA5);

}

TEST(DccServiceModeTaskRegister, write_cv_unmapped_cv_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_write_cv(5, 0x55, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_write_count, (uint32_t)0);

}

// @compliance DCC-S9.2.3-CS-022
TEST(DccServiceModeTaskRegister, write_cv_verifies_after_write_complete) {

    setup();
    DccServiceModeTaskRegister_write_cv(1, 0x55, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_reg, (uint8_t)1);
    EXPECT_EQ(last_register_verify_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskRegister, write_cv_verify_ack_success) {

    setup();
    DccServiceModeTaskRegister_write_cv(1, 0x55, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0x55);

}

TEST(DccServiceModeTaskRegister, write_cv_verify_no_ack_verify_fail) {

    setup();
    DccServiceModeTaskRegister_write_cv(1, 0x55, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_no_ack();

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

TEST(DccServiceModeTaskRegister, write_cv_progress_phases) {

    setup();
    DccServiceModeTaskRegister_write_cv(1, 0x55, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_no_ack();
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_WRITE);

    step_ack();
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_VERIFY);

}

// ============================================================================
// read_bit — scan + extract
// ============================================================================

TEST(DccServiceModeTaskRegister, read_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_bit(1, 8, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, read_bit_unmapped_cv_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_read_bit(5, 0, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskRegister, read_bit_extracts_bit0_from_0xA5) {

    setup();
    DccServiceModeTaskRegister_read_bit(1, 0, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    drive_scan_to(0xA5);

    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskRegister, read_bit_extracts_bit1_from_0xA5) {

    setup();
    DccServiceModeTaskRegister_read_bit(1, 1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    drive_scan_to(0xA5);

    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

TEST(DccServiceModeTaskRegister, read_bit_scan_exhausted_returns_error) {

    setup();
    DccServiceModeTaskRegister_read_bit(1, 3, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 256; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

// ============================================================================
// write_bit — read-modify-write
// ============================================================================

TEST(DccServiceModeTaskRegister, write_bit_position_8_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_write_bit(1, 8, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, write_bit_unmapped_cv_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskRegister_write_bit(5, 0, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress));

}

// @compliance DCC-S9.2.3-CS-023
TEST(DccServiceModeTaskRegister, write_bit_scans_then_writes_with_bit_set) {

    setup();
    // byte 0x00, set bit 3 → 0x08, register 5 (CV29)
    DccServiceModeTaskRegister_write_bit(29, 3, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack(); // scan found 0x00

    EXPECT_EQ(register_write_count, (uint32_t)1);
    EXPECT_EQ(last_register_write_reg, (uint8_t)5);
    EXPECT_EQ(last_register_write_value, (uint8_t)0x08);

}

TEST(DccServiceModeTaskRegister, write_bit_clears_bit) {

    setup();
    // byte 0xFF, clear bit 3 → 0xF7
    DccServiceModeTaskRegister_write_bit(1, 3, false, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    drive_scan_to(0xFF);

    EXPECT_EQ(last_register_write_value, (uint8_t)0xF7);

}

TEST(DccServiceModeTaskRegister, write_bit_verify_after_write) {

    setup();
    DccServiceModeTaskRegister_write_bit(29, 3, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack();    // scan found 0x00, write 0x08 issued
    step_no_ack(); // write complete, verify issued

    EXPECT_EQ(last_register_verify_value, (uint8_t)0x08);

}

TEST(DccServiceModeTaskRegister, write_bit_verify_ack_success) {

    setup();
    DccServiceModeTaskRegister_write_bit(1, 3, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack();    // scan
    step_no_ack(); // write
    step_ack();    // verify

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskRegister, write_bit_verify_no_ack_fail) {

    setup();
    DccServiceModeTaskRegister_write_bit(1, 3, true, DCC_DECODER_TYPE_MOBILE, mock_on_complete, mock_on_progress);
    step_ack();    // scan
    step_no_ack(); // write
    step_no_ack(); // verify no ack

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

// ============================================================================
// factory_reset — write 8 to register 8
// ============================================================================

// @compliance DCC-S9.2.3-CS-020
TEST(DccServiceModeTaskRegister, factory_reset_writes_8_to_register_8) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskRegister_factory_reset(mock_on_complete));

    EXPECT_EQ(register_write_count, (uint32_t)1);
    EXPECT_EQ(last_register_write_reg, (uint8_t)8);
    EXPECT_EQ(last_register_write_value, (uint8_t)8);

}

TEST(DccServiceModeTaskRegister, factory_reset_completes_on_primitive_complete) {

    setup();
    DccServiceModeTaskRegister_factory_reset(mock_on_complete);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);

}

TEST(DccServiceModeTaskRegister, factory_reset_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskRegister_factory_reset(mock_on_complete));
    EXPECT_FALSE(DccServiceModeTaskRegister_factory_reset(mock_on_complete));

}

TEST(DccServiceModeTaskRegister, factory_reset_returns_to_idle) {

    setup();
    DccServiceModeTaskRegister_factory_reset(mock_on_complete);
    step_ack();

    EXPECT_TRUE(DccServiceModeTaskRegister_factory_reset(mock_on_complete));

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccServiceModeTaskRegister, null_on_complete_read_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_register_t interface = make_interface();
    DccServiceModeTaskRegister_initialize(&interface);

    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, NULL, mock_on_progress);
    step_ack();

}

TEST(DccServiceModeTaskRegister, null_on_progress_read_cv_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_register_t interface = make_interface();
    DccServiceModeTaskRegister_initialize(&interface);

    DccServiceModeTaskRegister_read_cv(1, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);

}

// ============================================================================
// on_primitive_complete when IDLE — no crash
// ============================================================================

TEST(DccServiceModeTaskRegister, on_primitive_complete_when_idle_no_crash) {

    setup();
    DccServiceModeTaskRegister_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// verify_value: single register verify (no scan)
// ============================================================================

TEST(DccServiceModeTaskRegister, verify_value_issues_single_verify) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskRegister_verify_value(8, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL));

    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_reg, (uint8_t)8);     /* CV8 -> register 8 (mobile) */
    EXPECT_EQ(last_register_verify_value, (uint8_t)0x42);

}

TEST(DccServiceModeTaskRegister, verify_value_success_on_ack) {

    setup();
    DccServiceModeTaskRegister_verify_value(8, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)0x42);

}

TEST(DccServiceModeTaskRegister, verify_value_fail_on_no_ack) {

    setup();
    DccServiceModeTaskRegister_verify_value(8, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL);
    step_no_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

TEST(DccServiceModeTaskRegister, verify_value_invalid_cv_rejected) {

    setup();
    /* CV 99 is not accessible in register mode -> rejected. */
    EXPECT_FALSE(DccServiceModeTaskRegister_verify_value(99, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL));
    EXPECT_EQ(register_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskRegister, verify_value_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskRegister_verify_value(8, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL));
    EXPECT_FALSE(DccServiceModeTaskRegister_verify_value(8, 0x42, DCC_DECODER_TYPE_MOBILE, mock_on_complete, NULL));

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */
