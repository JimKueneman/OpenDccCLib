/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Application Service Track
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_service_track.h"
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

static uint32_t enter_service_mode_count;
static bool enter_service_mode_return;
static uint32_t exit_service_mode_count;
static uint32_t is_service_mode_active_count;
static bool is_service_mode_active_return;

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
static uint16_t direct_cv_number;
static uint8_t direct_value;
static uint8_t direct_bit_position;
static bool direct_bit_value;
static uint32_t direct_write_byte_count;
static bool direct_write_byte_return;
static uint32_t direct_verify_byte_count;
static bool direct_verify_byte_return;
static uint32_t direct_write_bit_count;
static bool direct_write_bit_return;
static uint32_t direct_verify_bit_count;
static bool direct_verify_bit_return;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
static uint16_t paged_cv_number;
static uint8_t paged_value;
static uint32_t paged_write_count;
static bool paged_write_return;
static uint32_t paged_verify_count;
static bool paged_verify_return;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
static uint8_t register_number;
static uint8_t register_value;
static uint32_t register_write_count;
static bool register_write_return;
static uint32_t register_verify_count;
static bool register_verify_return;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
static uint8_t address_value;
static uint32_t address_write_count;
static bool address_write_return;
static uint32_t address_verify_count;
static bool address_verify_return;
#endif

// ============================================================================
// Mock functions
// ============================================================================

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

static bool mock_enter_service_mode(void) {
    enter_service_mode_count++;
    return enter_service_mode_return;
}

static void mock_exit_service_mode(void) {
    exit_service_mode_count++;
}

static bool mock_is_service_mode_active(void) {
    is_service_mode_active_count++;
    return is_service_mode_active_return;
}

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

static bool mock_direct_write_byte(uint16_t cv, uint8_t val) {
    direct_cv_number = cv;
    direct_value = val;
    direct_write_byte_count++;
    return direct_write_byte_return;
}

static bool mock_direct_verify_byte(uint16_t cv, uint8_t val) {
    direct_cv_number = cv;
    direct_value = val;
    direct_verify_byte_count++;
    return direct_verify_byte_return;
}

static bool mock_direct_write_bit(uint16_t cv, uint8_t bit_position, bool bit_value) {
    direct_cv_number = cv;
    direct_bit_position = bit_position;
    direct_bit_value = bit_value;
    direct_write_bit_count++;
    return direct_write_bit_return;
}

static bool mock_direct_verify_bit(uint16_t cv, uint8_t bit_position, bool bit_value) {
    direct_cv_number = cv;
    direct_bit_position = bit_position;
    direct_bit_value = bit_value;
    direct_verify_bit_count++;
    return direct_verify_bit_return;
}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

static bool mock_paged_write(uint16_t cv, uint8_t val) {
    paged_cv_number = cv;
    paged_value = val;
    paged_write_count++;
    return paged_write_return;
}

static bool mock_paged_verify(uint16_t cv, uint8_t val) {
    paged_cv_number = cv;
    paged_value = val;
    paged_verify_count++;
    return paged_verify_return;
}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

static bool mock_register_write(uint8_t reg, uint8_t val) {
    register_number = reg;
    register_value = val;
    register_write_count++;
    return register_write_return;
}

static bool mock_register_verify(uint8_t reg, uint8_t val) {
    register_number = reg;
    register_value = val;
    register_verify_count++;
    return register_verify_return;
}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

static bool mock_address_write(uint8_t addr) {
    address_value = addr;
    address_write_count++;
    return address_write_return;
}

static bool mock_address_verify(uint8_t addr) {
    address_value = addr;
    address_verify_count++;
    return address_verify_return;
}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

// ============================================================================
// Reset and interface builder
// ============================================================================

