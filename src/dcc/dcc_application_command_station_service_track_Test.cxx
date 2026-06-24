/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for the command station service track application façade.
 *
 * The façade owns power control and service-mode entry/exit, and delegates the
 * programming surface to the high-level task layer through its interface struct.
 * These tests verify the power/session sequencing and that each programming
 * function forwards its arguments to the wired task interface member, returns the
 * member's result, and null-guards a missing interface / member.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_command_station_service_track.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

// ============================================================================
// Power / session mock state (with an ordering log)
// ============================================================================

static int  call_log[16];
static int  call_log_len;

#define LOG_TIMER_START   1
#define LOG_TIMER_STOP    2
#define LOG_POWER_ON      3
#define LOG_POWER_OFF     4
#define LOG_ENCODER_START 5
#define LOG_ENCODER_STOP  6
#define LOG_ENTER         7
#define LOG_EXIT          8

static void log_push(int e) { if (call_log_len < 16) call_log[call_log_len++] = e; }

static void mock_timer_start(uint16_t period) { (void)period; log_push(LOG_TIMER_START); }
static void mock_timer_stop(void) { log_push(LOG_TIMER_STOP); }
static void mock_power_set(bool enabled) { log_push(enabled ? LOG_POWER_ON : LOG_POWER_OFF); }
static void mock_encoder_start(void) { log_push(LOG_ENCODER_START); }
static void mock_encoder_stop(void) { log_push(LOG_ENCODER_STOP); }

static bool enter_return;
static bool is_active_return;
static bool mock_enter_service_mode(void) { log_push(LOG_ENTER); return enter_return; }
static void mock_exit_service_mode(void) { log_push(LOG_EXIT); }
static bool mock_is_service_mode_active(void) { return is_active_return; }

// ============================================================================
// Task-op mock state — captured args + per-op call count + return flag
// ============================================================================

static uint16_t last_cv;
static uint8_t  last_value;
static uint8_t  last_bit;
static bool     last_bit_value;
static uint8_t  last_address;
static dcc_decoder_type_enum last_decoder_type;
static dcc_service_mode_task_on_complete_callback_t last_on_complete;
static dcc_service_mode_task_on_progress_callback_t last_on_progress;
static dcc_service_mode_task_on_detect_callback_t   last_on_detect;

static uint32_t op_count;
static bool     op_return;

static void dummy_on_complete(dcc_service_mode_result_t r, uint8_t v) { (void)r; (void)v; }
static void dummy_on_progress(dcc_task_phase_enum p, uint8_t c, uint8_t e) { (void)p; (void)c; (void)e; }
static void dummy_on_detect(dcc_service_mode_result_t r, uint8_t modes) { (void)r; (void)modes; }

static bool mock_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_value = value; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_bit = bit; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_bit = bit; last_bit_value = bit_value; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_reg_read_cv(uint16_t cv, dcc_decoder_type_enum dt, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_decoder_type = dt; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_reg_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum dt, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_value = value; last_decoder_type = dt; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_reg_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum dt, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_bit = bit; last_decoder_type = dt; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_reg_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum dt, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_cv = cv; last_bit = bit; last_bit_value = bit_value; last_decoder_type = dt; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_reg_factory_reset(dcc_service_mode_task_on_complete_callback_t oc) {
    last_on_complete = oc; op_count++; return op_return;
}
static bool mock_addr_read(dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_addr_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_address = address; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_addr_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_bit = bit; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_addr_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t oc, dcc_service_mode_task_on_progress_callback_t op) {
    last_bit = bit; last_bit_value = bit_value; last_on_complete = oc; last_on_progress = op; op_count++; return op_return;
}
static bool mock_detect_mode(dcc_service_mode_task_on_detect_callback_t od) {
    last_on_detect = od; op_count++; return op_return;
}

static interface_dcc_application_command_station_service_track_t _iface;

static void reset_state(void) {
    call_log_len = 0;
    enter_return = true;
    is_active_return = false;
    last_cv = 0; last_value = 0; last_bit = 0; last_bit_value = false; last_address = 0;
    last_decoder_type = DCC_DECODER_TYPE_MOBILE;
    last_on_complete = NULL; last_on_progress = NULL; last_on_detect = NULL;
    op_count = 0; op_return = true;
}

static interface_dcc_application_command_station_service_track_t make_interface(void) {
    interface_dcc_application_command_station_service_track_t i;
    memset(&i, 0, sizeof(i));

    i.timer_start = mock_timer_start;
    i.timer_stop = mock_timer_stop;
    i.track_power_set = mock_power_set;
    i.encoder_start = mock_encoder_start;
    i.encoder_stop = mock_encoder_stop;
    i.enter_service_mode = mock_enter_service_mode;
    i.exit_service_mode = mock_exit_service_mode;
    i.is_service_mode_active = mock_is_service_mode_active;

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    i.direct_read_cv = mock_read_cv;
    i.direct_write_cv = mock_write_cv;
    i.direct_read_bit = mock_read_bit;
    i.direct_write_bit = mock_write_bit;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    i.paged_read_cv = mock_read_cv;
    i.paged_write_cv = mock_write_cv;
    i.paged_read_bit = mock_read_bit;
    i.paged_write_bit = mock_write_bit;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    i.register_read_cv = mock_reg_read_cv;
    i.register_write_cv = mock_reg_write_cv;
    i.register_read_bit = mock_reg_read_bit;
    i.register_write_bit = mock_reg_write_bit;
    i.register_factory_reset = mock_reg_factory_reset;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    i.address_read = mock_addr_read;
    i.address_write = mock_addr_write;
    i.address_read_bit = mock_addr_read_bit;
    i.address_write_bit = mock_addr_write_bit;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    i.detect_mode = mock_detect_mode;
#endif

    return i;
}

static void setup(void) {
    reset_state();
    _iface = make_interface();
    DccApplicationCommandStationServiceTrack_initialize(&_iface);
}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccApplicationCommandStationServiceTrack, initialize_does_not_crash) {
    reset_state();
    _iface = make_interface();
    DccApplicationCommandStationServiceTrack_initialize(&_iface);
}

