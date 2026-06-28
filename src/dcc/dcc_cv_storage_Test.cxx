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
static uint32_t factory_reset_count;

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

static void mock_factory_reset(void) {

    factory_reset_count++;

}

/* --- indexed CV mocks (CV257-512 window) --- */
static uint8_t last_idx_page_hi, last_idx_page_lo, last_idx_offset, last_idx_value;
static uint32_t idx_write_count, idx_read_count;
static uint8_t idx_store[256];

static bool mock_cv_write_indexed(uint8_t hi, uint8_t lo, uint8_t off, uint8_t val) {

    last_idx_page_hi = hi;
    last_idx_page_lo = lo;
    last_idx_offset = off;
    last_idx_value = val;
    idx_store[off] = val;
    idx_write_count++;
    return true;

}

static bool mock_cv_read_indexed(uint8_t hi, uint8_t lo, uint8_t off, uint8_t *val) {

    last_idx_page_hi = hi;
    last_idx_page_lo = lo;
    last_idx_offset = off;
    *val = idx_store[off];
    idx_read_count++;
    return true;

}

/* --- CV29 notify mock --- */
static dcc_cv29_flags_t last_cv29_flags;
static uint32_t cv29_notify_count;

static void mock_on_cv29(const dcc_cv29_flags_t *f) {

    last_cv29_flags = *f;
    cv29_notify_count++;

}

static bool mock_cv_write_fail(uint16_t cv_number, uint8_t value) {

    (void)cv_number;
    (void)value;
    return false;

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
    factory_reset_count = 0;
    last_idx_page_hi = last_idx_page_lo = last_idx_offset = last_idx_value = 0;
    idx_write_count = idx_read_count = 0;
    memset(idx_store, 0, sizeof(idx_store));
    cv29_notify_count = 0;
    memset(&last_cv29_flags, 0, sizeof(last_cv29_flags));

}

static interface_dcc_cv_storage_t make_interface(void) {

    interface_dcc_cv_storage_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.cv_read = mock_cv_read;
    interface.cv_write = mock_cv_write;
    interface.factory_reset = mock_factory_reset;
    interface.cv_read_indexed = mock_cv_read_indexed;
    interface.cv_write_indexed = mock_cv_write_indexed;
    interface.on_cv29_config_changed = mock_on_cv29;

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

    /* A write of 8 to CV8 (read-only Manufacturer ID) triggers factory_reset and
     * does NOT write the value -- even while locked (S-9.2.2: CV8 is unchangeable). */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_MANUFACTURER_ID, 8));
    EXPECT_EQ(factory_reset_count, (uint32_t)1);
    EXPECT_EQ(cv_write_count, (uint32_t)0);

}

TEST(DccCvStorage, cv8_write_with_null_factory_reset_no_crash) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    interface.factory_reset = NULL;
    DccCvStorage_initialize(&interface);

    /* No reset hook wired -> the CV8 write is still consumed (value not written), no crash. */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_MANUFACTURER_ID, 8));
    EXPECT_EQ(cv_write_count, (uint32_t)0);

}

// ============================================================================
// Indexed CV access (CV31/CV32 page pointer -> CV257-512 window)
// ============================================================================

// @compliance DCC-S9.2.2-DEC-005
TEST(DccCvStorage, indexed_write_routes_page_and_offset) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_INDEX_HIGH - 1] = 0x12;   /* CV31 page high */
    mock_cv_values[DCC_CV_INDEX_LOW  - 1] = 0x34;   /* CV32 page low  */

    /* CV (257 + 5) = 262 -> indexed write to page 0x1234, offset 5 */
    EXPECT_TRUE(DccCvStorage_write(262, 99));
    EXPECT_EQ(idx_write_count, (uint32_t)1);
    EXPECT_EQ(last_idx_page_hi, (uint8_t)0x12);
    EXPECT_EQ(last_idx_page_lo, (uint8_t)0x34);
    EXPECT_EQ(last_idx_offset, (uint8_t)5);
    EXPECT_EQ(last_idx_value, (uint8_t)99);
    EXPECT_EQ(cv_write_count, (uint32_t)0);         /* did not hit the flat store */

}

