/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Task Detect orchestrator.
 *
 * Detect determines ALL supported service modes, returned as a
 * DCC_SERVICE_MODE_SUPPORTED_* bitmask. Stages: Direct, Paged, Register (all on
 * CV#8 / register 8 = Manufacturer ID), then Address-Only (CV#1).
 *
 * Direct uses the spec bit-verify method (§E lines 95-99): verify CV#8 bit 7 with
 * value 0, then value 1; ACK of either => Direct supported. When Direct is
 * supported, the remaining CV#8 bits are read so the byte value is known and the
 * Paged/Register stages collapse to a single value-verify instead of a 0..255 scan.
 * Whichever of Paged/Register scans first also learns the value for the other.
 * Address-Only is a separate 0..127 scan of CV#1. If nothing acknowledges,
 * supported_modes == 0 and result is NO_ACK.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_task_detect.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

// ============================================================================
// Mock / tracking state
// ============================================================================

static interface_dcc_service_mode_task_detect_t _test_interface;

static uint16_t last_direct_verify_bit_cv;
static uint8_t  last_direct_verify_bit_position;
static bool     last_direct_verify_bit_value;
static uint32_t direct_verify_bit_count;
static bool     direct_verify_bit_return;

static uint16_t last_paged_verify_cv;
static uint8_t  last_paged_verify_value;
static uint32_t paged_verify_count;
static bool     paged_verify_return;

static uint8_t  last_register_verify_reg;
static uint8_t  last_register_verify_value;
static uint32_t register_verify_count;
static bool     register_verify_return;

static uint8_t  last_address_verify;
static uint32_t address_verify_count;
static bool     address_verify_return;

static bool     is_idle_return;
static uint32_t on_start_ack_scan_count;

static dcc_service_mode_result_t on_detect_result;
static uint8_t                   on_detect_modes;
static uint32_t                  on_detect_count;

static bool mock_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    last_direct_verify_bit_cv = cv_number;
    last_direct_verify_bit_position = bit_position;
    last_direct_verify_bit_value = bit_value;
    direct_verify_bit_count++;

    return direct_verify_bit_return;

}

static bool mock_paged_verify(uint16_t cv_number, uint8_t value) {

    last_paged_verify_cv = cv_number;
    last_paged_verify_value = value;
    paged_verify_count++;

    return paged_verify_return;

}

static bool mock_register_verify(uint8_t register_number, uint8_t value) {

    last_register_verify_reg = register_number;
    last_register_verify_value = value;
    register_verify_count++;

    return register_verify_return;

}

static bool mock_address_verify(uint8_t address) {

    last_address_verify = address;
    address_verify_count++;

    return address_verify_return;

}

static bool mock_is_idle(void) {

    return is_idle_return;

}

static void mock_on_start_ack_scan(void) {

    on_start_ack_scan_count++;

}

static void mock_on_detect(dcc_service_mode_result_t result, uint8_t supported_modes) {

    on_detect_result = result;
    on_detect_modes = supported_modes;
    on_detect_count++;

}

static void reset_mocks(void) {

    last_direct_verify_bit_cv = 0;
    last_direct_verify_bit_position = 0;
    last_direct_verify_bit_value = false;
    direct_verify_bit_count = 0;
    direct_verify_bit_return = true;

    last_paged_verify_cv = 0;
    last_paged_verify_value = 0;
    paged_verify_count = 0;
    paged_verify_return = true;

    last_register_verify_reg = 0;
    last_register_verify_value = 0;
    register_verify_count = 0;
    register_verify_return = true;

    last_address_verify = 0;
    address_verify_count = 0;
    address_verify_return = true;

    is_idle_return = true;
    on_start_ack_scan_count = 0;

    on_detect_result = DCC_SERVICE_MODE_ERROR;
    on_detect_modes = 0xFF;
    on_detect_count = 0;

}

static interface_dcc_service_mode_task_detect_t make_interface(void) {

    interface_dcc_service_mode_task_detect_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.direct_verify_bit = mock_direct_verify_bit;
    interface.paged_verify      = mock_paged_verify;
    interface.register_verify   = mock_register_verify;
    interface.address_verify    = mock_address_verify;
    interface.is_idle           = mock_is_idle;
    interface.on_start_ack_scan = mock_on_start_ack_scan;

    return interface;

}

static void setup(void) {

    reset_mocks();
    _test_interface = make_interface();
    DccServiceModeTaskDetect_initialize(&_test_interface);

}

static void step_no_ack(void) {

    DccServiceModeTaskDetect_on_primitive_complete(DCC_SERVICE_MODE_NO_ACK);

}

