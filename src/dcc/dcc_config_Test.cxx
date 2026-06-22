/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Config — dual-channel wiring
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_config.h"
#include "dcc/dcc_application_command_station_main_track.h"
#include "dcc/dcc_application_command_station_service_track.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock hardware driver functions
// ============================================================================

static void mock_lock(void) {}
static void mock_unlock(void) {}
static uint32_t mock_timestamp(void) { return 0; }

#ifdef DCC_COMPILE_COMMAND_STATION

static void mock_main_timer_start(uint16_t period) { (void)period; }
static void mock_main_timer_stop(void) {}
static void mock_main_power_set(bool enabled) { (void)enabled; }

static void mock_svc_timer_start(uint16_t period) { (void)period; }
static void mock_svc_timer_stop(void) {}
static void mock_svc_power_set(bool enabled) { (void)enabled; }

static uint16_t mock_current_sense_read(void) { return 100; }

static uint32_t shared_timer_start_count = 0;
static uint32_t shared_timer_stop_count = 0;
static void mock_shared_timer_start(uint16_t period) { (void)period; shared_timer_start_count++; }
static void mock_shared_timer_stop(void) { shared_timer_stop_count++; }

static void mock_main_pin_toggle(void) {}
static void mock_svc_pin_toggle(void) {}

static uint32_t on_packet_sent_count = 0;
static void mock_on_packet_sent(const dcc_packet_t *packet) { (void)packet; on_packet_sent_count++; }

static uint16_t last_railcom_timer_period = 0;
static void mock_railcom_timer_start(uint16_t period) { last_railcom_timer_period = period; }
static void mock_railcom_timer_stop(void) {}

static void mock_begin_railcom_cutout(void) {}
static void mock_end_railcom_cutout(void) {}
static void mock_uart_rx_enable(void) {}
static void mock_uart_rx_disable(void) {}
static bool mock_uart_read(uint8_t *byte) { (void)byte; return false; }

static dcc_railcom_hw_t make_railcom_hw(void) {
    dcc_railcom_hw_t rc;
    memset(&rc, 0, sizeof(rc));
    rc.begin_railcom_cutout = mock_begin_railcom_cutout;
    rc.end_railcom_cutout = mock_end_railcom_cutout;
    rc.uart_rx_enable = mock_uart_rx_enable;
    rc.uart_rx_disable = mock_uart_rx_disable;
    rc.uart_read = mock_uart_read;
    return rc;
}

static dcc_packet_t make_idle_packet(void) {
    dcc_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.data[0] = 0xFF;
    pkt.data[1] = 0x00;
    pkt.data[2] = 0xFF;
    pkt.byte_count = 3;
    pkt.preamble_bits = DCC_PREAMBLE_BITS_OPS;
    pkt.repeat_count = 1;
    return pkt;
}

#endif

#ifdef DCC_COMPILE_DECODER
static bool mock_cv_read(uint16_t cv, uint8_t *val) { (void)cv; *val = 0; return true; }
static bool mock_cv_write(uint16_t cv, uint8_t val) { (void)cv; (void)val; return true; }
#endif

// ============================================================================
// Helper: build a minimal valid config
// ============================================================================

static dcc_config_t make_test_config(void) {

    dcc_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.lock_shared_resources = mock_lock;
    cfg.unlock_shared_resources = mock_unlock;
    cfg.get_timestamp_usec = mock_timestamp;

#ifdef DCC_COMPILE_COMMAND_STATION
    cfg.main_track.timer_start = mock_main_timer_start;
    cfg.main_track.timer_stop = mock_main_timer_stop;
    cfg.main_track.track_power_set = mock_main_power_set;

    cfg.service_track.timer_start = mock_svc_timer_start;
    cfg.service_track.timer_stop = mock_svc_timer_stop;
    cfg.service_track.track_power_set = mock_svc_power_set;
#endif

#ifdef DCC_COMPILE_DECODER
    cfg.cv_read = mock_cv_read;
    cfg.cv_write = mock_cv_write;
#endif

    return cfg;

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccConfig, initialize_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
}

TEST(DccConfig, run_without_init_does_not_crash) {
    DccConfig_run();
}

TEST(DccConfig, run_after_init_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccConfig_run();
}

TEST(DccConfig, run_null_guard_returns) {
    DccConfig_initialize(NULL);
    DccConfig_run();
}

#ifdef DCC_COMPILE_COMMAND_STATION