// @compliance DCC-S9.2.2-DEC-005
TEST(DccCvStorage, indexed_read_routes_page_and_offset) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_INDEX_HIGH - 1] = 0x12;
    mock_cv_values[DCC_CV_INDEX_LOW  - 1] = 0x34;
    idx_store[5] = 77;

    uint8_t v = 0;
    EXPECT_TRUE(DccCvStorage_read(262, &v));
    EXPECT_EQ(v, (uint8_t)77);
    EXPECT_EQ(last_idx_page_hi, (uint8_t)0x12);
    EXPECT_EQ(last_idx_offset, (uint8_t)5);

}

// @compliance DCC-S9.2.2-DEC-005
TEST(DccCvStorage, indexed_window_boundaries) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    EXPECT_TRUE(DccCvStorage_write(257, 1));
    EXPECT_EQ(last_idx_offset, (uint8_t)0);
    EXPECT_TRUE(DccCvStorage_write(512, 2));
    EXPECT_EQ(last_idx_offset, (uint8_t)255);
    EXPECT_EQ(idx_write_count, (uint32_t)2);

    /* CV256 is below the window -> flat path, not indexed */
    DccCvStorage_write(256, 3);
    EXPECT_EQ(idx_write_count, (uint32_t)2);

}

// @compliance DCC-S9.2.2-DEC-005
TEST(DccCvStorage, indexed_write_without_hook_nacks) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    interface.cv_write_indexed = NULL;
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_write(300, 5));        /* no indexed hook -> NACK */
    EXPECT_EQ(cv_write_count, (uint32_t)0);

}

// @compliance DCC-S9.2.2-DEC-005
TEST(DccCvStorage, indexed_write_blocked_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;  /* locked */

    EXPECT_FALSE(DccCvStorage_write(300, 5));
    EXPECT_EQ(idx_write_count, (uint32_t)0);

}

// ============================================================================
// CV29 decode + notify (DccCvStorage hands the app named config flags)
// ============================================================================

// @compliance DCC-S9.2.2-DEC-004
TEST(DccCvStorage, cv29_write_decodes_named_flags) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* bit0 dir + bit3 railcom + bit5 extended = 0x29 */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_CONFIG, 0x29));
    EXPECT_EQ(cv29_notify_count, (uint32_t)1);
    EXPECT_TRUE(last_cv29_flags.direction_reversed);
    EXPECT_FALSE(last_cv29_flags.speed_steps_28_128);
    EXPECT_FALSE(last_cv29_flags.power_source_conversion);
    EXPECT_TRUE(last_cv29_flags.railcom_enabled);
    EXPECT_FALSE(last_cv29_flags.speed_table_enabled);
    EXPECT_TRUE(last_cv29_flags.extended_address);
    EXPECT_FALSE(last_cv29_flags.accessory_decoder);

}

// @compliance DCC-S9.2.2-DEC-004
TEST(DccCvStorage, cv29_write_forces_reserved_bit6_to_zero) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    /* bogus byte: bit6 (reserved, 0x40) + bit1 (0x02) = 0x42 */
    EXPECT_TRUE(DccCvStorage_write(DCC_CV_CONFIG, 0x42));
    EXPECT_EQ(mock_cv_values[DCC_CV_CONFIG - 1], (uint8_t)0x02);   /* bit6 forced off */
    EXPECT_TRUE(last_cv29_flags.speed_steps_28_128);               /* decode reflects sanitized value */

}

// @compliance DCC-S9.2.2-DEC-004
TEST(DccCvStorage, cv29_notify_only_on_successful_store) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    interface.cv_write = mock_cv_write_fail;    /* store always fails */
    DccCvStorage_initialize(&interface);

    EXPECT_FALSE(DccCvStorage_write(DCC_CV_CONFIG, 0x06));
    EXPECT_EQ(cv29_notify_count, (uint32_t)0);   /* failed store -> no notify */

}

// @compliance DCC-S9.2.2-DEC-004
TEST(DccCvStorage, cv29_write_blocked_when_locked) {

    reset_mocks();
    interface_dcc_cv_storage_t interface = make_interface();
    DccCvStorage_initialize(&interface);

    mock_cv_values[DCC_CV_DECODER_LOCK_1 - 1] = 5;
    mock_cv_values[DCC_CV_DECODER_LOCK_2 - 1] = 10;  /* locked */

    EXPECT_FALSE(DccCvStorage_write(DCC_CV_CONFIG, 0x06));
    EXPECT_EQ(cv29_notify_count, (uint32_t)0);

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
