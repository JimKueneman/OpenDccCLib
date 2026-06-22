/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Cutout Timer (5-state machine)
 *
 * State machine (action fires when each loaded duration EXPIRES;
 * cumulative spec event times in parens, defaults shown):
 *   DELAY    (26)  -> T_CS  = 26:  tristate H-bridge / begin cutout -> SETTLING
 *   SETTLING (54)  -> T_TS1 = 80:  enable UART Rx                   -> CH1
 *   CH1      (97)  -> T_TC1 = 177: disable UART Rx                  -> GAP
 *   GAP      (16)  -> T_TS2 = 193: enable UART Rx                   -> CH2
 *   CH2      (261) -> T_CE  = 454: disable UART Rx, restore H-bridge -> IDLE
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

/* Initialize a context with the spec-default timing values. */
static void init_with_defaults(dcc_railcom_cutout_context_t *context,
                               const interface_dcc_railcom_cutout_t *interface) {

    DccRailcomCutout_initialize(context, interface,
                                DCC_RAILCOM_CUTOUT_START_DELAY_US,
                                DCC_RAILCOM_UART_RX_DELAY_US,
                                DCC_RAILCOM_CH1_WINDOW_US,
                                DCC_RAILCOM_CH1_CH2_GAP_US,
                                DCC_RAILCOM_CH2_WINDOW_US);

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccRailcomCutout, initialize_sets_idle) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, initialize_stores_timing_fields) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface, 1, 2, 3, 4, 5);

    EXPECT_EQ(context.start_delay_us, (uint16_t)1);
    EXPECT_EQ(context.uart_rx_delay_us, (uint16_t)2);
    EXPECT_EQ(context.ch1_window_us, (uint16_t)3);
    EXPECT_EQ(context.ch1_ch2_gap_us, (uint16_t)4);
    EXPECT_EQ(context.ch2_window_us, (uint16_t)5);

}

// ============================================================================
// Begin starts DELAY one-shot (start_delay_us)
// ============================================================================

TEST(DccRailcomCutout, begin_starts_delay_timer) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);

    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));
    EXPECT_EQ(timer_start_count, (uint32_t)1);
    EXPECT_EQ(last_timer_period, (uint16_t)DCC_RAILCOM_CUTOUT_START_DELAY_US);
    EXPECT_EQ(last_timer_period, (uint16_t)26);

}

// ============================================================================
// Per-state one-shot periods: 26 / 54 / 97 / 16 / 261
// ============================================================================

TEST(DccRailcomCutout, per_state_timer_periods_are_correct) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* load DELAY */
    EXPECT_EQ(last_timer_period, (uint16_t)26);

    DccRailcomCutout_timer_isr(&context);        /* DELAY expiry -> load SETTLING */
    EXPECT_EQ(last_timer_period, (uint16_t)54);

    DccRailcomCutout_timer_isr(&context);        /* SETTLING expiry -> load CH1 */
    EXPECT_EQ(last_timer_period, (uint16_t)97);

    DccRailcomCutout_timer_isr(&context);        /* CH1 expiry -> load GAP */
    EXPECT_EQ(last_timer_period, (uint16_t)16);

    DccRailcomCutout_timer_isr(&context);        /* GAP expiry -> load CH2 */
    EXPECT_EQ(last_timer_period, (uint16_t)261);

    /* begin + 4 reprograms (DELAY,SETTLING,CH1,GAP); CH2 expiry does not reprogram */
    EXPECT_EQ(timer_start_count, (uint32_t)5);

}

// ============================================================================
// Full sequence: IDLE -> DELAY -> SETTLING -> CH1 -> GAP -> CH2 -> IDLE
// ============================================================================

TEST(DccRailcomCutout, full_sequence_state_transitions_and_actions) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    /* IDLE -> DELAY */
    DccRailcomCutout_begin(&context);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* DELAY expiry (T_CS): tristate H-bridge, NOT UART yet */
    DccRailcomCutout_timer_isr(&context);
    EXPECT_TRUE(cutout_began);
    EXPECT_FALSE(uart_rx_enabled);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* SETTLING expiry (T_TS1): enable UART Rx (Ch1 opens) */
    DccRailcomCutout_timer_isr(&context);
    EXPECT_TRUE(uart_rx_enabled);
    EXPECT_FALSE(uart_rx_disabled);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* CH1 expiry (T_TC1): disable UART Rx (Ch1 closes) */
    uart_rx_disabled = false;
    DccRailcomCutout_timer_isr(&context);
    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_FALSE(cutout_ended);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* GAP expiry (T_TS2): re-enable UART Rx (Ch2 opens) */
    uart_rx_enabled = false;
    DccRailcomCutout_timer_isr(&context);
    EXPECT_TRUE(uart_rx_enabled);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

    /* CH2 expiry (T_CE): disable UART Rx, restore H-bridge, complete */
    uart_rx_disabled = false;
    DccRailcomCutout_timer_isr(&context);
    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_TRUE(cutout_ended);
    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