static void reset_mocks(void) {
    timer_start_period = 0;
    timer_start_count = 0;
    timer_stop_count = 0;
    power_set_value = false;
    power_set_count = 0;
    encoder_start_count = 0;
    encoder_stop_count = 0;
    enter_service_mode_count = 0;
    enter_service_mode_return = true;
    exit_service_mode_count = 0;
    is_service_mode_active_count = 0;
    is_service_mode_active_return = false;

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
    direct_cv_number = 0;
    direct_value = 0;
    direct_bit_position = 0;
    direct_bit_value = false;
    direct_write_byte_count = 0;
    direct_write_byte_return = true;
    direct_verify_byte_count = 0;
    direct_verify_byte_return = true;
    direct_write_bit_count = 0;
    direct_write_bit_return = true;
    direct_verify_bit_count = 0;
    direct_verify_bit_return = true;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
    paged_cv_number = 0;
    paged_value = 0;
    paged_write_count = 0;
    paged_write_return = true;
    paged_verify_count = 0;
    paged_verify_return = true;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
    register_number = 0;
    register_value = 0;
    register_write_count = 0;
    register_write_return = true;
    register_verify_count = 0;
    register_verify_return = true;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
    address_value = 0;
    address_write_count = 0;
    address_write_return = true;
    address_verify_count = 0;
    address_verify_return = true;
#endif
}

static interface_dcc_application_service_track_t make_interface(void) {
    interface_dcc_application_service_track_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.timer_start = mock_timer_start;
    iface.timer_stop = mock_timer_stop;
    iface.track_power_set = mock_power_set;
    iface.encoder_start = mock_encoder_start;
    iface.encoder_stop = mock_encoder_stop;
    iface.enter_service_mode = mock_enter_service_mode;
    iface.exit_service_mode = mock_exit_service_mode;
    iface.is_service_mode_active = mock_is_service_mode_active;

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
    iface.direct_write_byte = mock_direct_write_byte;
    iface.direct_verify_byte = mock_direct_verify_byte;
    iface.direct_write_bit = mock_direct_write_bit;
    iface.direct_verify_bit = mock_direct_verify_bit;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
    iface.paged_write = mock_paged_write;
    iface.paged_verify = mock_paged_verify;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
    iface.register_write = mock_register_write;
    iface.register_verify = mock_register_verify;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
    iface.address_write = mock_address_write;
    iface.address_verify = mock_address_verify;
#endif

    return iface;
}

// ============================================================================
// initialize
// ============================================================================

TEST(DccApplicationServiceTrack, initialize_stores_interface) {
    reset_mocks();
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    DccApplicationServiceTrack_power_on();
    EXPECT_EQ(timer_start_count, 1u);
}

TEST(DccApplicationServiceTrack, initialize_with_null) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    DccApplicationServiceTrack_power_on();
    EXPECT_EQ(timer_start_count, 0u);
}

// ============================================================================
// power_on
// ============================================================================

TEST(DccApplicationServiceTrack, power_on_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    DccApplicationServiceTrack_power_on();
    EXPECT_EQ(power_set_count, 0u);
    EXPECT_EQ(timer_start_count, 0u);
    EXPECT_EQ(encoder_start_count, 0u);
}

TEST(DccApplicationServiceTrack, power_on_delegates) {
    reset_mocks();
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    DccApplicationServiceTrack_power_on();
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_TRUE(power_set_value);
    EXPECT_EQ(timer_start_count, 1u);
    EXPECT_EQ(timer_start_period, DCC_ONE_BIT_HALF_PERIOD_US);
    EXPECT_EQ(encoder_start_count, 1u);
}

// ============================================================================
// power_off
// ============================================================================

TEST(DccApplicationServiceTrack, power_off_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    DccApplicationServiceTrack_power_off();
    EXPECT_EQ(encoder_stop_count, 0u);
    EXPECT_EQ(timer_stop_count, 0u);
    EXPECT_EQ(power_set_count, 0u);
}

TEST(DccApplicationServiceTrack, power_off_delegates) {
    reset_mocks();
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    DccApplicationServiceTrack_power_off();
    EXPECT_EQ(encoder_stop_count, 1u);
    EXPECT_EQ(timer_stop_count, 1u);
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_FALSE(power_set_value);
}

// ============================================================================
// enter
// ============================================================================

TEST(DccApplicationServiceTrack, enter_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_enter();
    EXPECT_FALSE(result);
    EXPECT_EQ(enter_service_mode_count, 0u);
}

