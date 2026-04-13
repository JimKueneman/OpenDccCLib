/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Application Main Track
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_main_track.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint16_t timer_start_period;
static uint32_t timer_start_count;
static uint32_t timer_stop_count;
static bool power_set_value;
static uint32_t power_set_count;
static uint32_t encoder_start_count;
static uint32_t encoder_stop_count;

static const dcc_packet_t *insert_packet;
static dcc_address_t insert_address;
static dcc_tag_enum insert_tag;
static dcc_priority_enum insert_priority;
static bool insert_auto_refresh;
static uint32_t insert_count;
static bool insert_return;

static dcc_address_t remove_address_value;
static uint32_t remove_address_count;
static uint32_t clear_count;

static void mock_timer_start(uint16_t period) {
    timer_start_period = period;
    timer_start_count++;
}

static void mock_timer_stop(void) {
    timer_stop_count++;
}

static void mock_power_set(bool enabled) {
    power_set_value = enabled;
    power_set_count++;
}

static void mock_encoder_start(void) {
    encoder_start_count++;
}

static void mock_encoder_stop(void) {
    encoder_stop_count++;
}

static bool mock_scheduler_insert(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {
    insert_packet = packet;
    insert_address = address;
    insert_tag = tag;
    insert_priority = priority;
    insert_auto_refresh = auto_refresh;
    insert_count++;
    return insert_return;
}

static void mock_scheduler_remove_address(dcc_address_t address) {
    remove_address_value = address;
    remove_address_count++;
}

static void mock_scheduler_clear(void) {
    clear_count++;
}

static void reset_mocks(void) {
    timer_start_period = 0;
    timer_start_count = 0;
    timer_stop_count = 0;
    power_set_value = false;
    power_set_count = 0;
    encoder_start_count = 0;
    encoder_stop_count = 0;
    insert_packet = NULL;
    insert_address = 0;
    insert_tag = (dcc_tag_enum)0;
    insert_priority = (dcc_priority_enum)0;
    insert_auto_refresh = false;
    insert_count = 0;
    insert_return = true;
    remove_address_value = 0;
    remove_address_count = 0;
    clear_count = 0;
}

static interface_dcc_application_main_track_t make_interface(void) {
    interface_dcc_application_main_track_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.timer_start = mock_timer_start;
    iface.timer_stop = mock_timer_stop;
    iface.track_power_set = mock_power_set;
    iface.encoder_start = mock_encoder_start;
    iface.encoder_stop = mock_encoder_stop;
    iface.scheduler_insert = mock_scheduler_insert;
    iface.scheduler_remove_address = mock_scheduler_remove_address;
    iface.scheduler_clear = mock_scheduler_clear;
    return iface;
}

// ============================================================================
// initialize
// ============================================================================

TEST(DccApplicationMainTrack, initialize_stores_interface) {
    reset_mocks();
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    DccApplicationMainTrack_power_on();
    EXPECT_EQ(timer_start_count, 1u);
}

TEST(DccApplicationMainTrack, initialize_with_null) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    DccApplicationMainTrack_power_on();
    EXPECT_EQ(timer_start_count, 0u);
}

// ============================================================================
// power_on
// ============================================================================

TEST(DccApplicationMainTrack, power_on_null_guard) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    DccApplicationMainTrack_power_on();
    EXPECT_EQ(power_set_count, 0u);
    EXPECT_EQ(timer_start_count, 0u);
    EXPECT_EQ(encoder_start_count, 0u);
}

TEST(DccApplicationMainTrack, power_on_delegates) {
    reset_mocks();
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    DccApplicationMainTrack_power_on();
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_TRUE(power_set_value);
    EXPECT_EQ(timer_start_count, 1u);
    EXPECT_EQ(timer_start_period, DCC_ONE_BIT_HALF_PERIOD_US);
    EXPECT_EQ(encoder_start_count, 1u);
}

// ============================================================================
// power_off
// ============================================================================

TEST(DccApplicationMainTrack, power_off_null_guard) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    DccApplicationMainTrack_power_off();
    EXPECT_EQ(encoder_stop_count, 0u);
    EXPECT_EQ(timer_stop_count, 0u);
    EXPECT_EQ(power_set_count, 0u);
}

TEST(DccApplicationMainTrack, power_off_delegates) {
    reset_mocks();
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    DccApplicationMainTrack_power_off();
    EXPECT_EQ(encoder_stop_count, 1u);
    EXPECT_EQ(timer_stop_count, 1u);
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_FALSE(power_set_value);
}

// ============================================================================
// insert
// ============================================================================

TEST(DccApplicationMainTrack, insert_null_guard) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    dcc_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    bool result = DccApplicationMainTrack_insert(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);
    EXPECT_FALSE(result);
    EXPECT_EQ(insert_count, 0u);
}

TEST(DccApplicationMainTrack, insert_delegates_and_returns_true) {
    reset_mocks();
    insert_return = true;
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    dcc_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.byte_count = 3;
    bool result = DccApplicationMainTrack_insert(&pkt, 42, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);
    EXPECT_TRUE(result);
    EXPECT_EQ(insert_count, 1u);
    EXPECT_EQ(insert_address, 42u);
    EXPECT_EQ(insert_tag, DCC_TAG_SPEED);
    EXPECT_EQ(insert_priority, DCC_PRIORITY_SPEED);
    EXPECT_TRUE(insert_auto_refresh);
    EXPECT_EQ(insert_packet, &pkt);
}

TEST(DccApplicationMainTrack, insert_delegates_and_returns_false) {
    reset_mocks();
    insert_return = false;
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    dcc_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    bool result = DccApplicationMainTrack_insert(&pkt, 1, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);
    EXPECT_FALSE(result);
    EXPECT_EQ(insert_count, 1u);
}

// ============================================================================
// remove_address
// ============================================================================

TEST(DccApplicationMainTrack, remove_address_null_guard) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    DccApplicationMainTrack_remove_address(10);
    EXPECT_EQ(remove_address_count, 0u);
}

TEST(DccApplicationMainTrack, remove_address_delegates) {
    reset_mocks();
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    DccApplicationMainTrack_remove_address(55);
    EXPECT_EQ(remove_address_count, 1u);
    EXPECT_EQ(remove_address_value, 55u);
}

// ============================================================================
// clear
// ============================================================================

TEST(DccApplicationMainTrack, clear_null_guard) {
    reset_mocks();
    DccApplicationMainTrack_initialize(NULL);
    DccApplicationMainTrack_clear();
    EXPECT_EQ(clear_count, 0u);
}

TEST(DccApplicationMainTrack, clear_delegates) {
    reset_mocks();
    interface_dcc_application_main_track_t iface = make_interface();
    DccApplicationMainTrack_initialize(&iface);
    DccApplicationMainTrack_clear();
    EXPECT_EQ(clear_count, 1u);
}

#endif /* DCC_COMPILE_COMMAND_STATION */