// ============================================================================
// 100ms tick
// ============================================================================

TEST(DccConfig, timer_100ms_tick_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccConfig_100ms_timer_tick();
}

// ============================================================================
// Main track power control
// ============================================================================

static bool main_power_set_called = false;
static bool main_power_set_value = false;

static void mock_main_power_set_tracking(bool enabled) {
    main_power_set_called = true;
    main_power_set_value = enabled;
}

TEST(DccConfig, main_track_power_on_calls_driver) {
    dcc_config_t cfg = make_test_config();
    cfg.main_track.track_power_set = mock_main_power_set_tracking;
    DccConfig_initialize(&cfg);

    main_power_set_called = false;
    DccApplicationCommandStationMainTrack_power_on();
    EXPECT_TRUE(main_power_set_called);
    EXPECT_TRUE(main_power_set_value);
}

TEST(DccConfig, main_track_power_off_calls_driver) {
    dcc_config_t cfg = make_test_config();
    cfg.main_track.track_power_set = mock_main_power_set_tracking;
    DccConfig_initialize(&cfg);

    main_power_set_called = false;
    DccApplicationCommandStationMainTrack_power_off();
    EXPECT_TRUE(main_power_set_called);
    EXPECT_FALSE(main_power_set_value);
}

TEST(DccConfig, main_track_power_on_null_guard) {
    DccConfig_initialize(NULL);
    DccApplicationCommandStationMainTrack_power_on();
}

TEST(DccConfig, main_track_power_off_null_guard) {
    DccConfig_initialize(NULL);
    DccApplicationCommandStationMainTrack_power_off();
}

// ============================================================================
// Service track power control
// ============================================================================

static bool svc_power_set_called = false;
static bool svc_power_set_value = false;

static void mock_svc_power_set_tracking(bool enabled) {
    svc_power_set_called = true;
    svc_power_set_value = enabled;
}

TEST(DccConfig, service_track_power_on_calls_driver) {
    dcc_config_t cfg = make_test_config();
    cfg.service_track.track_power_set = mock_svc_power_set_tracking;
    DccConfig_initialize(&cfg);

    svc_power_set_called = false;
    DccApplicationCommandStationServiceTrack_power_on();
    EXPECT_TRUE(svc_power_set_called);
    EXPECT_TRUE(svc_power_set_value);
}

TEST(DccConfig, service_track_power_off_calls_driver) {
    dcc_config_t cfg = make_test_config();
    cfg.service_track.track_power_set = mock_svc_power_set_tracking;
    DccConfig_initialize(&cfg);

    svc_power_set_called = false;
    DccApplicationCommandStationServiceTrack_power_off();
    EXPECT_TRUE(svc_power_set_called);
    EXPECT_FALSE(svc_power_set_value);
}

TEST(DccConfig, service_track_power_on_null_guard) {
    DccConfig_initialize(NULL);
    DccApplicationCommandStationServiceTrack_power_on();
}

TEST(DccConfig, service_track_power_off_null_guard) {
    DccConfig_initialize(NULL);
    DccApplicationCommandStationServiceTrack_power_off();
}

// ============================================================================
// Service mode entry/exit via public API
// ============================================================================

TEST(DccConfig, enter_service_mode_returns_true) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_enter_service_mode());
}

TEST(DccConfig, exit_service_mode_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, run_calls_service_mode_when_active) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccConfig_run();
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

// ============================================================================
// Dual-channel independence: both ISRs callable in same test
// ============================================================================

TEST(DccConfig, both_tracks_power_independently) {
    dcc_config_t cfg = make_test_config();
    cfg.main_track.track_power_set = mock_main_power_set_tracking;
    cfg.service_track.track_power_set = mock_svc_power_set_tracking;
    DccConfig_initialize(&cfg);

    /* Power on main, service stays off */
    main_power_set_called = false;
    svc_power_set_called = false;
    DccApplicationCommandStationMainTrack_power_on();
    EXPECT_TRUE(main_power_set_called);
    EXPECT_FALSE(svc_power_set_called);

    /* Power on service independently */
    main_power_set_called = false;
    svc_power_set_called = false;
    DccApplicationCommandStationServiceTrack_power_on();
    EXPECT_FALSE(main_power_set_called);
    EXPECT_TRUE(svc_power_set_called);
}

// ============================================================================
// Group 1: Main track scheduler wrappers
// ============================================================================

