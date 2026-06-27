/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC CV Storage (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_cv_storage.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock CV backing store
// ============================================================================

#define MOCK_CV_COUNT 256

static uint8_t mock_cv_values[MOCK_CV_COUNT];
static uint16_t last_cv_written;
static uint8_t last_cv_write_value;
static uint32_t cv_write_count;

static bool mock_cv_read(uint16_t cv_number, uint8_t *value) {

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    *value = mock_cv_values[cv_number - 1];
    return true;

}

static bool mock_cv_write(uint16_t cv_number, uint8_t value) {

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    mock_cv_values[cv_number - 1] = value;
    last_cv_written = cv_number;
    last_cv_write_value = value;
    cv_write_count++;
    return true;

}

static bool mock_cv_read_fail_cv15(uint16_t cv_number, uint8_t *value) {

    if (cv_number == DCC_CV_DECODER_LOCK_1)
        return false;

    return mock_cv_read(cv_number, value);

}

static bool mock_cv_read_fail_cv16(uint16_t cv_number, uint8_t *value) {

    if (cv_number == DCC_CV_DECODER_LOCK_2)
        return false;

    return mock_cv_read(cv_number, value);

}

static void reset_mocks(void) {

    memset(mock_cv_values, 0, sizeof(mock_cv_values));
    last_cv_written = 0;
    last_cv_write_value = 0;
    cv_write_count = 0;

}

static interface_dcc_cv_storage_t make_interface(void) {

    interface_dcc_cv_storage_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.cv_read = mock_cv_read;
    interface.cv_write = mock_cv_write;

    return interface;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccCvStorage, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

}

// ============================================================================
// Basic read/write
// ============================================================================

// @compliance DCC-S9.2.2-DEC-001
TEST(DccCvStorage, read_returns_stored_value) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[0] = 0x42;  /* CV 1 */
    uint8_t value = 0;
    EXPECT_TRUE(DccCvStorage_read(1, &value));
    EXPECT_EQ(value, (uint8_t)0x42);

}

// @compliance DCC-S9.2.2-DEC-001
TEST(DccCvStorage, write_stores_value_when_unlocked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* CV 15 and CV 16 both default to 0 — unlocked */
    EXPECT_TRUE(DccCvStorage_write(1, 0x55));
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x55);

}

// ============================================================================
// Decoder lock
// ============================================================================

// @compliance DCC-S9.2.2-DEC-002
TEST(DccCvStorage, is_locked_when_cv15_ne_cv16) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    EXPECT_TRUE(DccCvStorage_is_locked());

}

TEST(DccCvStorage, is_unlocked_when_cv15_eq_cv16) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 7;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 7;

    EXPECT_FALSE(DccCvStorage_is_locked());

}

TEST(DccCvStorage, is_locked_returns_false_when_cv15_read_fails) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    interface.cv_read = mock_cv_read_fail_cv15;
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_is_locked());

}

TEST(DccCvStorage, is_locked_returns_false_when_cv16_read_fails) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    interface.cv_read = mock_cv_read_fail_cv16;
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_is_locked());

}

// @compliance DCC-S9.2.2-DEC-002
TEST(DccCvStorage, write_blocked_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock the decoder */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    EXPECT_FALSE(DccCvStorage_write(1, 0x42));
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0);  /* unchanged */

}

TEST(DccCvStorage, cv15_always_writable_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock the decoder */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    /* CV 15 write should still succeed */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_DECODER_LOCK_1, 10));
    EXPECT_EQ(mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1], (uint8_t)10);

}

TEST(DccCvStorage, cv16_always_writable_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock the decoder */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    /* CV 16 write should still succeed */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_DECODER_LOCK_2, 5));
    EXPECT_EQ(mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1], (uint8_t)5);

}

// ============================================================================
// Factory reset
// ============================================================================

// @compliance DCC-S9.2.2-DEC-003
TEST(DccCvStorage, factory_reset_allowed_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock the decoder */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    /* Writing manufacturer ID (8) to CV 8 is factory reset — always allowed */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_MANUFACTURER_ID, 8));
    EXPECT_EQ(last_cv_written, (uint16_t)DCC_CV_MANUFACTURER_ID);
    EXPECT_EQ(last_cv_write_value, (uint8_t)8);

}

TEST(DccCvStorage, cv8_non_reset_value_blocked_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock the decoder */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;

    /* Writing non-8 value to CV 8 should be blocked */
    EXPECT_FALSE(DccCvStorage_write(DCC_CV_MANUFACTURER_ID, 42));

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccCvStorage, read_with_null_callback_returns_false) {

    reset_mocks();
    interface_dcc_cv_storage_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.cv_read = NULL;
    interface.cv_write = mock_cv_write;
    DccCvStorage_initialize(&interface);

    uint8_t value = 0;
    EXPECT_FALSE(DccCvStorage_read(1, &value));

}

TEST(DccCvStorage, write_with_null_callback_returns_false) {

    reset_mocks();
    interface_dcc_cv_storage_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.cv_read = mock_cv_read;
    interface.cv_write = NULL;
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_write(1, 0x42));

}

TEST(DccCvStorage, is_locked_with_null_read_returns_false) {

    reset_mocks();
    interface_dcc_cv_storage_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.cv_read = NULL;
    interface.cv_write = mock_cv_write;
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_is_locked());

}

// ============================================================================
// Unlock and write
// ============================================================================

// @compliance DCC-S9.2.2-DEC-002
TEST(DccCvStorage, unlock_then_write_succeeds) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* Lock */
    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;
    EXPECT_TRUE(DccCvStorage_is_locked());

    /* Unlock by making CV 15 == CV 16 */
    DccCvStorage_write(DCC_CV_DECODER_LOCK_1, 10);
    EXPECT_FALSE(DccCvStorage_is_locked());

    /* Now normal writes should work */
    EXPECT_TRUE(DccCvStorage_write(1, 0x99));
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x99);

}
