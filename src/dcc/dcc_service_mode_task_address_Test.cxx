/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Task Address-Only orchestrator.
 *
 * Address-Only mode accesses only CV#1 (7-bit short address, 0-127). read scans
 * address_verify(0..127) until ACK; write is write + verify; read_bit scans then
 * extracts a bit (0-6); write_bit does read-modify-write + verify. Bit 7 is always
 * 0 in a 7-bit address, so it is rejected.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_task_address.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

// ============================================================================
// Mock / tracking state
// ============================================================================

static interface_dcc_service_mode_task_address_t _test_interface;

static uint8_t  last_address_verify;
static uint32_t address_verify_count;
static bool     address_verify_return;

static uint8_t  last_address_write;
static uint32_t address_write_count;
static bool     address_write_return;

static bool     is_idle_return;
static uint32_t on_start_ack_scan_count;

static dcc_service_mode_result_t on_complete_result;
static uint8_t                   on_complete_value;
static uint32_t                  on_complete_count;

static dcc_task_phase_enum on_progress_phase;
static uint8_t             on_progress_current_step;
static uint8_t             on_progress_estimated_steps;
static uint32_t            on_progress_count;

static bool mock_address_verify(uint8_t address) {

    last_address_verify = address;
    address_verify_count++;

    return address_verify_return;

}

static bool mock_address_write(uint8_t address) {

    last_address_write = address;
    address_write_count++;

    return address_write_return;

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

    last_address_verify = 0;
    address_verify_count = 0;
    address_verify_return = true;

    last_address_write = 0;
    address_write_count = 0;
    address_write_return = true;

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

static interface_dcc_service_mode_task_address_t make_interface(void) {

    interface_dcc_service_mode_task_address_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.address_verify    = mock_address_verify;
    interface.address_write     = mock_address_write;
    interface.is_idle           = mock_is_idle;
    interface.on_start_ack_scan = mock_on_start_ack_scan;

    return interface;

}

static void setup(void) {

    reset_mocks();
    _test_interface = make_interface();
    DccServiceModeTaskAddress_initialize(&_test_interface);

}

static void step_no_ack(void) {

    DccServiceModeTaskAddress_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

}

static void step_ack(void) {

    DccServiceModeTaskAddress_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

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

TEST(DccServiceModeTaskAddress, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_address_t interface = make_interface();
    DccServiceModeTaskAddress_initialize(&interface);

}

// ============================================================================
// read — scan 0-127
// ============================================================================

TEST(DccServiceModeTaskAddress, read_starts_with_address_0) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);

    EXPECT_EQ(address_verify_count, (uint32_t)1);
    EXPECT_EQ(last_address_verify, (uint8_t)0);

}

TEST(DccServiceModeTaskAddress, read_advances_after_no_ack) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(address_verify_count, (uint32_t)2);
    EXPECT_EQ(last_address_verify, (uint8_t)1);

}

TEST(DccServiceModeTaskAddress, read_ack_on_address_3_returns_3) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);
    drive_scan_to(3);

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)3);

}

TEST(DccServiceModeTaskAddress, read_ack_on_address_127_returns_127) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);
    drive_scan_to(127);

    EXPECT_EQ(on_complete_value, (uint8_t)127);

}

TEST(DccServiceModeTaskAddress, read_all_128_no_ack_returns_error) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 128; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

TEST(DccServiceModeTaskAddress, read_scans_at_most_128_addresses) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 128; i++) {

        step_no_ack();

    }

    EXPECT_EQ(address_verify_count, (uint32_t)128);

}

TEST(DccServiceModeTaskAddress, read_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskAddress, read_returns_to_idle_after_complete) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);
    step_ack();

    EXPECT_TRUE(DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskAddress, read_progress_uses_read_phase) {

    setup();
    DccServiceModeTaskAddress_read(mock_on_complete, mock_on_progress);
    step_ack();

    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_READ);

}

// ============================================================================
// write — input validation
// ============================================================================

TEST(DccServiceModeTaskAddress, write_address_0_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskAddress_write(0, mock_on_complete, mock_on_progress));
    EXPECT_EQ(address_write_count, (uint32_t)0);

}

TEST(DccServiceModeTaskAddress, write_address_128_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskAddress_write(128, mock_on_complete, mock_on_progress));
    EXPECT_EQ(address_write_count, (uint32_t)0);

}

TEST(DccServiceModeTaskAddress, write_address_127_accepted) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_write(127, mock_on_complete, mock_on_progress));
    EXPECT_EQ(address_write_count, (uint32_t)1);

}

TEST(DccServiceModeTaskAddress, write_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_write(3, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskAddress_write(3, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write — write then verify
// ============================================================================

TEST(DccServiceModeTaskAddress, write_calls_address_write) {

    setup();
    DccServiceModeTaskAddress_write(42, mock_on_complete, mock_on_progress);

    EXPECT_EQ(address_write_count, (uint32_t)1);
    EXPECT_EQ(last_address_write, (uint8_t)42);

}

TEST(DccServiceModeTaskAddress, write_verifies_after_write_complete) {

    setup();
    DccServiceModeTaskAddress_write(42, mock_on_complete, mock_on_progress);
    step_no_ack();

    EXPECT_EQ(address_verify_count, (uint32_t)1);
    EXPECT_EQ(last_address_verify, (uint8_t)42);

}

TEST(DccServiceModeTaskAddress, write_verify_ack_success) {

    setup();
    DccServiceModeTaskAddress_write(42, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)42);

}

TEST(DccServiceModeTaskAddress, write_verify_no_ack_verify_fail) {

    setup();
    DccServiceModeTaskAddress_write(42, mock_on_complete, mock_on_progress);
    step_no_ack();
    step_no_ack();

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);
    EXPECT_EQ(on_complete_value, (uint8_t)42);

}

TEST(DccServiceModeTaskAddress, write_progress_phases) {

    setup();
    DccServiceModeTaskAddress_write(42, mock_on_complete, mock_on_progress);
    step_no_ack();
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_WRITE);

    step_ack();
    EXPECT_EQ(on_progress_phase, DCC_TASK_PHASE_VERIFY);

}

