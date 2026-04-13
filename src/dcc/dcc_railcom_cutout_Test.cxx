/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Cutout Timer
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_cutout.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint16_t last_timer_period = 0;
static uint32_t timer_start_count = 0;
static bool timer_stopped = false;
static bool hbridge_disabled = false;
static bool hbridge_enabled = false;
static bool uart_ch1_enabled = false;
static bool uart_ch1_disabled = false;
static bool uart_ch2_enabled = false;
static bool uart_ch2_disabled = false;
static bool cutout_complete_called = false;

/* Ordered event log for verifying callback sequence */
#define MAX_EVENT_LOG 32

static const char *event_log[MAX_EVENT_LOG];
static uint32_t event_log_count = 0;

static void log_event(const char *name) {

    if (event_log_count < MAX_EVENT_LOG) {

        event_log[event_log_count++] = name;

    }

}

static void mock_timer_one_shot_start(uint16_t period_usec) {

    last_timer_period = period_usec;
    timer_start_count++;
    log_event("timer_start");

}

static void mock_timer_one_shot_stop(void) {

    timer_stopped = true;
    log_event("timer_stop");

}

static void mock_hbridge_disable(void) {

    hbridge_disabled = true;
    log_event("hbridge_disable");

}

static void mock_hbridge_enable(void) {

    hbridge_enabled = true;
    log_event("hbridge_enable");

}

static void mock_uart_ch1_enable(void) {

    uart_ch1_enabled = true;
    log_event("uart_ch1_enable");

}

static void mock_uart_ch1_disable(void) {

    uart_ch1_disabled = true;
    log_event("uart_ch1_disable");

}

static void mock_uart_ch2_enable(void) {

    uart_ch2_enabled = true;
    log_event("uart_ch2_enable");

}

static void mock_uart_ch2_disable(void) {

    uart_ch2_disabled = true;
    log_event("uart_ch2_disable");

}

static void mock_on_cutout_complete(void) {

    cutout_complete_called = true;
    log_event("cutout_complete");

}

static void reset_mocks(void) {

    last_timer_period = 0;
    timer_start_count = 0;
    timer_stopped = false;
    hbridge_disabled = false;
    hbridge_enabled = false;
    uart_ch1_enabled = false;
    uart_ch1_disabled = false;
    uart_ch2_enabled = false;
    uart_ch2_disabled = false;
    cutout_complete_called = false;
    event_log_count = 0;

}

static interface_dcc_railcom_cutout_t make_interface(void) {

    interface_dcc_railcom_cutout_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.timer_one_shot_start = mock_timer_one_shot_start;
    interface.timer_one_shot_stop = mock_timer_one_shot_stop;
    interface.hbridge_disable = mock_hbridge_disable;
    interface.hbridge_enable = mock_hbridge_enable;
    interface.uart_ch1_enable = mock_uart_ch1_enable;
    interface.uart_ch1_disable = mock_uart_ch1_disable;
    interface.uart_ch2_enable = mock_uart_ch2_enable;
    interface.uart_ch2_disable = mock_uart_ch2_disable;
    interface.on_cutout_complete = mock_on_cutout_complete;

    return interface;

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccRailcomCutout, initialize_sets_idle) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

// ============================================================================
// Begin starts 88us delay
// ============================================================================

TEST(DccRailcomCutout, begin_starts_delay_timer) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);

    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));
    EXPECT_EQ(timer_start_count, (uint32_t)1);
    EXPECT_EQ(last_timer_period, (uint16_t)DCC_RAILCOM_CUTOUT_START_US);

}

// ============================================================================
// Full cutout sequence: DELAY -> CH1 -> CH2 -> IDLE
// ============================================================================

TEST(DccRailcomCutout, full_sequence_callback_order) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);

    /* ISR 1: 88us delay elapsed -> disable H-bridge, enable Ch1, start 464us */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(hbridge_disabled);
    EXPECT_TRUE(uart_ch1_enabled);
    EXPECT_EQ(last_timer_period, (uint16_t)DCC_RAILCOM_CH1_WINDOW_US);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* ISR 2: 464us Ch1 elapsed -> disable Ch1, enable Ch2, start 1080us */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(uart_ch1_disabled);
    EXPECT_TRUE(uart_ch2_enabled);
    EXPECT_EQ(last_timer_period,
              (uint16_t)(DCC_RAILCOM_CUTOUT_TOTAL_US - DCC_RAILCOM_CH1_WINDOW_US));
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* ISR 3: 1080us Ch2 elapsed -> disable Ch2, enable H-bridge, complete */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(uart_ch2_disabled);
    EXPECT_TRUE(hbridge_enabled);
    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, full_sequence_event_order) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */
    DccRailcomCutout_timer_isr(&context);  /* CH1 -> CH2 */
    DccRailcomCutout_timer_isr(&context);  /* CH2 -> IDLE */

    /* Verify exact callback ordering */
    ASSERT_EQ(event_log_count, (uint32_t)10);

    /* begin: timer_start(88) */
    EXPECT_STREQ(event_log[0], "timer_start");

    /* ISR 1 (DELAY): hbridge_disable, uart_ch1_enable, timer_start(464) */
    EXPECT_STREQ(event_log[1], "hbridge_disable");
    EXPECT_STREQ(event_log[2], "uart_ch1_enable");
    EXPECT_STREQ(event_log[3], "timer_start");

    /* ISR 2 (CH1): uart_ch1_disable, uart_ch2_enable, timer_start(1080) */
    EXPECT_STREQ(event_log[4], "uart_ch1_disable");
    EXPECT_STREQ(event_log[5], "uart_ch2_enable");
    EXPECT_STREQ(event_log[6], "timer_start");

    /* ISR 3 (CH2): uart_ch2_disable, hbridge_enable, cutout_complete */
    EXPECT_STREQ(event_log[7], "uart_ch2_disable");
    EXPECT_STREQ(event_log[8], "hbridge_enable");
    EXPECT_STREQ(event_log[9], "cutout_complete");

}

TEST(DccRailcomCutout, timer_periods_are_correct) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)88);

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)464);

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)1080);

    EXPECT_EQ(timer_start_count, (uint32_t)3);

}

// ============================================================================
// Spurious ISR in IDLE state is harmless
// ============================================================================

TEST(DccRailcomCutout, spurious_isr_in_idle_is_harmless) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_EQ(event_log_count, (uint32_t)0);

}

// ============================================================================
// Cancel during cutout
// ============================================================================

TEST(DccRailcomCutout, cancel_during_delay_stops_timer) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    /* H-bridge was not disabled during DELAY, so no re-enable needed */
    EXPECT_FALSE(hbridge_enabled);

}

TEST(DccRailcomCutout, cancel_during_ch1_reenables_hbridge) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1, H-bridge disabled */

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(hbridge_enabled);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, cancel_during_ch2_reenables_hbridge) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */
    DccRailcomCutout_timer_isr(&context);  /* CH1 -> CH2 */

    hbridge_enabled = false;  /* Reset to verify cancel re-enables */
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(hbridge_enabled);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, cancel_when_idle_is_harmless) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(timer_stopped);

}

// ============================================================================
// NULL interface guards
// ============================================================================

TEST(DccRailcomCutout, begin_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL);

    DccRailcomCutout_begin(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL);

    DccRailcomCutout_timer_isr(&context);

}

// ============================================================================
// Second cutout after first completes
// ============================================================================

TEST(DccRailcomCutout, second_cutout_after_first_succeeds) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    /* First cutout */
    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

    /* Second cutout */
    cutout_complete_called = false;
    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}