TEST(DccApplicationServiceTrack, enter_delegates_returns_true) {
    reset_mocks();
    enter_service_mode_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_enter();
    EXPECT_TRUE(result);
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_TRUE(power_set_value);
    EXPECT_EQ(timer_start_count, 1u);
    EXPECT_EQ(timer_start_period, DCC_ONE_BIT_HALF_PERIOD_US);
    EXPECT_EQ(encoder_start_count, 1u);
    EXPECT_EQ(enter_service_mode_count, 1u);
}

TEST(DccApplicationServiceTrack, enter_delegates_returns_false) {
    reset_mocks();
    enter_service_mode_return = false;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_enter();
    EXPECT_FALSE(result);
    EXPECT_EQ(enter_service_mode_count, 1u);
}

// ============================================================================
// exit
// ============================================================================

TEST(DccApplicationServiceTrack, exit_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    DccApplicationServiceTrack_exit();
    EXPECT_EQ(exit_service_mode_count, 0u);
}

TEST(DccApplicationServiceTrack, exit_delegates) {
    reset_mocks();
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    DccApplicationServiceTrack_exit();
    EXPECT_EQ(exit_service_mode_count, 1u);
    EXPECT_EQ(encoder_stop_count, 1u);
    EXPECT_EQ(timer_stop_count, 1u);
    EXPECT_EQ(power_set_count, 1u);
    EXPECT_FALSE(power_set_value);
}

// ============================================================================
// is_active
// ============================================================================

TEST(DccApplicationServiceTrack, is_active_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_is_active();
    EXPECT_FALSE(result);
    EXPECT_EQ(is_service_mode_active_count, 0u);
}

TEST(DccApplicationServiceTrack, is_active_delegates_true) {
    reset_mocks();
    is_service_mode_active_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_is_active();
    EXPECT_TRUE(result);
    EXPECT_EQ(is_service_mode_active_count, 1u);
}

TEST(DccApplicationServiceTrack, is_active_delegates_false) {
    reset_mocks();
    is_service_mode_active_return = false;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_is_active();
    EXPECT_FALSE(result);
    EXPECT_EQ(is_service_mode_active_count, 1u);
}

// ============================================================================
// Direct mode programming
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

TEST(DccApplicationServiceTrack, direct_write_byte_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_direct_write_byte(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(direct_write_byte_count, 0u);
}

TEST(DccApplicationServiceTrack, direct_write_byte_delegates) {
    reset_mocks();
    direct_write_byte_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_direct_write_byte(29, 0xAB);
    EXPECT_TRUE(result);
    EXPECT_EQ(direct_write_byte_count, 1u);
    EXPECT_EQ(direct_cv_number, 29u);
    EXPECT_EQ(direct_value, 0xAB);
}

TEST(DccApplicationServiceTrack, direct_verify_byte_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_direct_verify_byte(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(direct_verify_byte_count, 0u);
}

TEST(DccApplicationServiceTrack, direct_verify_byte_delegates) {
    reset_mocks();
    direct_verify_byte_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_direct_verify_byte(8, 0x10);
    EXPECT_TRUE(result);
    EXPECT_EQ(direct_verify_byte_count, 1u);
    EXPECT_EQ(direct_cv_number, 8u);
    EXPECT_EQ(direct_value, 0x10);
}

TEST(DccApplicationServiceTrack, direct_write_bit_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_direct_write_bit(1, 3, true);
    EXPECT_FALSE(result);
    EXPECT_EQ(direct_write_bit_count, 0u);
}

TEST(DccApplicationServiceTrack, direct_write_bit_delegates) {
    reset_mocks();
    direct_write_bit_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_direct_write_bit(29, 5, true);
    EXPECT_TRUE(result);
    EXPECT_EQ(direct_write_bit_count, 1u);
    EXPECT_EQ(direct_cv_number, 29u);
    EXPECT_EQ(direct_bit_position, 5u);
    EXPECT_TRUE(direct_bit_value);
}

TEST(DccApplicationServiceTrack, direct_verify_bit_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_direct_verify_bit(1, 0, false);
    EXPECT_FALSE(result);
    EXPECT_EQ(direct_verify_bit_count, 0u);
}

TEST(DccApplicationServiceTrack, direct_verify_bit_delegates) {
    reset_mocks();
    direct_verify_bit_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_direct_verify_bit(8, 7, false);
    EXPECT_TRUE(result);
    EXPECT_EQ(direct_verify_bit_count, 1u);
    EXPECT_EQ(direct_cv_number, 8u);
    EXPECT_EQ(direct_bit_position, 7u);
    EXPECT_FALSE(direct_bit_value);
}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

// ============================================================================
// Paged mode programming
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

TEST(DccApplicationServiceTrack, paged_write_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_paged_write(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(paged_write_count, 0u);
}

TEST(DccApplicationServiceTrack, paged_write_delegates) {
    reset_mocks();
    paged_write_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_paged_write(29, 0xCD);
    EXPECT_TRUE(result);
    EXPECT_EQ(paged_write_count, 1u);
    EXPECT_EQ(paged_cv_number, 29u);
    EXPECT_EQ(paged_value, 0xCD);
}

TEST(DccApplicationServiceTrack, paged_verify_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_paged_verify(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(paged_verify_count, 0u);
}

TEST(DccApplicationServiceTrack, paged_verify_delegates) {
    reset_mocks();
    paged_verify_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_paged_verify(8, 0xEF);
    EXPECT_TRUE(result);
    EXPECT_EQ(paged_verify_count, 1u);
    EXPECT_EQ(paged_cv_number, 8u);
    EXPECT_EQ(paged_value, 0xEF);
}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

// ============================================================================
// Register mode programming
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

TEST(DccApplicationServiceTrack, register_write_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_register_write(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(register_write_count, 0u);
}

TEST(DccApplicationServiceTrack, register_write_delegates) {
    reset_mocks();
    register_write_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_register_write(3, 0x42);
    EXPECT_TRUE(result);
    EXPECT_EQ(register_write_count, 1u);
    EXPECT_EQ(register_number, 3u);
    EXPECT_EQ(register_value, 0x42);
}

TEST(DccApplicationServiceTrack, register_verify_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_register_verify(1, 0x55);
    EXPECT_FALSE(result);
    EXPECT_EQ(register_verify_count, 0u);
}

TEST(DccApplicationServiceTrack, register_verify_delegates) {
    reset_mocks();
    register_verify_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_register_verify(5, 0x99);
    EXPECT_TRUE(result);
    EXPECT_EQ(register_verify_count, 1u);
    EXPECT_EQ(register_number, 5u);
    EXPECT_EQ(register_value, 0x99);
}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

// ============================================================================
// Address-only mode programming
// ============================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

TEST(DccApplicationServiceTrack, address_write_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_address_write(3);
    EXPECT_FALSE(result);
    EXPECT_EQ(address_write_count, 0u);
}

TEST(DccApplicationServiceTrack, address_write_delegates) {
    reset_mocks();
    address_write_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_address_write(42);
    EXPECT_TRUE(result);
    EXPECT_EQ(address_write_count, 1u);
    EXPECT_EQ(address_value, 42u);
}

TEST(DccApplicationServiceTrack, address_verify_null_guard) {
    reset_mocks();
    DccApplicationServiceTrack_initialize(NULL);
    bool result = DccApplicationServiceTrack_address_verify(3);
    EXPECT_FALSE(result);
    EXPECT_EQ(address_verify_count, 0u);
}

TEST(DccApplicationServiceTrack, address_verify_delegates) {
    reset_mocks();
    address_verify_return = true;
    interface_dcc_application_service_track_t iface = make_interface();
    DccApplicationServiceTrack_initialize(&iface);
    bool result = DccApplicationServiceTrack_address_verify(99);
    EXPECT_TRUE(result);
    EXPECT_EQ(address_verify_count, 1u);
    EXPECT_EQ(address_value, 99u);
}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#endif /* DCC_COMPILE_COMMAND_STATION */
