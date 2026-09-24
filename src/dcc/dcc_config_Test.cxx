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

// Dedicated to the address race-condition regression test below (PR #1 review,
// commit 4). mock_uart_read above always returns false and is shared by every
// other RailCom test in this file; this one serves a settable buffer instead,
// wired in only by that one test (overridden on its own dcc_railcom_hw_t after
// make_railcom_hw()), so it never changes what any other test sees.
static uint8_t railcom_race_uart_buffer[16];
static uint8_t railcom_race_uart_count = 0;
static uint8_t railcom_race_uart_index = 0;
static bool mock_uart_read_from_buffer(uint8_t *byte) {
    if (railcom_race_uart_index >= railcom_race_uart_count) return false;
    *byte = railcom_race_uart_buffer[railcom_race_uart_index];
    railcom_race_uart_index++;
    return true;
}

static uint16_t railcom_race_result_address = 0xFFFF;
static uint32_t railcom_race_result_count = 0;
static void mock_railcom_datagram_result(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram) {
    (void)channel; (void)datagram;
    railcom_race_result_address = address;
    railcom_race_result_count++;
}

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

TEST(DccConfig, main_track_power_independent_of_service) {
    dcc_config_t cfg = make_test_config();
    cfg.main_track.track_power_set = mock_main_power_set_tracking;
    DccConfig_initialize(&cfg);

    /* Main-track power is driven by the main-track power callback. */
    main_power_set_called = false;
    DccApplicationCommandStationMainTrack_power_on();
    EXPECT_TRUE(main_power_set_called);

    /* Service-track operations do not touch main-track power. The service track
     * has no separate power callback (the encoder drives it). */
    main_power_set_called = false;
    DccApplicationCommandStationServiceTrack_power_on();
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    EXPECT_FALSE(main_power_set_called);
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

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

TEST(DccConfig, direct_read_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_read_cv(17, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_write_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_cv(17, 0x42, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_write_bit) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_direct_write_bit(17, 3, true, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

TEST(DccConfig, paged_read_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_paged_read_cv(29, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, paged_write_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_paged_write_cv(29, 0x50, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

TEST(DccConfig, register_read_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_register_read_cv(1, DCC_DECODER_TYPE_MOBILE, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, register_write_cv) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_register_write_cv(1, 0x35, DCC_DECODER_TYPE_MOBILE, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

TEST(DccConfig, address_read) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_address_read(NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, address_write) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_address_write(42, NULL, NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

TEST(DccConfig, detect_mode) {
    dcc_config_t cfg = make_test_config();
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    DccApplicationCommandStationServiceTrack_detect_mode(NULL);
    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

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

TEST(DccConfig, railcom_cutout_address_survives_a_packet_dispatched_during_the_cutout) {
    /* Jim Kueneman's PR #1 review, commit 4 (2026-09-23): the address a cutout's
     * decoded bytes get tagged with used to be recorded in on_packet_sent, which
     * fires for a NEW packet as soon as the PREVIOUS one's transmission ends --
     * the same instant this cutout begins. A main loop fast enough to dispatch
     * that next packet before the cutout completes overwrote the recorded
     * address before the cutout-complete wrapper read it, mistagging the reply.
     * Reproduces the race directly: dispatch address 5, let its cutout begin,
     * dispatch address 7 into the now-idle encoder before completing that
     * cutout, then finish the cutout and confirm the decoded datagram is
     * tagged with 5, not 7. */

    railcom_race_uart_count = 0;
    railcom_race_uart_index = 0;
    railcom_race_result_address = 0xFFFF;
    railcom_race_result_count = 0;

    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    rc.uart_read = mock_uart_read_from_buffer;
    rc.on_railcom_datagram_result = mock_railcom_datagram_result;
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    /* Packet A: short address 5 -- its cutout is the one under test. */
    dcc_packet_t pkt_a = make_idle_packet();
    pkt_a.data[0] = 5;
    DccApplicationCommandStationMainTrack_send_packet(&pkt_a, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();
    pump_main_track_until_idle(200);   /* transmits A fully; end bit arms the cutout */

    /* Packet B: short address 7, dispatched into the encoder while A's cutout
     * is still open -- exactly the race window Jim described. repeat_count 1
     * (make_idle_packet()) deactivates A's slot after one send, so this is
     * the next one-shot the scheduler picks up, not a re-send of A. */
    dcc_packet_t pkt_b = make_idle_packet();
    pkt_b.data[0] = 7;
    DccApplicationCommandStationMainTrack_send_packet(&pkt_b, 7, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();

    /* A minimal valid Channel 1 datagram -- same encoding
     * dcc_railcom_command_station_Test.cxx's ch1_valid_2_bytes uses -- so the
     * decode path actually produces a result to check the address on. */
    railcom_race_uart_buffer[0] = 0xAC;
    railcom_race_uart_buffer[1] = 0xAA;
    railcom_race_uart_count = 2;
    railcom_race_uart_index = 0;

    DccConfig_railcom_oneshot_timer_isr();  /* DELAY -> SETTLING */
    DccConfig_railcom_oneshot_timer_isr();  /* SETTLING -> CH1 */
    DccConfig_railcom_oneshot_timer_isr();  /* CH1 -> GAP */
    DccConfig_railcom_oneshot_timer_isr();  /* GAP -> CH2 */
    DccConfig_railcom_oneshot_timer_isr();  /* CH2 -> IDLE, begin_cutout(address) fires */

    DccConfig_run();   /* on_railcom_datagram_result fires from here, not the ISR */

    EXPECT_EQ(railcom_race_result_count, (uint32_t)1);
    EXPECT_EQ(railcom_race_result_address, (uint16_t)5);

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
    DccApplicationCommandStationServiceTrack_direct_write_cv(1, 0x55, NULL, NULL);

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
    DccApplicationCommandStationServiceTrack_direct_write_cv(1, 0x55, NULL, NULL);

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

// @compliance DCC-S9.1-CS-001, DCC-S9.1-CS-002
TEST(DccDefines, bit_timing_constants) {
    EXPECT_EQ(DCC_ONE_BIT_HALF_PERIOD_US, 58);
    EXPECT_EQ(DCC_ZERO_BIT_HALF_PERIOD_US, 100);
    EXPECT_EQ(DCC_ZERO_BIT_MAX_TOTAL_DURATION_US, 12000);
}

// @compliance DCC-S9.1-CS-003, DCC-S9.1-DEC-002
TEST(DccDefines, preamble_constants) {
    EXPECT_EQ(DCC_PREAMBLE_BITS_OPS, USER_DEFINED_DCC_PREAMBLE_BITS_OPS); /* sourced from user config */
    EXPECT_GE(DCC_PREAMBLE_BITS_OPS, 16);   /* RailCom build floor (S-9.3.2 sec 2.4): >= 16 */
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

// ============================================================================
// Group 10: RailCom cutout timing setter, cancel / is_active, all-five override
// ============================================================================

#if defined(DCC_COMPILE_COMMAND_STATION) && defined(DCC_COMPILE_RAILCOM)

/* Bring a RailCom-configured main track to the end bit of one packet so the
 * cutout state machine has just loaded its DELAY period. */
static void arm_cutout(dcc_config_t *cfg, dcc_railcom_hw_t *rc) {
    cfg->main_track.railcom = rc;
    cfg->railcom_timer_start = mock_railcom_timer_start;
    cfg->railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(cfg);
    DccApplicationCommandStationMainTrack_power_on();

    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();

    last_railcom_timer_period = 0;
    pump_main_track_until_idle(200);
}

static void expect_cutout_periods(uint16_t d, uint16_t s, uint16_t c1, uint16_t g, uint16_t c2) {
    EXPECT_EQ(last_railcom_timer_period, d);
    DccConfig_railcom_oneshot_timer_isr();
    EXPECT_EQ(last_railcom_timer_period, s);
    DccConfig_railcom_oneshot_timer_isr();
    EXPECT_EQ(last_railcom_timer_period, c1);
    DccConfig_railcom_oneshot_timer_isr();
    EXPECT_EQ(last_railcom_timer_period, g);
    DccConfig_railcom_oneshot_timer_isr();
    EXPECT_EQ(last_railcom_timer_period, c2);
    DccConfig_railcom_oneshot_timer_isr();   /* CH2 -> IDLE */
}

TEST(DccConfig, railcom_cutout_all_five_config_fields_override_defaults) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.railcom_cutout_start_delay_us = 11;
    cfg.railcom_uart_rx_delay_us = 22;
    cfg.railcom_ch1_window_us = 33;
    cfg.railcom_ch1_ch2_gap_us = 44;
    cfg.railcom_ch2_window_us = 55;
    arm_cutout(&cfg, &rc);

    expect_cutout_periods(11, 22, 33, 44, 55);

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, set_railcom_cutout_timing_nonzero_applies_to_next_cutout) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);

    DccConfig_set_railcom_cutout_timing(12, 23, 34, 45, 56);

    DccApplicationCommandStationMainTrack_power_on();
    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();
    last_railcom_timer_period = 0;
    pump_main_track_until_idle(200);

    expect_cutout_periods(12, 23, 34, 45, 56);

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, set_railcom_cutout_timing_zero_restores_spec_defaults) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.railcom_cutout_start_delay_us = 11;
    cfg.railcom_uart_rx_delay_us = 22;
    cfg.railcom_ch1_window_us = 33;
    cfg.railcom_ch1_ch2_gap_us = 44;
    cfg.railcom_ch2_window_us = 55;
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);

    /* 0 in every field selects that field's spec default. */
    DccConfig_set_railcom_cutout_timing(0, 0, 0, 0, 0);

    DccApplicationCommandStationMainTrack_power_on();
    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();
    last_railcom_timer_period = 0;
    pump_main_track_until_idle(200);

    expect_cutout_periods(DCC_RAILCOM_CUTOUT_START_DELAY_US, DCC_RAILCOM_UART_RX_DELAY_US,
                          DCC_RAILCOM_CH1_WINDOW_US, DCC_RAILCOM_CH1_CH2_GAP_US,
                          DCC_RAILCOM_CH2_WINDOW_US);

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, railcom_cutout_is_active_tracks_state_and_cancel_clears_it) {
    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);

    EXPECT_FALSE(DccConfig_railcom_cutout_is_active());
    DccConfig_cancel_railcom_cutout();               /* no-op when idle */
    EXPECT_FALSE(DccConfig_railcom_cutout_is_active());

    DccApplicationCommandStationMainTrack_power_on();
    dcc_packet_t pkt = make_idle_packet();
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();
    pump_main_track_until_idle(200);                 /* end bit -> DELAY */
    EXPECT_TRUE(DccConfig_railcom_cutout_is_active());

    DccConfig_railcom_oneshot_timer_isr();           /* DELAY -> SETTLING (H-bridge off) */
    EXPECT_TRUE(DccConfig_railcom_cutout_is_active());

    DccConfig_cancel_railcom_cutout();
    EXPECT_FALSE(DccConfig_railcom_cutout_is_active());

    DccApplicationCommandStationMainTrack_power_off();
}

TEST(DccConfig, railcom_cutout_tags_long_address_packets) {
    railcom_race_uart_count = 0;
    railcom_race_uart_index = 0;
    railcom_race_result_address = 0xFFFF;
    railcom_race_result_count = 0;

    dcc_config_t cfg = make_test_config();
    dcc_railcom_hw_t rc = make_railcom_hw();
    rc.uart_read = mock_uart_read_from_buffer;
    rc.on_railcom_datagram_result = mock_railcom_datagram_result;
    cfg.main_track.railcom = &rc;
    cfg.railcom_timer_start = mock_railcom_timer_start;
    cfg.railcom_timer_stop = mock_railcom_timer_stop;
    DccConfig_initialize(&cfg);
    DccApplicationCommandStationMainTrack_power_on();

    /* 128-step speed to long address 1000 (0xC3 0xE8). */
    dcc_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.data[0] = 0xC3; pkt.data[1] = 0xE8; pkt.data[2] = 0x3F; pkt.data[3] = 0x90;
    pkt.data[4] = pkt.data[0] ^ pkt.data[1] ^ pkt.data[2] ^ pkt.data[3];
    pkt.byte_count = 5;
    pkt.preamble_bits = DCC_PREAMBLE_BITS_OPS;
    pkt.repeat_count = 1;
    DccApplicationCommandStationMainTrack_send_packet(&pkt, 1000, DCC_TAG_SPEED, DCC_PRIORITY_SPEED);
    DccConfig_run();
    pump_main_track_until_idle(250);

    railcom_race_uart_buffer[0] = 0xAC;
    railcom_race_uart_buffer[1] = 0xAA;
    railcom_race_uart_count = 2;
    railcom_race_uart_index = 0;

    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_railcom_oneshot_timer_isr();
    DccConfig_run();

    EXPECT_EQ(railcom_race_result_count, (uint32_t)1);
    EXPECT_EQ(railcom_race_result_address, (uint16_t)1000);

    DccApplicationCommandStationMainTrack_power_off();
}

#endif /* DCC_COMPILE_COMMAND_STATION && DCC_COMPILE_RAILCOM */

// ============================================================================
// Group 11: a service-mode task driven to completion through the wiring
// ============================================================================

#if defined(DCC_COMPILE_COMMAND_STATION) && defined(DCC_COMPILE_SERVICE_MODE_TASK_DIRECT)

static uint32_t task_complete_count;
static dcc_service_mode_result_t task_complete_result;
static void mock_task_on_complete(dcc_service_mode_result_t result, uint8_t value) {
    (void)value;
    task_complete_result = result;
    task_complete_count++;
}
static uint16_t mock_current_sense_silent(void) { return 0; }   /* never crosses the ACK threshold */

TEST(DccConfig, direct_write_cv_task_completes_through_primitive_dispatcher) {
    dcc_config_t cfg = make_test_config();
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    cfg.service_track.current_sense_read = mock_current_sense_silent;
    DccConfig_initialize(&cfg);

    task_complete_count = 0;
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_write_cv(8, 0x55, mock_task_on_complete, NULL));

    /* Write byte, then verify byte: two full S-9.2.3 sequences, each with an
     * ACK window that stays silent, so the verify step reports VERIFY_FAIL. */
    pump_service_mode_cycle(4000);

    EXPECT_EQ(task_complete_count, (uint32_t)1);
    EXPECT_EQ(task_complete_result, DCC_SERVICE_MODE_VERIFY_FAIL);

    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

TEST(DccConfig, direct_read_cv_task_reports_no_ack_through_primitive_dispatcher) {
    dcc_config_t cfg = make_test_config();
    cfg.service_track.pin_toggle = mock_svc_pin_toggle;
    cfg.service_track.current_sense_read = mock_current_sense_silent;
    DccConfig_initialize(&cfg);

    task_complete_count = 0;
    DccApplicationCommandStationServiceTrack_enter_service_mode();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_read_cv(8, mock_task_on_complete, NULL));

    /* Eight bit-verifies then the confirming byte-verify, nine full S-9.2.3
     * sequences with a silent ACK window each: no decoder on the track. */
    pump_service_mode_cycle(12000);

    EXPECT_EQ(task_complete_count, (uint32_t)1);
    EXPECT_EQ(task_complete_result, DCC_SERVICE_MODE_NO_ACK);

    DccApplicationCommandStationServiceTrack_exit_service_mode();
}

#endif /* DCC_COMPILE_COMMAND_STATION && DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

// ============================================================================
// Group 12: decoder edge feed -> packet dispatch through the wiring
// ============================================================================

#ifdef DCC_COMPILE_DECODER

static uint32_t cfg_speed_count;
static uint16_t cfg_speed_address;
static uint8_t  cfg_speed_value;
static void mock_cfg_on_speed(uint16_t address, uint8_t speed, bool direction, dcc_speed_mode_enum mode) {
    (void)direction; (void)mode;
    cfg_speed_address = address;
    cfg_speed_value = speed;
    cfg_speed_count++;
}

static uint32_t edge_ts;
static void feed_half(uint32_t us) { edge_ts += us; DccConfig_decoder_edge_isr(edge_ts); }
static void feed_bit(bool one) {
    uint32_t half = one ? DCC_ONE_BIT_HALF_PERIOD_US : DCC_ZERO_BIT_HALF_PERIOD_US;
    feed_half(half);
    feed_half(half);
}
static void feed_byte(uint8_t b) { for (int i = 7; i >= 0; i--) feed_bit((b >> i) & 1); }
static void feed_packet(const uint8_t *data, uint8_t count) {
    edge_ts = 1000;
    DccConfig_decoder_edge_isr(edge_ts);          /* first edge: timestamp only */
    for (int i = 0; i < 16; i++) feed_bit(true);  /* preamble */
    for (uint8_t i = 0; i < count; i++) {
        feed_bit(false);                          /* start / separator */
        feed_byte(data[i]);
    }
    feed_bit(true);                               /* end bit */
}

TEST(DccConfig, decoder_edges_dispatch_a_packet_on_run) {
    dcc_config_t cfg = make_test_config();
    cfg.on_speed_command = mock_cfg_on_speed;
    DccConfig_initialize(&cfg);

    cfg_speed_count = 0;
    /* Broadcast 128-step speed: accepted whatever address the CV store yields. */
    uint8_t data[] = {0x00, 0x3F, 0x90, 0x00};
    data[3] = data[0] ^ data[1] ^ data[2];
    feed_packet(data, 4);

    EXPECT_EQ(cfg_speed_count, (uint32_t)0);      /* queued at the end bit, not dispatched */
    DccConfig_run();
    EXPECT_EQ(cfg_speed_count, (uint32_t)1);
    EXPECT_EQ(cfg_speed_address, (uint16_t)0);
    EXPECT_EQ(cfg_speed_value, (uint8_t)0x10);
}

#endif /* DCC_COMPILE_DECODER */

// ============================================================================
// Group 13: ACK pulse -- library owns the 6 ms stop (S-9.2.3 sec 3)
// ============================================================================

#ifdef DCC_COMPILE_DECODER

static uint32_t mock_clock_usec;
static uint32_t mock_clock(void) { return mock_clock_usec; }

static uint32_t ack_start_count;
static uint32_t ack_stop_count;
static void mock_start_ack_pulse(void) { ack_start_count++; }
static void mock_stop_ack_pulse(void) { ack_stop_count++; }

/* Arm service mode with three broadcast resets, then verify CV5 == 0, which
 * matches the all-zero mock CV store and so fires the ACK. */
static void feed_matching_service_mode_verify(void) {
    uint8_t reset[] = {0x00, 0x00, 0x00};
    for (int i = 0; i < 3; i++) {
        feed_packet(reset, 3);
        DccConfig_run();
    }
    uint8_t verify[] = {0x74, 0x04, 0x00, 0x00};
    verify[3] = verify[0] ^ verify[1] ^ verify[2];
    feed_packet(verify, 4);
    DccConfig_run();
}

TEST(DccConfig, ack_pulse_is_stopped_by_run_after_6ms) {
    dcc_config_t cfg = make_test_config();
    cfg.get_timestamp_usec = mock_clock;
    cfg.start_ack_pulse = mock_start_ack_pulse;
    cfg.stop_ack_pulse = mock_stop_ack_pulse;
    DccConfig_initialize(&cfg);

    mock_clock_usec = 1000;
    ack_start_count = 0;
    ack_stop_count = 0;

    feed_matching_service_mode_verify();
    EXPECT_EQ(ack_start_count, (uint32_t)1);
    EXPECT_EQ(ack_stop_count, (uint32_t)0);

    mock_clock_usec += DCC_ACK_PULSE_DURATION_US - 1;
    DccConfig_run();
    EXPECT_EQ(ack_stop_count, (uint32_t)0);       /* 5.999 ms: still on */

    mock_clock_usec += 1;
    DccConfig_run();
    EXPECT_EQ(ack_stop_count, (uint32_t)1);       /* 6 ms: off */

    DccConfig_run();
    EXPECT_EQ(ack_stop_count, (uint32_t)1);       /* one stop per pulse */
}

TEST(DccConfig, ack_pulse_expiry_with_null_stop_hook_does_not_crash) {
    dcc_config_t cfg = make_test_config();
    cfg.get_timestamp_usec = mock_clock;
    cfg.start_ack_pulse = mock_start_ack_pulse;
    /* stop_ack_pulse left NULL */
    DccConfig_initialize(&cfg);

    mock_clock_usec = 1000;
    ack_start_count = 0;

    feed_matching_service_mode_verify();
    EXPECT_EQ(ack_start_count, (uint32_t)1);

    mock_clock_usec += DCC_ACK_PULSE_DURATION_US;
    DccConfig_run();

    /* Timer disarmed: a second matching verify starts a fresh pulse. */
    feed_matching_service_mode_verify();
    EXPECT_EQ(ack_start_count, (uint32_t)2);
}

TEST(DccConfig, no_ack_hardware_never_arms_the_pulse_timer) {
    dcc_config_t cfg = make_test_config();
    cfg.get_timestamp_usec = mock_clock;
    /* start_ack_pulse and stop_ack_pulse both NULL */
    cfg.stop_ack_pulse = mock_stop_ack_pulse;
    DccConfig_initialize(&cfg);

    mock_clock_usec = 1000;
    ack_stop_count = 0;

    feed_matching_service_mode_verify();
    mock_clock_usec += DCC_ACK_PULSE_DURATION_US;
    DccConfig_run();

    EXPECT_EQ(ack_stop_count, (uint32_t)0);
}

#endif /* DCC_COMPILE_DECODER */