TEST(DccConfig, main_track_insert_packet) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    dcc_packet_t pkt = make_idle_packet();
    bool result = DccApplicationCommandStationMainTrack_add_to_auto_refresh(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    EXPECT_TRUE(result);
}

TEST(DccConfig, main_track_remove_address) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);

    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_add_to_auto_refresh(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccApplicationCommandStationMainTrack_remove_from_auto_refresh(3);
}

TEST(DccConfig, main_track_clear) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);

    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_add_to_auto_refresh(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccApplicationCommandStationMainTrack_remove_all_auto_refresh();
}

// ============================================================================
// Group 2: Service mode is_active
// ============================================================================

TEST(DccConfig, service_track_is_active_false) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_is_service_mode_active());
}

TEST(DccConfig, service_track_is_active_true) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_is_service_mode_active());
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

// ============================================================================
// Group 3: Service mode programming wrappers
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

TEST(DccConfig, direct_write_byte) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_byte(17, 0x42);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_verify_byte) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_verify_byte(17, 0x42);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_write_bit) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_bit(17, 3, true);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_verify_bit) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_verify_bit(17, 3, true);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

TEST(DccConfig, paged_write) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_paged_write(29, 0x50);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, paged_verify) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_paged_verify(29, 0x50);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

TEST(DccConfig, register_write) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_register_write(1, 0x35);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, register_verify) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_register_verify(1, 0x35);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

TEST(DccConfig, address_write) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_address_write(42);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, address_verify) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_address_verify(42);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

// ============================================================================
// Group 4: Shared timer ref-counting
// ============================================================================

TEST(DccConfig, shared_timer_acquire_first) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    shared_timer_start_count = 0;
    DccApplicationCommandStationMainTrack_power_on();
    EXPECT_EQ(shared_timer_start_count, (uint32_t)1);
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, shared_timer_acquire_second_no_extra_start) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    shared_timer_start_count = 0;
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationServiceTrack_power_on();
    EXPECT_EQ(shared_timer_start_count, (uint32_t)1);
    DccApplicationCommandStationServiceTrack_power_off();
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, shared_timer_release_to_zero) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    shared_timer_stop_count = 0;
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationMainTrack_power_off();
    EXPECT_EQ(shared_timer_stop_count, (uint32_t)1);
}

TEST(DccConfig, shared_timer_release_not_zero) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    shared_timer_stop_count = 0;
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationServiceTrack_power_on();
    DccApplicationCommandStationMainTrack_power_off();
    EXPECT_EQ(shared_timer_stop_count, (uint32_t)0);
    DccApplicationCommandStationServiceTrack_power_off();
    EXPECT_EQ(shared_timer_stop_count, (uint32_t)1);
}

TEST(DccConfig, shared_timer_release_underflow_guard) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    shared_timer_stop_count = 0;
    /* power_off without power_on: ref_count is 0, should not underflow */
    DccApplicationCommandStationMainTrack_power_off();
    EXPECT_EQ(shared_timer_stop_count, (uint32_t)1);
}

TEST(DccConfig, shared_timer_acquire_null_start_callback) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    /* Null out the callback after wiring — _shared_timer_acquire is
     * already installed as timer_start but now the guard sees NULL. */
    cfg.shared_timer_start = NULL;
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, shared_timer_release_null_stop_callback) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);

    DccApplicationCommandStationMainTrack_power_on();
    /* Null out the callback after wiring */
    cfg.shared_timer_stop = NULL;
    DccApplicationCommandStationMainTrack_power_off();
}

// ============================================================================
// Helpers for ISR-pumping tests
// ============================================================================

/* Pump the main track ISR until the bit encoder completes a full packet
 * and returns to idle. Safety limit prevents infinite loop. */
static void pump_main_track_until_idle(uint16_t max_cycles) {
    uint16_t i;
    for (i = 0; i < max_cycles; i++) {
        DccConfig_58us_timer_isr();
    }
}

/* Interleave run() and ISR to drive service mode state machine through
 * at least one packet load + transmission cycle. */
static void pump_service_mode_cycle(uint16_t outer_cycles) {
    for (uint16_t c = 0; c < outer_cycles; c++) {
        DccConfig_run();
        for (uint16_t i = 0; i < 200; i++) {
            DccConfig_58us_timer_isr();
        }
    }
}

// ============================================================================
// Group 5: ISRs
// ============================================================================