// ============================================================================
// Power / session
// ============================================================================

TEST(DccApplicationCommandStationServiceTrack, power_on_sequence) {
    setup();
    DccApplicationCommandStationServiceTrack_power_on();

    ASSERT_EQ(call_log_len, 3);
    EXPECT_EQ(call_log[0], LOG_POWER_ON);
    EXPECT_EQ(call_log[1], LOG_TIMER_START);
    EXPECT_EQ(call_log[2], LOG_ENCODER_START);
}

TEST(DccApplicationCommandStationServiceTrack, power_off_sequence) {
    setup();
    DccApplicationCommandStationServiceTrack_power_off();

    ASSERT_EQ(call_log_len, 3);
    EXPECT_EQ(call_log[0], LOG_ENCODER_STOP);
    EXPECT_EQ(call_log[1], LOG_TIMER_STOP);
    EXPECT_EQ(call_log[2], LOG_POWER_OFF);
}

TEST(DccApplicationCommandStationServiceTrack, enter_service_mode_powers_and_enters) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_enter_service_mode());

    EXPECT_EQ(call_log[0], LOG_POWER_ON);
    EXPECT_EQ(call_log[1], LOG_TIMER_START);
    EXPECT_EQ(call_log[2], LOG_ENCODER_START);
    EXPECT_EQ(call_log[3], LOG_ENTER);
}

TEST(DccApplicationCommandStationServiceTrack, enter_service_mode_passes_through_false) {
    setup();
    enter_return = false;
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_enter_service_mode());
}

TEST(DccApplicationCommandStationServiceTrack, exit_service_mode_exits_then_powers_down) {
    setup();
    DccApplicationCommandStationServiceTrack_exit_service_mode();

    ASSERT_EQ(call_log_len, 4);
    EXPECT_EQ(call_log[0], LOG_EXIT);
    EXPECT_EQ(call_log[1], LOG_ENCODER_STOP);
    EXPECT_EQ(call_log[2], LOG_TIMER_STOP);
    EXPECT_EQ(call_log[3], LOG_POWER_OFF);
}

TEST(DccApplicationCommandStationServiceTrack, is_service_mode_active_delegates) {
    setup();
    is_active_return = true;
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_is_service_mode_active());
    is_active_return = false;
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_is_service_mode_active());
}

TEST(DccApplicationCommandStationServiceTrack, power_on_null_interface_no_crash) {
    DccApplicationCommandStationServiceTrack_initialize(NULL);
    DccApplicationCommandStationServiceTrack_power_on();
}

// ============================================================================
// Direct task delegation
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

TEST(DccApplicationCommandStationServiceTrack, direct_read_cv_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_read_cv(29, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(op_count, (uint32_t)1);
    EXPECT_EQ(last_cv, (uint16_t)29);
    EXPECT_EQ(last_on_complete, dummy_on_complete);
    EXPECT_EQ(last_on_progress, dummy_on_progress);
}

TEST(DccApplicationCommandStationServiceTrack, direct_write_cv_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_write_cv(29, 0xA5, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_cv, (uint16_t)29);
    EXPECT_EQ(last_value, (uint8_t)0xA5);
}

TEST(DccApplicationCommandStationServiceTrack, direct_read_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_read_bit(29, 5, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_cv, (uint16_t)29);
    EXPECT_EQ(last_bit, (uint8_t)5);
}

TEST(DccApplicationCommandStationServiceTrack, direct_write_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_direct_write_bit(29, 5, true, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)5);
    EXPECT_TRUE(last_bit_value);
}