// ============================================================================
// Full sequence: exact event ordering (incl. UART enable/disable across gap)
// ============================================================================

TEST(DccRailcomCutout, full_sequence_event_order) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* IDLE -> DELAY */
    DccRailcomCutout_timer_isr(&context);        /* DELAY -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* SETTLING -> CH1 */
    DccRailcomCutout_timer_isr(&context);        /* CH1 -> GAP */
    DccRailcomCutout_timer_isr(&context);        /* GAP -> CH2 */
    DccRailcomCutout_timer_isr(&context);        /* CH2 -> IDLE */

    /* begin: timer_start(26)
     * DELAY:    begin_railcom_cutout, timer_start(54)
     * SETTLING: uart_rx_enable,       timer_start(97)
     * CH1:      uart_rx_disable,      timer_start(16)
     * GAP:      uart_rx_enable,       timer_start(261)
     * CH2:      uart_rx_disable, end_railcom_cutout, cutout_complete
     *
     * Total: begin(1) + DELAY(2) + SETTLING(2) + CH1(2) + GAP(2) + CH2(3) = 12 */
    ASSERT_EQ(event_log_count, (uint32_t)12);

    EXPECT_STREQ(event_log[0], "timer_start");           /* begin DELAY */

    EXPECT_STREQ(event_log[1], "begin_railcom_cutout");  /* DELAY expiry */
    EXPECT_STREQ(event_log[2], "timer_start");

    EXPECT_STREQ(event_log[3], "uart_rx_enable");        /* SETTLING expiry: Ch1 opens */
    EXPECT_STREQ(event_log[4], "timer_start");

    EXPECT_STREQ(event_log[5], "uart_rx_disable");       /* CH1 expiry: Ch1 closes */
    EXPECT_STREQ(event_log[6], "timer_start");

    EXPECT_STREQ(event_log[7], "uart_rx_enable");        /* GAP expiry: Ch2 opens */
    EXPECT_STREQ(event_log[8], "timer_start");

    EXPECT_STREQ(event_log[9], "uart_rx_disable");       /* CH2 expiry: Ch2 closes */
    EXPECT_STREQ(event_log[10], "end_railcom_cutout");   /* restore H-bridge */
    EXPECT_STREQ(event_log[11], "cutout_complete");

}

// ============================================================================
// Spurious ISR in IDLE state is harmless
// ============================================================================

TEST(DccRailcomCutout, spurious_isr_in_idle_is_harmless) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_EQ(event_log_count, (uint32_t)0);

}

// ============================================================================
// Cancel during each active state
// ============================================================================

TEST(DccRailcomCutout, cancel_during_delay_stops_timer_no_restore) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* DELAY */
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    /* H-bridge not yet tristated during DELAY -> no restore */
    EXPECT_FALSE(cutout_ended);
    EXPECT_FALSE(uart_rx_disabled);

}

TEST(DccRailcomCutout, cancel_during_settling_ends_cutout) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* DELAY */
    DccRailcomCutout_timer_isr(&context);        /* DELAY -> SETTLING, H-bridge tristated */

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(cutout_ended);
    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, cancel_during_ch1_ends_cutout) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* DELAY */
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */

    uart_rx_disabled = false;
    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(timer_stopped);
    EXPECT_TRUE(cutout_ended);
    EXPECT_TRUE(uart_rx_disabled);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, cancel_during_gap_ends_cutout) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* DELAY */
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */
    DccRailcomCutout_timer_isr(&context);        /* -> GAP */

    cutout_ended = false;
    uart_rx_disabled = false;
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
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);            /* DELAY */
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */
    DccRailcomCutout_timer_isr(&context);        /* -> GAP */
    DccRailcomCutout_timer_isr(&context);        /* -> CH2 */

    cutout_ended = false;
    uart_rx_disabled = false;
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
    init_with_defaults(&context, &interface);

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(timer_stopped);

}

// ============================================================================
// NULL interface guards
// ============================================================================

TEST(DccRailcomCutout, begin_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL, 26, 54, 97, 16, 261);

    DccRailcomCutout_begin(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL, 26, 54, 97, 16, 261);

    DccRailcomCutout_timer_isr(&context);

}