TEST(DccConfig, shared_timer_isr_basic) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationServiceTrack_power_on();
    DccConfig_58us_timer_isr();
    DccConfig_58us_timer_isr();
    DccApplicationCommandStationServiceTrack_power_off();
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, shared_timer_isr_toggle_next_false) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    DccConfig_initialize(&cfg);
    /* Call ISR without starting encoders — toggle_next is false */
    DccConfig_58us_timer_isr();
}

TEST(DccConfig, shared_timer_isr_null_pin_toggle) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    /* pin_toggle left as NULL — toggle_next will be true after start
     * but pin_toggle is NULL so the branch should short-circuit. */
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();
    DccApplicationCommandStationServiceTrack_power_on();
    DccConfig_58us_timer_isr();
    DccConfig_58us_timer_isr();
    DccApplicationCommandStationServiceTrack_power_off();
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, shared_timer_isr_with_current_sense) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    cfg.service_track.current_sense_read = mock_current_sense_read;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();
    DccConfig_58us_timer_isr();
    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, railcom_cutout_timer_isr_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);
    DccConfig_railcom_oneshot_timer_isr();
}

TEST(DccConfig, railcom_cutout_full_cycle_via_isr) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    /* Insert a packet and let scheduler load it */
    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();

    /* Pump ISR to transmit the full packet including end bit.
     * At end bit, _railcom_cutout_begin_wrapper fires and the encoder
     * enters RAILCOM_CUTOUT state. */
    pump_main_track_until_idle(200);

    /* Drive the cutout state machine through all 5 states:
     * DELAY → SETTLING → CH1 → GAP → CH2 → IDLE.
     * On CH2→IDLE, _railcom_on_cutout_complete fires. */
    DccConfig_railcom_oneshot_timer_isr();  /* DELAY -> SETTLING */
    DccConfig_railcom_oneshot_timer_isr();  /* SETTLING -> CH1 */
    DccConfig_railcom_oneshot_timer_isr();  /* CH1 -> GAP */
    DccConfig_railcom_oneshot_timer_isr();  /* GAP -> CH2 */
    DccConfig_railcom_oneshot_timer_isr();  /* CH2 -> IDLE, complete */

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, railcom_cutout_zero_config_uses_spec_defaults) {
    /* Leaving the five timing fields at 0 in the config must make
     * dcc_config.c substitute the dcc_defines spec defaults. The first
     * one-shot period loaded by begin() is the DELAY duration. */
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    /* All five railcom_cutout_*_us fields are 0 (memset in make_test_config). */
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();

    last_railcom_timer_period = 0;
    pump_main_track_until_idle(200);  /* end bit fires begin() -> loads DELAY */
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)DCC_RAILCOM_CUTOUT_START_DELAY_US);

    DccConfig_railcom_oneshot_timer_isr();  /* DELAY expiry -> loads SETTLING */
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)DCC_RAILCOM_UART_RX_DELAY_US);

    DccConfig_railcom_oneshot_timer_isr();  /* SETTLING expiry -> loads CH1 */
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)DCC_RAILCOM_CH1_WINDOW_US);

    DccConfig_railcom_oneshot_timer_isr();  /* CH1 expiry -> loads GAP */
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)DCC_RAILCOM_CH1_CH2_GAP_US);

    DccConfig_railcom_oneshot_timer_isr();  /* GAP expiry -> loads CH2 */
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)DCC_RAILCOM_CH2_WINDOW_US);

    DccConfig_railcom_oneshot_timer_isr();  /* CH2 expiry -> IDLE */

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, railcom_cutout_nonzero_config_overrides_defaults) {
    /* A non-zero config value must override the spec default. */
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    cfg.railcom_cutout_start_delay_us = 7;  /* custom DELAY */
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();

    last_railcom_timer_period = 0;
    pump_main_track_until_idle(200);
    EXPECT_EQ(last_railcom_timer_period, (uint16_t)7);

    /* Pump to completion to leave the state machine idle. */
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();

    DccApplicationCommandStationMainTrack_power_off();
}

// ============================================================================
// Group 6: DccConfig_initialize branch coverage
// ============================================================================

TEST(DccConfig, init_with_railcom_new_arch) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);
}

TEST(DccConfig, init_with_railcom_legacy_arch) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    /* railcom_timer_start is NULL → legacy cutout architecture */
    DccConfig_initialize(&cfg);
}

TEST(DccConfig, init_with_shared_timer) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    DccConfig_initialize(&cfg);
}

