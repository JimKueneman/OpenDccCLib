/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC packet-timeout fail-safe (S-9.2.4 §4, CV11)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_failsafe.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Fakes: controllable microsecond clock + CV backing store
// ============================================================================

static uint32_t fake_now_usec;
static uint8_t fake_cv11;
static bool fake_cv_read_ok;

static uint32_t fake_enter_count;
static uint32_t fake_exit_count;

static uint32_t fake_get_timestamp_usec(void) {

    return fake_now_usec;

}

static bool fake_cv_read(uint16_t cv_number, uint8_t *value) {

    if (!fake_cv_read_ok) {

        return false;

    }

    if (cv_number == DCC_CV_PACKET_TIMEOUT) {

        *value = fake_cv11;
        return true;

    }

    *value = 0;
    return true;

}

static void fake_on_entered(void) {

    fake_enter_count++;

}

static void fake_on_exited(void) {

    fake_exit_count++;

}

static void reset_fakes(void) {

    fake_now_usec = 0;
    fake_cv11 = 0;
    fake_cv_read_ok = true;
    fake_enter_count = 0;
    fake_exit_count = 0;

}

static interface_dcc_failsafe_t make_interface(void) {

    interface_dcc_failsafe_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.cv_read = fake_cv_read;
    interface.get_timestamp_usec = fake_get_timestamp_usec;
    interface.on_failsafe_entered = fake_on_entered;
    interface.on_failsafe_exited = fake_on_exited;

    return interface;

}

/* CV11 = 200 maps to exactly 20.0 s with the 0.1 s/LSB unit -- the S-9.2.4
 * TIMEOUT_MAX floor. */
static const uint8_t CV11_20_SECONDS = 200;
static const uint32_t TWENTY_SECONDS_US = 20000000u;

// ============================================================================
// Initialization
// ============================================================================

TEST(DccFailsafe, initialize_does_not_crash) {

    reset_fakes();
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    EXPECT_FALSE(DccFailsafe_is_active());

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, initialize_stamps_clock_so_no_immediate_trip) {

    reset_fakes();
    fake_now_usec = 5000000u;          /* clock already well advanced */
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    /* Only 1 s passes since init -- nowhere near the 20 s timeout. */
    fake_now_usec += 1000000u;
    DccFailsafe_run();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 0u);

}

// ============================================================================
// CV11 = 0 disables the time-out (S-9.2.4 §4)
// ============================================================================

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, cv11_zero_disables_timeout) {

    reset_fakes();
    fake_cv11 = 0;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    /* Advance far beyond any plausible timeout. */
    fake_now_usec += 60000000u;        /* 60 s */
    DccFailsafe_run();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 0u);

}

// ============================================================================
// Threshold behavior
// ============================================================================

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, no_trip_just_below_threshold) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_now_usec += (TWENTY_SECONDS_US - 1u);
    DccFailsafe_run();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 0u);

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, trips_at_threshold) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_now_usec += TWENTY_SECONDS_US;   /* exactly 20.0 s */
    DccFailsafe_run();

    EXPECT_TRUE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 1u);

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, entered_fires_exactly_once) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_now_usec += TWENTY_SECONDS_US;
    DccFailsafe_run();
    fake_now_usec += 5000000u;            /* keep timing out */
    DccFailsafe_run();
    DccFailsafe_run();

    EXPECT_TRUE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 1u);      /* not re-fired */

}

// ============================================================================
// Recovery on a valid addressed packet
// ============================================================================

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, valid_packet_exits_failsafe_once) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_now_usec += TWENTY_SECONDS_US;
    DccFailsafe_run();
    EXPECT_TRUE(DccFailsafe_is_active());

    DccFailsafe_note_valid_packet();
    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_exit_count, 1u);

    /* A second packet must not re-fire exit. */
    DccFailsafe_note_valid_packet();
    EXPECT_EQ(fake_exit_count, 1u);

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, packet_while_not_tripped_does_not_fire_exit) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    DccFailsafe_note_valid_packet();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_exit_count, 0u);

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, packet_rearms_timer) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    /* Get close to the timeout, then receive a packet. */
    fake_now_usec += (TWENTY_SECONDS_US - 1000000u);   /* 19 s */
    DccFailsafe_note_valid_packet();

    /* Another 19 s -- still under 20 s since the re-arm. */
    fake_now_usec += (TWENTY_SECONDS_US - 1000000u);
    DccFailsafe_run();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 0u);

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, recovers_then_can_retrip) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_now_usec += TWENTY_SECONDS_US;
    DccFailsafe_run();
    DccFailsafe_note_valid_packet();      /* recover */

    fake_now_usec += TWENTY_SECONDS_US;   /* lose stream again */
    DccFailsafe_run();

    EXPECT_TRUE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 2u);
    EXPECT_EQ(fake_exit_count, 1u);

}

// ============================================================================
// Robustness
// ============================================================================

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, null_callbacks_are_safe) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    interface.on_failsafe_entered = NULL;
    interface.on_failsafe_exited = NULL;
    DccFailsafe_initialize(&interface);

    fake_now_usec += TWENTY_SECONDS_US;
    DccFailsafe_run();                    /* would crash if it deref'd NULL */
    EXPECT_TRUE(DccFailsafe_is_active());

    DccFailsafe_note_valid_packet();
    EXPECT_FALSE(DccFailsafe_is_active());

}

// @compliance DCC-S9.2.4-DEC-001
TEST(DccFailsafe, cv_read_failure_does_not_trip) {

    reset_fakes();
    fake_cv11 = CV11_20_SECONDS;
    interface_dcc_failsafe_t interface = make_interface();
    DccFailsafe_initialize(&interface);

    fake_cv_read_ok = false;              /* CV store unavailable */
    fake_now_usec += TWENTY_SECONDS_US;
    DccFailsafe_run();

    EXPECT_FALSE(DccFailsafe_is_active());
    EXPECT_EQ(fake_enter_count, 0u);

}