TEST(DccRailcomCutout, cancel_with_null_interface_does_not_crash) {

    dcc_railcom_cutout_context_t context;
    DccRailcomCutout_initialize(&context, NULL, 26, 54, 97, 16, 261);

    DccRailcomCutout_cancel(&context);

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
    init_with_defaults(&context, &interface);

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
    interface.timer_one_shot_start = mock_timer_one_shot_start;
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);

    /* NULL out timer_one_shot_start so the DELAY ISR sees all relevant NULLs */
    interface.timer_one_shot_start = NULL;

    DccRailcomCutout_timer_isr(&context);        /* DELAY: begin=NULL, timer=NULL */

    EXPECT_FALSE(cutout_began);

}

TEST(DccRailcomCutout, timer_isr_settling_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);        /* DELAY -> SETTLING */

    interface.uart_rx_enable = NULL;
    interface.timer_one_shot_start = NULL;
    timer_start_count = 0;

    DccRailcomCutout_timer_isr(&context);        /* SETTLING: enable=NULL, timer=NULL */

    EXPECT_FALSE(uart_rx_enabled);
    EXPECT_EQ(timer_start_count, (uint32_t)0);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_ch1_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */

    interface.uart_rx_disable = NULL;
    interface.timer_one_shot_start = NULL;
    timer_start_count = 0;
    uart_rx_disabled = false;

    DccRailcomCutout_timer_isr(&context);        /* CH1: disable=NULL, timer=NULL */

    EXPECT_FALSE(uart_rx_disabled);
    EXPECT_EQ(timer_start_count, (uint32_t)0);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_gap_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */
    DccRailcomCutout_timer_isr(&context);        /* -> GAP */

    interface.uart_rx_enable = NULL;
    interface.timer_one_shot_start = NULL;
    timer_start_count = 0;
    uart_rx_enabled = false;

    DccRailcomCutout_timer_isr(&context);        /* GAP: enable=NULL, timer=NULL */

    EXPECT_FALSE(uart_rx_enabled);
    EXPECT_EQ(timer_start_count, (uint32_t)0);
    EXPECT_FALSE(DccRailcomCutout_is_idle(&context));

}

TEST(DccRailcomCutout, timer_isr_ch2_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */
    DccRailcomCutout_timer_isr(&context);        /* -> GAP */
    DccRailcomCutout_timer_isr(&context);        /* -> CH2 */

    interface.uart_rx_disable = NULL;
    interface.end_railcom_cutout = NULL;
    interface.on_cutout_complete = NULL;
    uart_rx_disabled = false;

    DccRailcomCutout_timer_isr(&context);        /* CH2 -> IDLE, all NULL */

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(uart_rx_disabled);
    EXPECT_FALSE(cutout_ended);
    EXPECT_FALSE(cutout_complete_called);

}

TEST(DccRailcomCutout, cancel_during_ch1_with_null_callbacks) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);        /* -> SETTLING */
    DccRailcomCutout_timer_isr(&context);        /* -> CH1 */

    interface.timer_one_shot_stop = NULL;
    interface.uart_rx_disable = NULL;
    interface.end_railcom_cutout = NULL;
    uart_rx_disabled = false;
    cutout_ended = false;

    DccRailcomCutout_cancel(&context);

    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));
    EXPECT_FALSE(timer_stopped);
    EXPECT_FALSE(uart_rx_disabled);
    EXPECT_FALSE(cutout_ended);

}

// ============================================================================
// Second cutout after first completes
// ============================================================================

TEST(DccRailcomCutout, second_cutout_after_first_succeeds) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    init_with_defaults(&context, &interface);

    /* First cutout: DELAY,SETTLING,CH1,GAP,CH2 = 5 ISRs */
    DccRailcomCutout_begin(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

    /* Second cutout */
    cutout_complete_called = false;
    last_timer_period = 0;
    DccRailcomCutout_begin(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)26);  /* reloads DELAY */

    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);
    DccRailcomCutout_timer_isr(&context);

    EXPECT_TRUE(cutout_complete_called);
    EXPECT_TRUE(DccRailcomCutout_is_idle(&context));

}

// ============================================================================
// 0 -> default substitution happens in dcc_config.c, not in the module.
// The module uses exactly the values passed to initialize. Verify that a
// caller passing custom (non-default) timings drives those periods.
// ============================================================================

TEST(DccRailcomCutout, custom_timings_drive_each_state_period) {

    reset_mocks();
    dcc_railcom_cutout_context_t context;
    interface_dcc_railcom_cutout_t interface = make_interface();
    DccRailcomCutout_initialize(&context, &interface, 10, 20, 30, 40, 50);

    DccRailcomCutout_begin(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)10);  /* DELAY */

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)20);  /* SETTLING */

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)30);  /* CH1 */

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)40);  /* GAP */

    DccRailcomCutout_timer_isr(&context);
    EXPECT_EQ(last_timer_period, (uint16_t)50);  /* CH2 */

}