static void step_ack(void) {

    DccServiceModeTaskDetect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

// Drive Direct detection + full CV#8 read for a decoder whose CV#8 == cv8.
// Leaves the state machine at the start of the Paged stage with value_known.
static void direct_present(uint8_t cv8) {

    if ((cv8 >> 7) & 1u) {

        step_no_ack(); // verify(8,7,0) fails
        step_ack();    // verify(8,7,1) acks -> bit7 = 1

    } else {

        step_ack();    // verify(8,7,0) acks -> bit7 = 0

    }

    // Read bits 6..0 to complete the byte.
    for (int b = 6; b >= 0; b--) {

        if ((cv8 >> b) & 1u) {

            step_ack();

        } else {

            step_no_ack();

        }

    }

}

// Drive Direct detection where Direct is not supported (both probes fail).
// Leaves the state machine at the start of the Paged scan (value unknown).
static void direct_absent(void) {

    step_no_ack(); // verify(8,7,0) fails
    step_no_ack(); // verify(8,7,1) fails

}

// Unknown-value scan: no-ack for 0..target-1, then ack at target.
static void scan_to(uint8_t target) {

    for (uint16_t v = 0; v < (uint16_t)target; v++) {

        step_no_ack();

    }

    step_ack();

}

static void scan_exhaust_256(void) {

    for (uint16_t v = 0; v < 256; v++) {

        step_no_ack();

    }

}

static void scan_exhaust_128(void) {

    for (uint16_t v = 0; v < 128; v++) {

        step_no_ack();

    }

}

// ============================================================================
// Initialization / lifecycle
// ============================================================================

TEST(DccServiceModeTaskDetect, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_detect_t interface = make_interface();
    DccServiceModeTaskDetect_initialize(&interface);

}

TEST(DccServiceModeTaskDetect, detect_mode_busy_rejected) {

    setup();
    EXPECT_TRUE(DccServiceModeTaskDetect_detect_mode(mock_on_detect));
    EXPECT_FALSE(DccServiceModeTaskDetect_detect_mode(mock_on_detect));

}

TEST(DccServiceModeTaskDetect, detect_mode_starts_with_direct_bit_verify) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);

    EXPECT_EQ(direct_verify_bit_count, (uint32_t)1);
    EXPECT_EQ(last_direct_verify_bit_cv, (uint16_t)8);
    EXPECT_EQ(last_direct_verify_bit_position, (uint8_t)7);
    EXPECT_FALSE(last_direct_verify_bit_value);

}

// ============================================================================
// Direct detection (spec method) + CV#8 read
// ============================================================================

TEST(DccServiceModeTaskDetect, direct_second_probe_uses_bit_value_1) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    step_no_ack(); // first probe (value 0) fails -> second probe

    EXPECT_EQ(direct_verify_bit_count, (uint32_t)2);
    EXPECT_EQ(last_direct_verify_bit_position, (uint8_t)7);
    EXPECT_TRUE(last_direct_verify_bit_value);

}

TEST(DccServiceModeTaskDetect, direct_ack_reads_remaining_7_bits) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42); // bit7=0 -> 1 detect probe + 7 read probes

    EXPECT_EQ(direct_verify_bit_count, (uint32_t)8);

}

TEST(DccServiceModeTaskDetect, direct_both_probes_fail_no_direct_flag) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    // After both Direct probes fail, a Paged scan must have started (value unknown)
    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(on_detect_count, (uint32_t)0);

}

// ============================================================================
// Value reuse: Direct supported => Paged/Register are single value-verifies
// ============================================================================

TEST(DccServiceModeTaskDetect, direct_supported_paged_is_single_verify_with_known_value) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42);

    // Paged stage should be a single verify of CV#8 == 0x42, NOT a scan.
    EXPECT_EQ(paged_verify_count, (uint32_t)1);
    EXPECT_EQ(last_paged_verify_cv, (uint16_t)8);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0x42);

}

TEST(DccServiceModeTaskDetect, direct_supported_register_is_single_verify_with_known_value) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42);
    step_ack(); // paged single verify acks -> register single verify

    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_reg, (uint8_t)8);
    EXPECT_EQ(last_register_verify_value, (uint8_t)0x42);

}

TEST(DccServiceModeTaskDetect, direct_high_bit_value_reused_correctly) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0xC5); // bit7=1 path

    EXPECT_EQ(last_paged_verify_value, (uint8_t)0xC5);

}

// ============================================================================
// Full capability bitmask results
// ============================================================================

TEST(DccServiceModeTaskDetect, all_modes_supported_sets_all_flags) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42);
    step_ack(); // paged verify acks
    step_ack(); // register verify acks
    step_ack(); // address scan acks at value 0

    EXPECT_EQ(on_detect_count, (uint32_t)1);
    EXPECT_EQ(on_detect_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_detect_modes, (uint8_t)(DCC_SERVICE_MODE_SUPPORTED_DIRECT |
                                         DCC_SERVICE_MODE_SUPPORTED_PAGED |
                                         DCC_SERVICE_MODE_SUPPORTED_REGISTER |
                                         DCC_SERVICE_MODE_SUPPORTED_ADDRESS));

}