TEST(DccApplicationCommandStationServiceTrack, direct_read_cv_passes_through_return) {
    setup();
    op_return = false;
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_direct_read_cv(1, dummy_on_complete, dummy_on_progress));
}

TEST(DccApplicationCommandStationServiceTrack, direct_read_cv_null_member_returns_false) {
    reset_state();
    _iface = make_interface();
    _iface.direct_read_cv = NULL;
    DccApplicationCommandStationServiceTrack_initialize(&_iface);
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_direct_read_cv(1, dummy_on_complete, dummy_on_progress));
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

// ============================================================================
// Paged task delegation
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

TEST(DccApplicationCommandStationServiceTrack, paged_read_cv_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_paged_read_cv(7, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_cv, (uint16_t)7);
}

TEST(DccApplicationCommandStationServiceTrack, paged_write_cv_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_paged_write_cv(7, 0x33, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_value, (uint8_t)0x33);
}

TEST(DccApplicationCommandStationServiceTrack, paged_read_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_paged_read_bit(7, 2, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)2);
}

TEST(DccApplicationCommandStationServiceTrack, paged_write_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_paged_write_bit(7, 2, false, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)2);
    EXPECT_FALSE(last_bit_value);
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

// ============================================================================
// Register task delegation (decoder_type per call)
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

TEST(DccApplicationCommandStationServiceTrack, register_read_cv_delegates_with_decoder_type) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_register_read_cv(29, DCC_DECODER_TYPE_MOBILE, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_cv, (uint16_t)29);
    EXPECT_EQ(last_decoder_type, DCC_DECODER_TYPE_MOBILE);
}

TEST(DccApplicationCommandStationServiceTrack, register_write_cv_delegates_accessory) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_register_write_cv(513, 0x10, DCC_DECODER_TYPE_ACCESSORY, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_value, (uint8_t)0x10);
    EXPECT_EQ(last_decoder_type, DCC_DECODER_TYPE_ACCESSORY);
}

TEST(DccApplicationCommandStationServiceTrack, register_read_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_register_read_bit(29, 3, DCC_DECODER_TYPE_MOBILE, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)3);
}

TEST(DccApplicationCommandStationServiceTrack, register_write_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_register_write_bit(29, 3, true, DCC_DECODER_TYPE_MOBILE, dummy_on_complete, dummy_on_progress));
    EXPECT_TRUE(last_bit_value);
}

TEST(DccApplicationCommandStationServiceTrack, register_factory_reset_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_register_factory_reset(dummy_on_complete));
    EXPECT_EQ(op_count, (uint32_t)1);
    EXPECT_EQ(last_on_complete, dummy_on_complete);
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

// ============================================================================
// Address task delegation
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

TEST(DccApplicationCommandStationServiceTrack, address_read_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_address_read(dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(op_count, (uint32_t)1);
}

TEST(DccApplicationCommandStationServiceTrack, address_write_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_address_write(42, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_address, (uint8_t)42);
}

TEST(DccApplicationCommandStationServiceTrack, address_read_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_address_read_bit(4, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)4);
}

TEST(DccApplicationCommandStationServiceTrack, address_write_bit_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_address_write_bit(4, true, dummy_on_complete, dummy_on_progress));
    EXPECT_EQ(last_bit, (uint8_t)4);
    EXPECT_TRUE(last_bit_value);
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

// ============================================================================
// Detect task delegation
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

TEST(DccApplicationCommandStationServiceTrack, detect_mode_delegates) {
    setup();
    EXPECT_TRUE(DccApplicationCommandStationServiceTrack_detect_mode(dummy_on_detect));
    EXPECT_EQ(op_count, (uint32_t)1);
    EXPECT_EQ(last_on_detect, dummy_on_detect);
}

TEST(DccApplicationCommandStationServiceTrack, detect_mode_null_member_returns_false) {
    reset_state();
    _iface = make_interface();
    _iface.detect_mode = NULL;
    DccApplicationCommandStationServiceTrack_initialize(&_iface);
    EXPECT_FALSE(DccApplicationCommandStationServiceTrack_detect_mode(dummy_on_detect));
}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#endif /* DCC_COMPILE_COMMAND_STATION */