TEST(DccConfig, init_with_on_packet_sent) {
    dcc_config_t cfg = make_test_config();
    cfg.on_packet_sent = mock_on_packet_sent;
    DccConfig_initialize(&cfg);
}

// ============================================================================
// Group 7: Callback wrappers via ISR simulation
// ============================================================================

TEST(DccConfig, main_on_packet_complete_via_isr) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    /* Insert a packet so the scheduler has something to transmit */
    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);

    /* Scheduler_run picks up the packet and loads it into the encoder */
    DccConfig_run();

    /* Pump ISR to transmit: preamble(14*2) + start(2) + 3 bytes(3*16)
     * + byte_seps(2*2) + end(2) = 86 half-bit ISR calls, add margin */
    pump_main_track_until_idle(200);

    /* On completion, _main_on_packet_complete fires and sets
     * packet_complete_flag in the scheduler. A second run() picks it up. */
    DccConfig_run();

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, service_load_packet_via_isr) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);

    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_byte(1, 0x55);

    /* Drive state machine: run() loads packets, ISR transmits them */
    pump_service_mode_cycle(10);

    DccApplicationCommandStationServiceTrack_exit_service_mode();
}


TEST(DccConfig, service_load_packet_with_on_packet_sent) {
    dcc_config_t cfg = make_test_config();
    cfg.on_packet_sent = mock_on_packet_sent;
    DccConfig_initialize(&cfg);

    on_packet_sent_count = 0;
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_byte(1, 0x55);

    /* Drive state machine: run() loads packets, ISR transmits them */
    pump_service_mode_cycle(10);

    EXPECT_GT(on_packet_sent_count, (uint32_t)0);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

// ============================================================================
// Group 8: ISR branch coverage — current_sense_read NULL
// ============================================================================

TEST(DccConfig, isr_null_current_sense_read) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    /* service_track.current_sense_read is NULL (default) */
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();
    DccConfig_58us_timer_isr();
    DccConfig_58us_timer_isr();
    DccApplicationCommandStationMainTrack_power_off();
}

// ============================================================================
// Group 9: 100ms tick NOP scheduling branch coverage
// ============================================================================

static uint32_t mock_srq_callback_count = 0;
static void mock_on_accessory_srq(uint16_t address, bool is_extended) {
    (void)address; (void)is_extended; mock_srq_callback_count++;
}

TEST(DccConfig, nop_tick_fires_at_50) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    cfg.on_accessory_srq = mock_on_accessory_srq;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    /* Pump 49 ticks — no NOP yet (counter < 50) */
    for (int i = 0; i < 49; i++) {
        DccConfig_100ms_timer_tick();
    }

    /* 50th tick should fire the NOP packet scheduling */
    DccConfig_100ms_timer_tick();

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, nop_tick_no_railcom_skips) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    /* main_track.railcom = NULL — first part of compound && is false */
    cfg.on_accessory_srq = mock_on_accessory_srq;
    DccConfig_initialize(&cfg);

    mock_srq_callback_count = 0;
    for (int i = 0; i < 60; i++) {
        DccConfig_100ms_timer_tick();
    }
    /* No crash, no NOP scheduled because railcom is NULL */
}

TEST(DccConfig, nop_tick_no_srq_callback_skips) {
    dcc_config_t cfg = make_test_config();
    cfg.shared_timer_start = mock_shared_timer_start;
    cfg.shared_timer_stop = mock_shared_timer_stop;
    cfg.main_track.pin_toggle = mock_main_pin_toggle;
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    /* on_accessory_srq = NULL — second part of compound && is false */
    DccConfig_initialize(&cfg);

    for (int i = 0; i < 60; i++) {
        DccConfig_100ms_timer_tick();
    }
    /* No crash, no NOP scheduled because on_accessory_srq is NULL */
}

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

TEST(DccConfig, decoder_edge_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccConfig_decoder_edge_isr(1000);
}

#endif /* DCC_COMPILE_DECODER */

// ============================================================================
// dcc_types.h compile-time validation tests
// ============================================================================

TEST(DccTypes, packet_max_bytes_is_six) {
    EXPECT_EQ(DCC_PACKET_MAX_BYTES, 6);
}

TEST(DccTypes, packet_struct_size) {
    dcc_packet_t pkt;
    EXPECT_EQ(sizeof(pkt.data), (size_t)DCC_PACKET_MAX_BYTES);
}