// ============================================================================
// read_bit — validation
// ============================================================================

TEST(DccServiceModeTaskAddress, read_bit_position_7_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskAddress_read_bit(7, mock_on_complete, mock_on_progress));
    EXPECT_EQ(address_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskAddress, read_bit_position_6_accepted) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_read_bit(6, mock_on_complete, mock_on_progress));

}

TEST(DccServiceModeTaskAddress, read_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_read_bit(0, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskAddress_read_bit(0, mock_on_complete, mock_on_progress));

}

// ============================================================================
// read_bit — scan + extract
// ============================================================================

TEST(DccServiceModeTaskAddress, read_bit_extracts_bit0_from_0x55) {

    setup();
    // 0x55 = 0b1010101, bit 0 = 1
    DccServiceModeTaskAddress_read_bit(0, mock_on_complete, mock_on_progress);
    drive_scan_to(0x55);

    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskAddress, read_bit_extracts_bit1_from_0x55) {

    setup();
    // 0x55 = 0b1010101, bit 1 = 0
    DccServiceModeTaskAddress_read_bit(1, mock_on_complete, mock_on_progress);
    drive_scan_to(0x55);

    EXPECT_EQ(on_complete_value, (uint8_t)0);

}

TEST(DccServiceModeTaskAddress, read_bit_extracts_bit6_from_0x55) {

    setup();
    // 0x55 = 0b1010101, bit 6 = 1
    DccServiceModeTaskAddress_read_bit(6, mock_on_complete, mock_on_progress);
    drive_scan_to(0x55);

    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskAddress, read_bit_scan_exhausted_returns_error) {

    setup();
    DccServiceModeTaskAddress_read_bit(0, mock_on_complete, mock_on_progress);

    for (uint16_t i = 0; i < 128; i++) {

        step_no_ack();

    }

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_ERROR);

}

// ============================================================================
// write_bit — validation
// ============================================================================

TEST(DccServiceModeTaskAddress, write_bit_position_7_rejected) {

    setup();
    EXPECT_FALSE(DccServiceModeTaskAddress_write_bit(7, true, mock_on_complete, mock_on_progress));
    EXPECT_EQ(address_verify_count, (uint32_t)0);

}

TEST(DccServiceModeTaskAddress, write_bit_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskAddress_write_bit(0, true, mock_on_complete, mock_on_progress));
    EXPECT_FALSE(DccServiceModeTaskAddress_write_bit(0, true, mock_on_complete, mock_on_progress));

}

// ============================================================================
// write_bit — read-modify-write
// ============================================================================

TEST(DccServiceModeTaskAddress, write_bit_scans_then_writes_with_bit_set) {

    setup();
    // byte 0x00, set bit 2 → 0x04
    DccServiceModeTaskAddress_write_bit(2, true, mock_on_complete, mock_on_progress);
    step_ack(); // scan found 0x00

    EXPECT_EQ(address_write_count, (uint32_t)1);
    EXPECT_EQ(last_address_write, (uint8_t)0x04);

}

TEST(DccServiceModeTaskAddress, write_bit_clears_bit) {

    setup();
    // byte 0x7F, clear bit 2 → 0x7B
    DccServiceModeTaskAddress_write_bit(2, false, mock_on_complete, mock_on_progress);
    drive_scan_to(0x7F);

    EXPECT_EQ(last_address_write, (uint8_t)0x7B);

}

TEST(DccServiceModeTaskAddress, write_bit_verify_after_write) {

    setup();
    DccServiceModeTaskAddress_write_bit(2, true, mock_on_complete, mock_on_progress);
    step_ack();    // scan found 0x00, write 0x04
    step_no_ack(); // write complete, verify 0x04

    EXPECT_EQ(last_address_verify, (uint8_t)0x04);

}

TEST(DccServiceModeTaskAddress, write_bit_verify_ack_success) {

    setup();
    DccServiceModeTaskAddress_write_bit(2, true, mock_on_complete, mock_on_progress);
    step_ack();    // scan
    step_no_ack(); // write
    step_ack();    // verify

    EXPECT_EQ(on_complete_count, (uint32_t)1);
    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_complete_value, (uint8_t)1);

}

TEST(DccServiceModeTaskAddress, write_bit_verify_no_ack_fail) {

    setup();
    DccServiceModeTaskAddress_write_bit(2, true, mock_on_complete, mock_on_progress);
    step_ack();    // scan
    step_no_ack(); // write
    step_no_ack(); // verify no ack

    EXPECT_EQ(on_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccServiceModeTaskAddress, null_on_complete_read_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_address_t interface = make_interface();
    DccServiceModeTaskAddress_initialize(&interface);

    DccServiceModeTaskAddress_read(NULL, mock_on_progress);
    step_ack();

}

TEST(DccServiceModeTaskAddress, null_on_progress_read_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_address_t interface = make_interface();
    DccServiceModeTaskAddress_initialize(&interface);

    DccServiceModeTaskAddress_read(mock_on_complete, NULL);
    step_ack();

    EXPECT_EQ(on_complete_count, (uint32_t)1);

}

// ============================================================================
// on_primitive_complete when IDLE — no crash
// ============================================================================

TEST(DccServiceModeTaskAddress, on_primitive_complete_when_idle_no_crash) {

    setup();
    DccServiceModeTaskAddress_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */
