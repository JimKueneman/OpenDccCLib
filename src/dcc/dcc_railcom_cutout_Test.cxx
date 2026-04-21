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
static bool cutout_began = false;
static bool cutout_ended = false;
static bool uart_rx_enabled = false;
static bool uart_rx_disabled = false;
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

static void mock_begin_railcom_cutout(void) {

    cutout_began = true;
    log_event("begin_railcom_cutout");

}

static void mock_end_railcom_cutout(void) {

    cutout_ended = true;
    log_event("end_railcom_cutout");

}

static void mock_uart_rx_enable(void) {

    uart_rx_enabled = true;
    log_event("uart_rx_enable");

}

static void mock_uart_rx_disable(void) {

    uart_rx_disabled = true;
    log_event("uart_rx_disable");

}

static void mock_on_cutout_complete(void) {

    cutout_complete_called = true;
    log_event("cutout_complete");

}

static void reset_mocks(void) {

    last_timer_period = 0;
    timer_start_count = 0;
    timer_stopped = false;
    cutout_began = false;
    cutout_ended = false;
    uart_rx_enabled = false;
    uart_rx_disabled = false;
    cutout_complete_called = false;
    event_log_count = 0;

}

static interface_dcc_railcom_cutout_t make_interface(void) {

    interface_dcc_railcom_cutout_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.timer_one_shot_start = mock_timer_one_shot_start;
    interface.timer_one_shot_stop = mock_timer_one_shot_stop;
    interface.begin_railcom_cutout = mock_begin_railcom_cutout;
    interface.end_railcom_cutout = mock_end_railcom_cutout;
    interface.uart_rx_enable = mock_uart_rx_enable;
    interface.uart_rx_disable = mock_uart_rx_disable;
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

    /* ISR 1: 88us delay elapsed -> begin cutout, enable UART rx, start 464us */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(cutout_began);
    EXPECT_TRUE(uart_rx_enabled);
    EXPECT_EQ(last_timer_period, (uint16_t)DCC_RAILCOM_CH1_WINDOW_US);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* ISR 2: 464us Ch1 elapsed -> start 1080us (UART rx stays enabled) */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_EQ(last_timer_period,
              (uint16_t)(DCC_RAILCOM_CUTOUT_TOTAL_US - DCC_RAILCOM_CH1_WINDOW_US));
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* ISR 3: 1080us Ch2 elapsed -> disable UART rx, end cutout, complete */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_TRUE(cutout_ended);
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
    ASSERT_EQ(event_log_count, (uint32_t)8);

    /* begin: timer_start(88) */
    EXPECT_STREQ(event_log[0], "timer_start");

    /* ISR 1 (DELAY): begin_railcom_cutout, uart_rx_enable, timer_start(464) */
    EXPECT_STREQ(event_log[1], "begin_railcom_cutout");
    EXPECT_STREQ(event_log[2], "uart_rx_enable");
    EXPECT_STREQ(event_log[3], "timer_start");

    /* ISR 2 (CH1): timer_start(1080) — UART rx stays enabled */
    EXPECT_STREQ(event_log[4], "timer_start");

    /* ISR 3 (CH2): uart_rx_disable, end_railcom_cutout, cutout_complete */
    EXPECT_STREQ(event_log[5], "uart_rx_disable");
    EXPECT_STREQ(event_log[6], "end_railcom_cutout");
    EXPECT_STREQ(event_log[7], "cutout_complete");

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
    /* Cutout was not begun during DELAY, so no end needed */
    EXPECT_FALSE(cutout_ended);

}

TEST(DccRailcomCutout, cancel_during_ch1_ends_cutout) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1, cutout began */

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(cutout_ended);
    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, cancel_during_ch2_ends_cutout) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */
    DccRailcomCutout_timer_isr(&context);  /* CH1 -> CH2 */

    cutout_ended = false;  /* Reset to verify cancel ends cutout */
    uart_rx_disabled = false;  /* Reset to verify cancel disables UART */
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(cutout_ended);
    EXPECT_TRUE(uart_rx_disabled);
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

// ============================================================================
// Individual NULL callback guards (branch coverage)
// ============================================================================

TEST(DccRailcomCutout, begin_with_null_timer_one_shot_start) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    interface.timer_one_shot_start = NULL;
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);

    /* State should still transition to DELAY even though timer didn't start */
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));
    EXPECT_EQ(timer_start_count, (uint32_t)0);

}

TEST(DccRailcomCutout, timer_isr_delay_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface;
    memset(&interface, 0, sizeof(interface));
    /* Only timer_one_shot_start for begin(), all others NULL */
    interface.timer_one_shot_start = mock_timer_one_shot_start;
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);

    /* Now NULL out timer_one_shot_start so ISR DELAY state sees all NULL */
    interface.timer_one_shot_start = NULL;

    /* ISR in DELAY state: begin_railcom_cutout=NULL, uart_rx_enable=NULL,
     * timer_one_shot_start=NULL — all three NULL branches taken */
    DccRailcomCutout_timer_isr(&context);

    EXPECT_FALSE(cutout_began);
    EXPECT_FALSE(uart_rx_enabled);

}

TEST(DccRailcomCutout, timer_isr_ch1_with_null_timer_start) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */

    /* NULL out timer_one_shot_start before CH1 ISR */
    interface.timer_one_shot_start = NULL;
    timer_start_count = 0;

    DccRailcomCutout_timer_isr(&context);  /* CH1 -> CH2, timer_start NULL */

    EXPECT_EQ(timer_start_count, (uint32_t)0);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_ch2_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */
    DccRailcomCutout_timer_isr(&context);  /* CH1 -> CH2 */

    /* NULL out CH2 completion callbacks */
    interface.uart_rx_disable = NULL;
    interface.end_railcom_cutout = NULL;
    interface.on_cutout_complete = NULL;

    DccRailcomCutout_timer_isr(&context);  /* CH2 -> IDLE */

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(uart_rx_disabled);
    EXPECT_FALSE(cutout_ended);
    EXPECT_FALSE(cutout_complete_called);

}

TEST(DccRailcomCutout, cancel_during_ch1_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);  /* DELAY -> CH1 */

    /* NULL out cancel-path callbacks */
    interface.timer_one_shot_stop = NULL;
    interface.uart_rx_disable = NULL;
    interface.end_railcom_cutout = NULL;

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(timer_stopped);

}

TEST(DccRailcomCutout, cancel_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL);

    /* Force state to non-idle to verify cancel's NULL guard */
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}