TEST(DccTypes, address_type_enum_values) {
    EXPECT_EQ(DCC_ADDRESS_SHORT, 0);
    EXPECT_EQ(DCC_ADDRESS_LONG, 1);
    EXPECT_EQ(DCC_ADDRESS_BROADCAST, 2);
    EXPECT_EQ(DCC_ADDRESS_IDLE, 3);
    EXPECT_EQ(DCC_ADDRESS_ACCESSORY, 4);
    EXPECT_EQ(DCC_ADDRESS_ACCESSORY_EXTENDED, 5);
}

TEST(DccTypes, speed_mode_enum_values) {
    EXPECT_EQ(DCC_SPEED_MODE_14, 0);
    EXPECT_EQ(DCC_SPEED_MODE_28, 1);
    EXPECT_EQ(DCC_SPEED_MODE_128, 2);
}

// ============================================================================
// dcc_defines.h protocol constant tests
// ============================================================================

TEST(DccDefines, bit_timing_constants) {
    EXPECT_EQ(DCC_ONE_BIT_HALF_PERIOD_US, 58);
    EXPECT_EQ(DCC_ZERO_BIT_HALF_PERIOD_US, 100);
    EXPECT_EQ(DCC_ZERO_BIT_MAX_TOTAL_DURATION_US, 12000);
}

TEST(DccDefines, preamble_constants) {
    EXPECT_EQ(DCC_PREAMBLE_BITS_OPS, 14);
    EXPECT_EQ(DCC_PREAMBLE_BITS_SERVICE, 20);
    EXPECT_EQ(DCC_PREAMBLE_BITS_DECODER_MIN, 10);
}

TEST(DccDefines, address_range_constants) {
    EXPECT_EQ(DCC_ADDRESS_BROADCAST_VALUE, 0);
    EXPECT_EQ(DCC_ADDRESS_SHORT_MAX, 127);
    EXPECT_EQ(DCC_ADDRESS_LONG_MIN, 128);
    EXPECT_EQ(DCC_ADDRESS_LONG_MAX, 10239);
    EXPECT_EQ(DCC_ADDRESS_IDLE_VALUE, 255);
}

TEST(DccDefines, idle_packet_constants) {
    EXPECT_EQ(DCC_IDLE_ADDR_BYTE, 0xFF);
    EXPECT_EQ(DCC_IDLE_DATA_BYTE, 0x00);
    EXPECT_EQ(DCC_IDLE_XOR_BYTE, 0xFF);
    EXPECT_EQ(DCC_IDLE_ADDR_BYTE ^ DCC_IDLE_DATA_BYTE, DCC_IDLE_XOR_BYTE);
}

TEST(DccDefines, cv29_bit_masks) {
    EXPECT_EQ(DCC_CV29_DIRECTION_BIT, 0x01);
    EXPECT_EQ(DCC_CV29_SPEED_STEPS_BIT, 0x02);
    EXPECT_EQ(DCC_CV29_ANALOG_ENABLE_BIT, 0x04);
    EXPECT_EQ(DCC_CV29_RAILCOM_ENABLE_BIT, 0x08);
    EXPECT_EQ(DCC_CV29_SPEED_TABLE_BIT, 0x10);
    EXPECT_EQ(DCC_CV29_EXTENDED_ADDRESS_BIT, 0x20);
}

TEST(DccDefines, cv28_bit_masks) {
    EXPECT_EQ(DCC_CV28_CH1_ENABLE_BIT, 0x01);
    EXPECT_EQ(DCC_CV28_CH2_ENABLE_BIT, 0x02);
    EXPECT_EQ(DCC_CV28_UNSOLICITED_BIT, 0x04);
}

TEST(DccDefines, railcom_timing_constants) {
    /* 5-state cutout per-state default durations (microseconds). */
    EXPECT_EQ(DCC_RAILCOM_CUTOUT_START_DELAY_US, 26);
    EXPECT_EQ(DCC_RAILCOM_UART_RX_DELAY_US, 54);
    EXPECT_EQ(DCC_RAILCOM_CH1_WINDOW_US, 97);
    EXPECT_EQ(DCC_RAILCOM_CH1_CH2_GAP_US, 16);
    EXPECT_EQ(DCC_RAILCOM_CH2_WINDOW_US, 261);
}

TEST(DccDefines, user_config_constants_are_set) {
    EXPECT_EQ(USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT, 16);
    EXPECT_EQ(USER_DEFINED_DCC_MAX_LOCOS, 8);
    EXPECT_EQ(USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH, 4);
    EXPECT_EQ(USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS, 29);
}