TEST(DccServiceModeTaskDetect, only_direct_supported) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42);
    step_no_ack();        // paged verify fails
    step_no_ack();        // register verify fails
    scan_exhaust_128();   // address scan exhausts

    EXPECT_EQ(on_detect_count, (uint32_t)1);
    EXPECT_EQ(on_detect_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_EQ(on_detect_modes, (uint8_t)DCC_SERVICE_MODE_SUPPORTED_DIRECT);

}

TEST(DccServiceModeTaskDetect, direct_absent_paged_found_reuses_value_for_register) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_to(42);  // paged scan acks at 42 -> value learned

    // Register stage should now be a single verify of the learned value.
    EXPECT_EQ(register_verify_count, (uint32_t)1);
    EXPECT_EQ(last_register_verify_value, (uint8_t)42);

}

TEST(DccServiceModeTaskDetect, direct_absent_paged_found_flags) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_to(42);   // paged found
    step_no_ack(); // register verify fails
    scan_exhaust_128(); // address exhausts

    EXPECT_EQ(on_detect_modes, (uint8_t)DCC_SERVICE_MODE_SUPPORTED_PAGED);

}

TEST(DccServiceModeTaskDetect, direct_and_paged_absent_register_found) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256(); // paged scan exhausts (value still unknown)
    scan_to(7);         // register scan acks at 7
    scan_exhaust_128(); // address exhausts

    EXPECT_EQ(on_detect_modes, (uint8_t)DCC_SERVICE_MODE_SUPPORTED_REGISTER);

}

TEST(DccServiceModeTaskDetect, only_address_supported) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256(); // paged
    scan_exhaust_256(); // register
    scan_to(5);         // address acks at 5

    EXPECT_EQ(on_detect_modes, (uint8_t)DCC_SERVICE_MODE_SUPPORTED_ADDRESS);

}

TEST(DccServiceModeTaskDetect, nothing_supported_reports_zero_and_no_ack) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256(); // paged
    scan_exhaust_256(); // register
    scan_exhaust_128(); // address

    EXPECT_EQ(on_detect_count, (uint32_t)1);
    EXPECT_EQ(on_detect_result, DCC_SERVICE_MODE_NO_ACK);
    EXPECT_EQ(on_detect_modes, (uint8_t)0);

}

// ============================================================================
// Probe targeting
// ============================================================================

TEST(DccServiceModeTaskDetect, paged_scan_uses_cv8) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();

    EXPECT_EQ(last_paged_verify_cv, (uint16_t)8);
    EXPECT_EQ(last_paged_verify_value, (uint8_t)0);

}

TEST(DccServiceModeTaskDetect, register_scan_uses_register_8) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256(); // paged exhausts -> register scan starts

    EXPECT_EQ(last_register_verify_reg, (uint8_t)8);
    EXPECT_EQ(last_register_verify_value, (uint8_t)0);

}

TEST(DccServiceModeTaskDetect, address_scan_starts_at_zero) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256(); // paged
    scan_exhaust_256(); // register -> address scan starts

    EXPECT_EQ(address_verify_count, (uint32_t)1);
    EXPECT_EQ(last_address_verify, (uint8_t)0);

}

TEST(DccServiceModeTaskDetect, address_scans_at_most_128_values) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_absent();
    scan_exhaust_256();
    scan_exhaust_256();
    scan_exhaust_128();

    EXPECT_EQ(address_verify_count, (uint32_t)128);

}

// ============================================================================
// Lifecycle / safety
// ============================================================================

TEST(DccServiceModeTaskDetect, returns_to_idle_after_complete) {

    setup();
    DccServiceModeTaskDetect_detect_mode(mock_on_detect);
    direct_present(0x42);
    step_ack(); // paged
    step_ack(); // register
    step_ack(); // address

    EXPECT_TRUE(DccServiceModeTaskDetect_detect_mode(mock_on_detect));

}

TEST(DccServiceModeTaskDetect, null_on_detect_no_crash) {

    reset_mocks();
    interface_dcc_service_mode_task_detect_t interface = make_interface();
    DccServiceModeTaskDetect_initialize(&interface);

    DccServiceModeTaskDetect_detect_mode(NULL);
    direct_present(0x42);
    step_ack(); // paged
    step_ack(); // register
    step_ack(); // address

}

TEST(DccServiceModeTaskDetect, on_primitive_complete_when_idle_no_crash) {

    setup();
    DccServiceModeTaskDetect_on_primitive_complete(DCC_SERVICE_MODE_SUCCESS);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */
