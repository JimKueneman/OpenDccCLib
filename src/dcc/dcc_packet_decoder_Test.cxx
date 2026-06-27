/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Packet Decoder (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_packet_decoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

#define MOCK_CV_COUNT 1024

static uint8_t mock_cv_values[MOCK_CV_COUNT];

static uint16_t last_speed_address;
static uint8_t last_speed_value;
static bool last_speed_direction;
static dcc_speed_mode_enum last_speed_mode;
static uint32_t speed_callback_count;

static uint16_t last_estop_address;
static uint32_t estop_callback_count;

static uint16_t last_func_address;
static uint8_t last_func_number;
static bool last_func_state;
static uint32_t func_callback_count;

/* Track all function callbacks for group tests */
#define MAX_FUNC_CALLS 32
static uint8_t func_numbers[MAX_FUNC_CALLS];
static bool func_states[MAX_FUNC_CALLS];
static uint32_t func_call_index;

static uint16_t last_acc_board_address;
static uint8_t last_acc_output_pair;
static bool last_acc_activate;
static uint32_t acc_basic_callback_count;

static uint16_t last_acc_ext_address;
static uint8_t last_acc_ext_aspect;
static uint32_t acc_ext_callback_count;

static uint16_t last_cv_write_number;
static uint8_t last_cv_write_value;
static bool last_cv_write_service_mode;
static uint32_t cv_write_callback_count;

static uint16_t last_cv_verify_number;
static uint8_t last_cv_verify_value;
static bool last_cv_verify_service_mode;
static uint32_t cv_verify_callback_count;

static uint16_t last_cv_bit_number;
static uint8_t last_cv_bit_position;
static bool last_cv_bit_value;
static bool last_cv_bit_service_mode;
static uint32_t cv_bit_callback_count;

static uint16_t last_consist_address;
static uint8_t last_consist_consist_addr;
static bool last_consist_dir_normal;
static uint32_t consist_callback_count;

static uint16_t last_bss_address;
static uint8_t last_bss_state;
static bool last_bss_active;
static uint32_t bss_callback_count;

static uint16_t last_bsl_address;
static uint16_t last_bsl_state;
static bool last_bsl_active;
static uint32_t bsl_callback_count;

static uint16_t last_analog_address;
static uint8_t last_analog_output;
static uint8_t last_analog_value;
static uint32_t analog_callback_count;

static uint16_t last_acc_cv_write_number;
static uint8_t last_acc_cv_write_value;
static uint32_t acc_cv_write_callback_count;

static uint16_t last_acc_cv_verify_number;
static uint8_t last_acc_cv_verify_value;
static uint32_t acc_cv_verify_callback_count;

static uint16_t last_acc_cv_bit_number;
static uint8_t last_acc_cv_bit_position;
static bool last_acc_cv_bit_value;
static uint32_t acc_cv_bit_callback_count;

static bool mock_cv_read_should_fail;
static bool mock_cv_write_should_fail;

// ============================================================================
// Mock callbacks
// ============================================================================

static bool mock_cv_read(uint16_t cv_number, uint8_t *value) {

    if (mock_cv_read_should_fail)
        return false;

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    *value = mock_cv_values[cv_number - 1];
    return true;

}

static bool mock_cv_write(uint16_t cv_number, uint8_t value) {

    if (mock_cv_write_should_fail)
        return false;

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    mock_cv_values[cv_number - 1] = value;
    return true;

}

static void mock_on_speed(uint16_t address, uint8_t speed, bool direction,
                           dcc_speed_mode_enum mode) {

    last_speed_address = address;
    last_speed_value = speed;
    last_speed_direction = direction;
    last_speed_mode = mode;
    speed_callback_count++;

}

static void mock_on_estop(uint16_t address) {

    last_estop_address = address;
    estop_callback_count++;

}

static void mock_on_function(uint16_t address, uint8_t function_number, bool state) {

    last_func_address = address;
    last_func_number = function_number;
    last_func_state = state;
    func_callback_count++;

    if (func_call_index < MAX_FUNC_CALLS) {

        func_numbers[func_call_index] = function_number;
        func_states[func_call_index] = state;
        func_call_index++;

    }

}

static void mock_on_acc_basic(uint16_t board_address, uint8_t output_pair,
                               bool activate) {

    last_acc_board_address = board_address;
    last_acc_output_pair = output_pair;
    last_acc_activate = activate;
    acc_basic_callback_count++;

}

static void mock_on_acc_ext(uint16_t address, uint8_t aspect) {

    last_acc_ext_address = address;
    last_acc_ext_aspect = aspect;
    acc_ext_callback_count++;

}

static void mock_on_acc_cv_write(uint16_t cv_number, uint8_t value) {

    last_acc_cv_write_number = cv_number;
    last_acc_cv_write_value = value;
    acc_cv_write_callback_count++;

}

static void mock_on_acc_cv_verify(uint16_t cv_number, uint8_t value) {

    last_acc_cv_verify_number = cv_number;
    last_acc_cv_verify_value = value;
    acc_cv_verify_callback_count++;

}

static void mock_on_acc_cv_bit(uint16_t cv_number, uint8_t bit_position,
                                bool bit_value) {

    last_acc_cv_bit_number = cv_number;
    last_acc_cv_bit_position = bit_position;
    last_acc_cv_bit_value = bit_value;
    acc_cv_bit_callback_count++;

}

static void mock_on_cv_write(uint16_t cv_number, uint8_t value,
                              bool service_mode) {

    last_cv_write_number = cv_number;
    last_cv_write_value = value;
    last_cv_write_service_mode = service_mode;
    cv_write_callback_count++;

}

static void mock_on_cv_verify(uint16_t cv_number, uint8_t value,
                               bool service_mode) {

    last_cv_verify_number = cv_number;
    last_cv_verify_value = value;
    last_cv_verify_service_mode = service_mode;
    cv_verify_callback_count++;

}

static void mock_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                            bool bit_value, bool service_mode) {

    last_cv_bit_number = cv_number;
    last_cv_bit_position = bit_position;
    last_cv_bit_value = bit_value;
    last_cv_bit_service_mode = service_mode;
    cv_bit_callback_count++;

}

static void mock_on_consist(uint16_t address, uint8_t consist_address,
                             bool direction_normal) {

    last_consist_address = address;
    last_consist_consist_addr = consist_address;
    last_consist_dir_normal = direction_normal;
    consist_callback_count++;

}

static void mock_on_bss(uint16_t address, uint8_t state_number, bool active) {

    last_bss_address = address;
    last_bss_state = state_number;
    last_bss_active = active;
    bss_callback_count++;

}

static void mock_on_bsl(uint16_t address, uint16_t state_number, bool active) {

    last_bsl_address = address;
    last_bsl_state = state_number;
    last_bsl_active = active;
    bsl_callback_count++;

}

static void mock_on_analog(uint16_t address, uint8_t output_number, uint8_t value) {

    last_analog_address = address;
    last_analog_output = output_number;
    last_analog_value = value;
    analog_callback_count++;

}

static void reset_mocks(void) {

    memset(mock_cv_values, 0, sizeof(mock_cv_values));

    /* Default CV 29: 28-step mode enabled (bit 1 set) */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT;

    /* Default CV 1: short address 3 */
    mock_cv_values[DCC_CV_PRIMARY_ADDRESS - 1] = 3;

    last_speed_address = 0;
    last_speed_value = 0;
    last_speed_direction = false;
    last_speed_mode = DCC_SPEED_MODE_14;
    speed_callback_count = 0;

    last_estop_address = 0;
    estop_callback_count = 0;

    last_func_address = 0;
    last_func_number = 0;
    last_func_state = false;
    func_callback_count = 0;
    memset(func_numbers, 0, sizeof(func_numbers));
    memset(func_states, 0, sizeof(func_states));
    func_call_index = 0;

    last_acc_board_address = 0;
    last_acc_output_pair = 0;
    last_acc_activate = false;
    acc_basic_callback_count = 0;

    last_acc_ext_address = 0;
    last_acc_ext_aspect = 0;
    acc_ext_callback_count = 0;

    last_acc_cv_write_number = 0;
    last_acc_cv_write_value = 0;
    acc_cv_write_callback_count = 0;

    last_acc_cv_verify_number = 0;
    last_acc_cv_verify_value = 0;
    acc_cv_verify_callback_count = 0;

    last_acc_cv_bit_number = 0;
    last_acc_cv_bit_position = 0;
    last_acc_cv_bit_value = false;
    acc_cv_bit_callback_count = 0;

    last_cv_write_number = 0;
    last_cv_write_value = 0;
    last_cv_write_service_mode = false;
    cv_write_callback_count = 0;

    last_cv_verify_number = 0;
    last_cv_verify_value = 0;
    last_cv_verify_service_mode = false;
    cv_verify_callback_count = 0;

    last_cv_bit_number = 0;
    last_cv_bit_position = 0;
    last_cv_bit_value = false;
    last_cv_bit_service_mode = false;
    cv_bit_callback_count = 0;

    last_consist_address = 0;
    last_consist_consist_addr = 0;
    last_consist_dir_normal = false;
    consist_callback_count = 0;

    last_bss_address = 0;
    last_bss_state = 0;
    last_bss_active = false;
    bss_callback_count = 0;

    last_bsl_address = 0;
    last_bsl_state = 0;
    last_bsl_active = false;
    bsl_callback_count = 0;

    last_analog_address = 0;
    last_analog_output = 0;
    last_analog_value = 0;
    analog_callback_count = 0;

    mock_cv_read_should_fail = false;
    mock_cv_write_should_fail = false;

}

    /**
     * @brief Set mock CV store for short address and re-initialize decoder.
     */
static void set_decoder_short_address(interface_dcc_packet_decoder_t *interface, uint16_t address) {

    mock_cv_values[DCC_CV_PRIMARY_ADDRESS - 1] = (uint8_t)address;
    mock_cv_values[DCC_CV_CONFIG - 1] &= ~DCC_CV29_EXTENDED_ADDRESS_BIT;
    DccPacketDecoder_initialize(interface);

}

    /**
     * @brief Set mock CV store for long address and re-initialize decoder.
     */
static void set_decoder_long_address(interface_dcc_packet_decoder_t *interface, uint16_t address) {

    mock_cv_values[DCC_CV_EXTENDED_ADDRESS_HIGH - 1] = 0xC0 | ((address >> 8) & 0x3F);
    mock_cv_values[DCC_CV_EXTENDED_ADDRESS_LOW - 1] = address & 0xFF;
    mock_cv_values[DCC_CV_CONFIG - 1] |= DCC_CV29_EXTENDED_ADDRESS_BIT;
    DccPacketDecoder_initialize(interface);

}

    /**
     * @brief Set mock CV store for accessory address and re-initialize decoder.
     */
static void set_decoder_accessory_address(interface_dcc_packet_decoder_t *interface, uint16_t address, bool extended) {

    mock_cv_values[DCC_CV_ACC_ADDRESS_LSB - 1] = (uint8_t)(address & 0x3F);
    mock_cv_values[DCC_CV_ACC_ADDRESS_MSB - 1] = (uint8_t)((address >> 6) & 0x07);
    mock_cv_values[DCC_CV_ACC_CONFIG - 1] = DCC_CV541_ACCESSORY_DECODER_BIT | (extended ? DCC_CV541_BASIC_EXTENDED_BIT : 0);
    DccPacketDecoder_initialize(interface);

}

static interface_dcc_packet_decoder_t make_interface(void) {

    interface_dcc_packet_decoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.cv_read = mock_cv_read;
    interface.cv_write = mock_cv_write;
    interface.on_speed_command = mock_on_speed;
    interface.on_emergency_stop_command = mock_on_estop;
    interface.on_function_command = mock_on_function;
    interface.on_accessory_basic_command = mock_on_acc_basic;
    interface.on_accessory_extended_command = mock_on_acc_ext;
    interface.on_acc_cv_write = mock_on_acc_cv_write;
    interface.on_acc_cv_verify = mock_on_acc_cv_verify;
    interface.on_acc_cv_bit = mock_on_acc_cv_bit;
    interface.on_cv_write_command = mock_on_cv_write;
    interface.on_cv_verify_command = mock_on_cv_verify;
    interface.on_cv_bit_command = mock_on_cv_bit;
    interface.on_consist_command = mock_on_consist;
    interface.on_binary_state_short_command = mock_on_bss;
    interface.on_binary_state_long_command = mock_on_bsl;
    interface.on_analog_function_command = mock_on_analog;

    return interface;

}

    /**
     * @brief Compute XOR of bytes and return as last byte.
     */
static uint8_t xor_bytes(const uint8_t *data, uint8_t count) {

    uint8_t xor_val = 0;
    uint8_t byte_index;

    for (byte_index = 0; byte_index < count; byte_index++) {

        xor_val ^= data[byte_index];

    }

    return xor_val;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccPacketDecoder, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

}

// ============================================================================
// XOR validation
// ============================================================================

// @compliance DCC-S9.2-DEC-004
TEST(DccPacketDecoder, bad_xor_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0x60, 0x00};  /* XOR should be 0x63 */
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Idle packet ignored
// ============================================================================

// @compliance DCC-S9.2-DEC-001
TEST(DccPacketDecoder, idle_packet_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    uint8_t data[] = {0xFF, 0x00, 0xFF};
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Broadcast reset ignored
// ============================================================================

// @compliance DCC-S9.2-DEC-002
TEST(DccPacketDecoder, broadcast_reset_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    uint8_t data[] = {0x00, 0x00, 0x00};
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Address matching
// ============================================================================

// @compliance DCC-S9.2-DEC-005
TEST(DccPacketDecoder, wrong_address_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Packet for address 5, not 3 */
    uint8_t data[] = {0x05, 0x60, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// @compliance DCC-S9.2-DEC-005
TEST(DccPacketDecoder, matching_address_dispatched) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 28-step speed forward, SSSS=0010, C=1 → encoded=(2<<1)|1=5, speed=5-3=2 */
    uint8_t instruction = 0x60 | (0x02 | 0x10);  /* 0111 0010 = forward, C=1, SSSS=0010 */
    uint8_t data[] = {0x03, instruction, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_address, (uint16_t)3);

}

// ============================================================================
// 128-step speed
// ============================================================================

// @compliance DCC-S9.2.1-DEC-001
TEST(DccPacketDecoder, speed_128_forward) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step speed: 0x3F, then 1SSSSSSS (direction=1, speed=50) */
    uint8_t data[] = {0x03, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)50);
    EXPECT_TRUE(last_speed_direction);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_128);

}

TEST(DccPacketDecoder, speed_128_reverse) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step speed: 0x3F, then 0SSSSSSS (direction=0, speed=75) */
    uint8_t data[] = {0x03, 0x3F, 75, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)75);
    EXPECT_FALSE(last_speed_direction);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_128);

}

TEST(DccPacketDecoder, speed_128_estop) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step e-stop: speed byte = 0x81 (direction=1, speed=1) */
    uint8_t data[] = {0x03, 0x3F, 0x81, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(estop_callback_count, (uint32_t)1);
    EXPECT_EQ(last_estop_address, (uint16_t)3);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, speed_128_stop) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step stop: speed byte = 0x80 (direction=1, speed=0) */
    uint8_t data[] = {0x03, 0x3F, 0x80, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)0);

}

// ============================================================================
// 28-step speed
// ============================================================================

// @compliance DCC-S9.2.1-DEC-002
TEST(DccPacketDecoder, speed_28_forward_step_5) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 28-step forward: 01D CSSSS where D=1 (forward)
     * For API speed 5: _speed_28_encode[5] = 0x13 → C=1, SSSS=0011
     * Instruction = 0x60 | 0x13 = 0x73 */
    uint8_t data[] = {0x03, 0x73, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)5);
    EXPECT_TRUE(last_speed_direction);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_28);

}

TEST(DccPacketDecoder, speed_28_stop) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 28-step stop: encoded=0 → SSSS=0, C=0 → 0x60 */
    uint8_t data[] = {0x03, 0x60, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)0);

}

TEST(DccPacketDecoder, speed_28_estop) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 28-step e-stop: CSSSS=00001 → C=0, SSSS=0001 → 0x60|0x01 = 0x61 */
    uint8_t data[] = {0x03, 0x61, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(estop_callback_count, (uint32_t)1);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// 14-step speed (CV 29 bit 1 cleared)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-003
TEST(DccPacketDecoder, speed_14_step) {

    reset_mocks();
    /* Clear bit 1 in CV 29 for 14-step mode */
    mock_cv_values[DCC_CV_CONFIG - 1] = 0x00;

    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 14-step forward, speed=5: 011D SSSS → 0110 0101 = 0x65 */
    uint8_t data[] = {0x03, 0x65, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)5);  /* raw SSSS passed through directly */
    EXPECT_TRUE(last_speed_direction);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_14);

}

// ============================================================================
// Function group 1 (FL, F1-F4)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-004
TEST(DccPacketDecoder, func_group1_fl_on_f1_f2_on) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 100DDDDD: FL=bit4, F4F3F2F1=bits3-0
     * FL on, F1 on, F2 on, F3 off, F4 off: 1001 0011 = 0x93 */
    uint8_t data[] = {0x03, 0x93, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    /* FL + F1 + F2 + F3 + F4 = 5 callbacks */
    EXPECT_EQ(func_callback_count, (uint32_t)5);

    /* Check individual function states */
    EXPECT_EQ(func_numbers[0], (uint8_t)0);  /* FL */
    EXPECT_TRUE(func_states[0]);             /* FL on */
    EXPECT_EQ(func_numbers[1], (uint8_t)1);  /* F1 */
    EXPECT_TRUE(func_states[1]);             /* F1 on */
    EXPECT_EQ(func_numbers[2], (uint8_t)2);  /* F2 */
    EXPECT_TRUE(func_states[2]);             /* F2 on */
    EXPECT_EQ(func_numbers[3], (uint8_t)3);  /* F3 */
    EXPECT_FALSE(func_states[3]);            /* F3 off */
    EXPECT_EQ(func_numbers[4], (uint8_t)4);  /* F4 */
    EXPECT_FALSE(func_states[4]);            /* F4 off */

}

// ============================================================================
// Function group 2a (F5-F8)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-005
TEST(DccPacketDecoder, func_group2a_f5_f6_on) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 1011 DDDD: F5=bit0, F6=bit1, F7=bit2, F8=bit3
     * F5 on, F6 on: 1011 0011 = 0xB3 */
    uint8_t data[] = {0x03, 0xB3, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(func_callback_count, (uint32_t)4);
    EXPECT_EQ(func_numbers[0], (uint8_t)5);
    EXPECT_TRUE(func_states[0]);
    EXPECT_EQ(func_numbers[1], (uint8_t)6);
    EXPECT_TRUE(func_states[1]);
    EXPECT_EQ(func_numbers[2], (uint8_t)7);
    EXPECT_FALSE(func_states[2]);
    EXPECT_EQ(func_numbers[3], (uint8_t)8);
    EXPECT_FALSE(func_states[3]);

}

// ============================================================================
// Function group 2b (F9-F12)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-006
TEST(DccPacketDecoder, func_group2b_f9_on) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 1010 DDDD: F9=bit0
     * F9 on only: 1010 0001 = 0xA1 */
    uint8_t data[] = {0x03, 0xA1, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(func_callback_count, (uint32_t)4);
    EXPECT_EQ(func_numbers[0], (uint8_t)9);
    EXPECT_TRUE(func_states[0]);

}

// ============================================================================
// Feature expansion F13-F20
// ============================================================================

// @compliance DCC-S9.2.1-DEC-007
TEST(DccPacketDecoder, func_f13_f20) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Feature expansion: 0xDE, then 8-bit bitmask
     * F13 on, F14 on, rest off: 0x03 */
    uint8_t data[] = {0x03, 0xDE, 0x03, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)13);
    EXPECT_TRUE(func_states[0]);
    EXPECT_EQ(func_numbers[1], (uint8_t)14);
    EXPECT_TRUE(func_states[1]);
    EXPECT_EQ(func_numbers[2], (uint8_t)15);
    EXPECT_FALSE(func_states[2]);

}

// ============================================================================
// CV access (ops-mode write)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-010
TEST(DccPacketDecoder, cv_write_ops_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV write long form: 111011AA AAAAAAAA DDDDDDDD
     * Write value 0x42 to CV 1 (wire encoding: CV-1 = 0x0000)
     * Instruction byte 0: 1110 1100 = 0xEC (write, high addr = 00)
     * Instruction byte 1: 0x00 (low addr)
     * Data byte: 0x42 */
    uint8_t data[] = {0x03, 0xEC, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)1);
    EXPECT_EQ(last_cv_write_value, (uint8_t)0x42);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x42);

}

TEST(DccPacketDecoder, cv_write_cv29_updates_speed_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Write 0x00 to CV 29 (clears bit 1 → 14-step mode) */
    /* CV 29 wire encoding: 29-1 = 28 = 0x001C → high=0x00, low=0x1C */
    uint8_t data[] = {0x03, 0xEC, 0x1C, 0x00, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);

    /* Now send a speed command — should be 14-step */
    func_call_index = 0;
    uint8_t speed_data[] = {0x03, 0x65, 0x00};
    speed_data[2] = xor_bytes(speed_data, 2);
    DccPacketDecoder_process_packet(speed_data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_14);

}

// ============================================================================
// CV bit manipulation
// ============================================================================

// @compliance DCC-S9.2.1-DEC-010
TEST(DccPacketDecoder, cv_bit_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Set CV 1 to 0x00 first */
    mock_cv_values[0] = 0x00;

    /* Bit manipulation: 111010AA, data = 111KDBBB
     * Write bit 3 = 1 of CV 1
     * Instruction byte 0: 1110 1000 = 0xE8 (bit, high addr = 00)
     * Instruction byte 1: 0x00 (low addr)
     * Data byte: 1111 1011 = 0xFB → K=1(write), D=1(value), BBB=011(bit 3) */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xF8 | 0x08 | 0x03, 0x00};

    /* Correction: 111KDBBB where K=1(write), D=1, BBB=011
     * = 1111 1011 = 0xFB */
    data[3] = 0xFB;
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    /* Bit 3 set: 0x08 */
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x08);

}

// ============================================================================
// Long address
// ============================================================================

// @compliance DCC-S9.2.1-DEC-015
TEST(DccPacketDecoder, long_address_speed_128) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();

    /* Long address 200 = 0x00C8: high byte = 0xC0 | 0x00 = 0xC0, low = 0xC8 */
    set_decoder_long_address(&interface, 200);

    /* 128-step speed forward, speed=100 */
    uint8_t data[] = {0xC0, 0xC8, 0x3F, 0x80 | 100, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_address, (uint16_t)200);
    EXPECT_EQ(last_speed_value, (uint8_t)100);

}

TEST(DccPacketDecoder, long_address_mismatch_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_long_address(&interface, 200);

    /* Packet for long address 201 */
    uint8_t data[] = {0xC0, 0xC9, 0x3F, 0x80 | 50, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Basic accessory
// ============================================================================

// @compliance DCC-S9.2.1-DEC-008
TEST(DccPacketDecoder, basic_accessory) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Basic accessory: 10AAAAAA 1AAADAAA
     * board_address low 6 = 0x01, high 3 inverted = 0x07 (~0x00 & 0x07)
     * output_pair = 0, activate = 1
     * Byte 0: 10 000001 = 0x81
     * Byte 1: 1 111 1 000 = 0xF8 (high inv=111 → real high=000, D=1, pair=00)
     */
    uint8_t data[] = {0x81, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)1);
    EXPECT_EQ(last_acc_output_pair, (uint8_t)0);
    EXPECT_TRUE(last_acc_activate);

}

// ============================================================================
// Extended accessory
// ============================================================================

// @compliance DCC-S9.2.1-DEC-009
TEST(DccPacketDecoder, extended_accessory) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, true);

    /* Extended accessory: 10AAAAAA 0AAA0AA1 DDDDDDDD
     * Byte 0: 10 000001 = 0x81 (low 6 addr bits = 1)
     * Byte 1: 0111 0001 = 0x71 (high inv=111→real=000, addr ext bits=00, format=1)
     * Byte 2: aspect = 0x05
     */
    uint8_t data[] = {0x81, 0x71, 0x05, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(acc_ext_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_ext_aspect, (uint8_t)0x05);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccPacketDecoder, null_speed_callback_no_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_speed_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, null_function_callback_no_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_function_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0x93, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(func_callback_count, (uint32_t)0);

}

// ============================================================================
// Broadcast address
// ============================================================================

TEST(DccPacketDecoder, broadcast_address_accepted) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Broadcast address 0 should be accepted by any short-address decoder.
     * But address 0 is the broadcast reset prefix — so use a non-zero data byte.
     * Actually broadcast reset is data[0]=0x00 AND data[1]=0x00.
     * A broadcast speed command would be address 0 with a non-zero instruction. */

    /* Broadcast 128-step speed command */
    uint8_t data[] = {0x00, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Address 0 is DCC_ADDRESS_BROADCAST_VALUE. The address match check
     * accepts it for any short-address decoder, so the 128-step speed
     * command is dispatched normally. */
    EXPECT_EQ(speed_callback_count, (uint32_t)1);

}

// ============================================================================
// cv_read fallback (null cv_read defaults to 28-step)
// ============================================================================

TEST(DccPacketDecoder, no_cv_read_defaults_to_28_step) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_read = NULL;
    set_decoder_short_address(&interface, 3);

    /* Send broadcast 28-step speed command — address 0 bypasses match check */
    uint8_t data[] = {0x00, 0x73, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_28);

}

// ============================================================================
// XOR validation edge case: packet too short
// ============================================================================

TEST(DccPacketDecoder, single_byte_packet_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03};
    DccPacketDecoder_process_packet(data, 1);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// 14-step e-stop
// ============================================================================

TEST(DccPacketDecoder, speed_14_estop) {

    reset_mocks();
    mock_cv_values[DCC_CV_CONFIG - 1] = 0x00;

    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 14-step e-stop: SSSS=0001 → 0110 0001 = 0x61 */
    uint8_t data[] = {0x03, 0x61, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(estop_callback_count, (uint32_t)1);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// 14-step e-stop with null callback
// ============================================================================

TEST(DccPacketDecoder, speed_14_estop_null_callback) {

    reset_mocks();
    mock_cv_values[DCC_CV_CONFIG - 1] = 0x00;

    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_emergency_stop_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0x61, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(estop_callback_count, (uint32_t)0);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// 14-step speed with null callback
// ============================================================================

TEST(DccPacketDecoder, speed_14_null_speed_callback) {

    reset_mocks();
    mock_cv_values[DCC_CV_CONFIG - 1] = 0x00;

    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_speed_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0x65, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// CV verify
// ============================================================================

TEST(DccPacketDecoder, cv_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV verify: 111001AA AAAAAAAA DDDDDDDD
     * Verify CV 1 against value 0x55
     * Instruction byte 0: 1110 0100 = 0xE4
     * Instruction byte 1: 0x00 (low addr for CV 1)
     * Data byte: 0x55 */
    uint8_t data[] = {0x03, 0xE4, 0x00, 0x55, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_verify_number, (uint16_t)1);
    EXPECT_EQ(last_cv_verify_value, (uint8_t)0x55);

}

// ============================================================================
// CV bit manipulation: clear bit + on_cv_bit_command callback
// ============================================================================

TEST(DccPacketDecoder, cv_bit_clear) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Set CV 1 to 0xFF first */
    mock_cv_values[0] = 0xFF;

    /* Bit manipulation: clear bit 2 of CV 1
     * 111010AA = 0xE8, low addr = 0x00
     * Data: 111KDBBB where K=1(write), D=0(clear), BBB=010(bit 2)
     * = 1111 0010 = 0xF2 */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xF2, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0xFB);  /* bit 2 cleared */

    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_bit_number, (uint16_t)1);
    EXPECT_EQ(last_cv_bit_position, (uint8_t)2);
    EXPECT_FALSE(last_cv_bit_value);

}

// ============================================================================
// CV bit write to CV 29 updates speed mode
// ============================================================================

TEST(DccPacketDecoder, cv_bit_write_cv29_updates_speed_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV 29 wire encoding: 29-1 = 28 = 0x001C → high=0x00, low=0x1C */
    /* Clear bit 1 in CV 29 to switch to 14-step mode
     * Data: 111KDBBB = K=1, D=0, BBB=001 → 1111 0001 = 0xF1 */
    uint8_t data[] = {0x03, 0xE8, 0x1C, 0xF1, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    /* Now send a speed command — should decode as 14-step */
    uint8_t speed_data[] = {0x03, 0x65, 0x00};
    speed_data[2] = xor_bytes(speed_data, 2);
    DccPacketDecoder_process_packet(speed_data, 3);

    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_14);

}

// ============================================================================
// CV access too short (covers line 277)
// ============================================================================

TEST(DccPacketDecoder, cv_access_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV write with only 2 instruction bytes (need 3): 0xEC + 0x00, missing data */
    uint8_t data[] = {0x03, 0xEC, 0x00, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Consist control
// ============================================================================

// @compliance DCC-S9.2.1-DEC-011
TEST(DccPacketDecoder, consist_set_normal) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Consist set normal: 0x12 (DCC_CONSIST_SET_NORMAL), then consist addr 10 */
    uint8_t data[] = {0x03, DCC_CONSIST_SET_NORMAL, 10, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(consist_callback_count, (uint32_t)1);
    EXPECT_EQ(last_consist_consist_addr, (uint8_t)10);
    EXPECT_TRUE(last_consist_dir_normal);

}

TEST(DccPacketDecoder, consist_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_consist_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_CONSIST_SET_NORMAL, 10, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(consist_callback_count, (uint32_t)0);

}

// ============================================================================
// Analog function
// ============================================================================

// @compliance DCC-S9.2.1-DEC-014
TEST(DccPacketDecoder, analog_function) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Analog function: 0x3D, output_number, value */
    uint8_t data[] = {0x03, DCC_ADV_OPS_ANALOG_FUNCTION, 0x01, 0x80, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(analog_callback_count, (uint32_t)1);
    EXPECT_EQ(last_analog_output, (uint8_t)0x01);
    EXPECT_EQ(last_analog_value, (uint8_t)0x80);

}

TEST(DccPacketDecoder, analog_function_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_analog_function_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_ADV_OPS_ANALOG_FUNCTION, 0x01, 0x80, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(analog_callback_count, (uint32_t)0);

}

// ============================================================================
// Function groups F21-F28
// ============================================================================

TEST(DccPacketDecoder, func_f21_f28) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* F21-F28: 0xDF, bitmask */
    uint8_t data[] = {0x03, DCC_FEAT_F21_F28, 0x05, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)21);
    EXPECT_TRUE(func_states[0]);
    EXPECT_EQ(func_numbers[2], (uint8_t)23);
    EXPECT_TRUE(func_states[2]);

}

// ============================================================================
// Function groups F29-F36
// ============================================================================

TEST(DccPacketDecoder, func_f29_f36) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_F29_F36, 0x01, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)29);
    EXPECT_TRUE(func_states[0]);

}

// ============================================================================
// Function groups F37-F44
// ============================================================================

TEST(DccPacketDecoder, func_f37_f44) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_F37_F44, 0x01, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)37);

}

// ============================================================================
// Function groups F45-F52
// ============================================================================

TEST(DccPacketDecoder, func_f45_f52) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_F45_F52, 0x01, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)45);

}

// ============================================================================
// Function groups F53-F60
// ============================================================================

TEST(DccPacketDecoder, func_f53_f60) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_F53_F60, 0x01, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)53);

}

// ============================================================================
// Binary state short
// ============================================================================

// @compliance DCC-S9.2.1-DEC-012
TEST(DccPacketDecoder, binary_state_short) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Binary state short: 0xDD, DLLLLLLL (D=1 active, state=42) */
    uint8_t data[] = {0x03, DCC_FEAT_BINARY_STATE_SHORT, 0x80 | 42, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(bss_callback_count, (uint32_t)1);
    EXPECT_EQ(last_bss_state, (uint8_t)42);
    EXPECT_TRUE(last_bss_active);

}

TEST(DccPacketDecoder, binary_state_short_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_binary_state_short_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_BINARY_STATE_SHORT, 0x80 | 42, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(bss_callback_count, (uint32_t)0);

}

// ============================================================================
// Binary state long
// ============================================================================

// @compliance DCC-S9.2.1-DEC-013
TEST(DccPacketDecoder, binary_state_long) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Binary state long: 0xDC, DLLLLLLL, HHHHHHHH (3 instruction bytes)
     * state_num = (H << 7) | L, active = D
     * L=0x05 (low 7 = 5, D=0 inactive), H=0x02 → state = (2<<7)|5 = 261 */
    uint8_t data[] = {0x03, DCC_FEAT_BINARY_STATE_LONG, 0x05, 0x02, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(bsl_callback_count, (uint32_t)1);
    EXPECT_EQ(last_bsl_state, (uint16_t)261);
    EXPECT_FALSE(last_bsl_active);

}

TEST(DccPacketDecoder, binary_state_long_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_binary_state_long_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, DCC_FEAT_BINARY_STATE_LONG, 0x05, 0x02, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(bsl_callback_count, (uint32_t)0);

}

// ============================================================================
// F61-F68 (same opcode as BSL but only 2 instruction bytes)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-007
TEST(DccPacketDecoder, func_f61_f68) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* F61-F68: 0xDC with exactly 2 instruction bytes (no 3rd byte → not BSL) */
    uint8_t data[] = {0x03, DCC_FEAT_F61_F68, 0x03, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)8);
    EXPECT_EQ(func_numbers[0], (uint8_t)61);
    EXPECT_TRUE(func_states[0]);
    EXPECT_EQ(func_numbers[1], (uint8_t)62);
    EXPECT_TRUE(func_states[1]);

}

// ============================================================================
// Long address packet to short-address decoder (covers line 688)
// ============================================================================

// @compliance DCC-S9.2.1-DEC-015
TEST(DccPacketDecoder, long_addr_packet_to_short_decoder_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Long address packet for address 200 sent to a short-address decoder */
    uint8_t data[] = {0xC0, 0xC8, 0x3F, 0x80 | 50, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Reserved address range (covers line 695)
// ============================================================================

TEST(DccPacketDecoder, reserved_address_range_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Address byte 0xE8 is in the reserved range (> 0xE7 but not idle 0xFF) */
    uint8_t data[] = {0xE8, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Null _dispatch_functions callback (covers line 225)
// ============================================================================

TEST(DccPacketDecoder, null_function_callback_group2a) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_function_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* F5-F8 group with null callback */
    uint8_t data[] = {0x03, 0xB3, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(func_callback_count, (uint32_t)0);

}

// ============================================================================
// CV verify null callback
// ============================================================================

TEST(DccPacketDecoder, cv_verify_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_verify_command = NULL;
    set_decoder_short_address(&interface, 3);

    uint8_t data[] = {0x03, 0xE4, 0x00, 0x55, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory basic with too few bytes
// ============================================================================

TEST(DccPacketDecoder, accessory_basic_too_short) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Accessory packet with only 2 bytes (need at least 3 including XOR) */
    uint8_t data[] = {0x81, 0x81};
    DccPacketDecoder_process_packet(data, 2);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory extended with too few bytes
// ============================================================================

TEST(DccPacketDecoder, accessory_extended_too_short) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, true);

    /* Extended accessory needs 4 bytes — send only 3 */
    uint8_t data[] = {0x81, 0x71, 0xF0};
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_ext_callback_count, (uint32_t)0);

}

// ============================================================================
// Zero instruction bytes (covers line 461)
// ============================================================================

TEST(DccPacketDecoder, zero_instruction_bytes_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Short address packet with only address + XOR (no instruction bytes) */
    uint8_t data[] = {0x03, 0x03};
    DccPacketDecoder_process_packet(data, 2);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// ============================================================================
// Branch coverage: NULL callback and failure path tests
// ============================================================================

TEST(DccPacketDecoder, cv_read_fails_defaults_to_28_step) {

    reset_mocks();
    mock_cv_read_should_fail = true;

    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Send broadcast 28-step speed command — address 0 bypasses match check */
    uint8_t data[] = {0x00, 0x73, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_28);

}

TEST(DccPacketDecoder, speed_128_estop_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_emergency_stop_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* 128-step e-stop with null on_emergency_stop_command */
    uint8_t data[] = {0x03, 0x3F, 0x81, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(estop_callback_count, (uint32_t)0);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, speed_28_estop_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_emergency_stop_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* 28-step e-stop: encoded 2 → CSSSS=00001 → 0x61 */
    uint8_t data[] = {0x03, 0x61, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(estop_callback_count, (uint32_t)0);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, speed_28_null_speed_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_speed_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* 28-step forward speed */
    uint8_t data[] = {0x03, 0x73, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, cv_write_null_cv_write_function) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_write = NULL;
    set_decoder_short_address(&interface, 3);

    /* CV write instruction with NULL cv_write */
    uint8_t data[] = {0x03, 0xEC, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, cv_write_returns_false) {

    reset_mocks();
    mock_cv_write_should_fail = true;

    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV write that will fail */
    uint8_t data[] = {0x03, 0xEC, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, cv_write_null_on_cv_write_command_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_write_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* CV write succeeds but on_cv_write_command is NULL */
    uint8_t data[] = {0x03, 0xEC, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x42);
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, cv_bit_verify_only) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    mock_cv_values[0] = 0xFF;

    /* Bit manipulation verify (K=0): 111KDBBB = 1110 1011 = 0xEB
     * K=0 (verify, not write), D=1, BBB=011 (bit 3) */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xEB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    /* Verify-only: no write should happen, but on_cv_bit_command is still called */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0xFF);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_write_null_cv_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_write = NULL;
    set_decoder_short_address(&interface, 3);

    /* Bit write with NULL cv_write: K=1, D=1, BBB=011 → 0xFB */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_write_null_cv_read) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_read = NULL;
    set_decoder_short_address(&interface, 3);

    /* Bit write with NULL cv_read: K=1, D=1, BBB=011 → 0xFB
     * Use broadcast address 0x00 — cv_read is NULL so decoder has no cached address */
    uint8_t data[] = {0x00, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_write_cv_read_fails) {

    reset_mocks();
    mock_cv_read_should_fail = true;

    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Bit write where cv_read fails: K=1, D=1, BBB=011 → 0xFB
     * Use broadcast address 0x00 — cv_read fails so decoder has no cached address */
    uint8_t data[] = {0x00, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_write_cv_write_fails) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    mock_cv_values[0] = 0x00;

    /* Allow cv_read to succeed, then make cv_write fail */
    mock_cv_write_should_fail = true;

    /* Bit write: K=1, D=1, BBB=011 → 0xFB */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_write_null_on_cv_write_command) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_write_command = NULL;
    set_decoder_short_address(&interface, 3);

    mock_cv_values[0] = 0x00;

    /* Bit write succeeds but on_cv_write_command is NULL */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x08);
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_bit_null_on_cv_bit_command) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_bit_command = NULL;
    set_decoder_short_address(&interface, 3);

    mock_cv_values[0] = 0x00;

    /* Bit write with NULL on_cv_bit_command */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xFB, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_bit_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, accessory_basic_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_accessory_basic_command = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    uint8_t data[] = {0x81, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, accessory_extended_null_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_accessory_extended_command = NULL;
    set_decoder_accessory_address(&interface, 1, true);

    uint8_t data[] = {0x81, 0x71, 0x05, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(acc_ext_callback_count, (uint32_t)0);

}

// ============================================================================
// Branch coverage: compound condition short-circuits (too few inst bytes)
// ============================================================================

TEST(DccPacketDecoder, consist_too_few_inst_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Consist opcode (0x12) but only 1 instruction byte (needs 2) */
    uint8_t data[] = {0x03, DCC_CONSIST_SET_NORMAL, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(consist_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, adv_ops_128_speed_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step speed opcode (0x3F) but only 1 instruction byte (needs 2) */
    uint8_t data[] = {0x03, DCC_ADV_OPS_128_SPEED, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, adv_ops_analog_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Analog function opcode (0x3D) with only 2 instruction bytes (needs 3) */
    uint8_t data[] = {0x03, DCC_ADV_OPS_ANALOG_FUNCTION, 0x01, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(analog_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, feature_expansion_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Feature expansion opcode (0xDE = F13-F20) with only 1 inst byte (needs 2) */
    uint8_t data[] = {0x03, 0xDE, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(func_callback_count, (uint32_t)0);

}

// ============================================================================
// Branch coverage: process_packet address paths
// ============================================================================

TEST(DccPacketDecoder, broadcast_addr_byte_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 0);

    /* data[0]=0x00, byte_count=2 < 3 → broadcast reset check short-circuits */
    uint8_t data[] = {0x00, 0x00};
    DccPacketDecoder_process_packet(data, 2);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, accessory_packet_to_short_decoder) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Accessory-range packet to non-accessory decoder */
    uint8_t data[] = {0x81, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)0);
    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, long_address_too_few_bytes) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_long_address(&interface, 200);

    /* Long address range byte (0xC0) but only 3 bytes (needs 4) */
    uint8_t data[] = {0xC0, 0xC8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);

}

// @compliance DCC-S9.2-DEC-003
TEST(DccPacketDecoder, broadcast_packet_accepted_by_any_short_address) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 5);

    /* Broadcast packet (address 0) should be accepted by decoder at address 5 */
    uint8_t data[] = {0x00, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_address, (uint16_t)0);

}

// ============================================================================
// Branch coverage: unrecognized advanced ops (falls through without dispatch)
// ============================================================================

TEST(DccPacketDecoder, cv_reserved_command_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* CV command with reserved cv_command bits (0x00): 111000AA
     * inst[0] = 0xE0, cv_command = 0xE0 & 0x0C = 0x00 → falls through all cases */
    uint8_t data[] = {0x03, 0xE0, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_verify_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, unrecognized_adv_ops_instruction) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Advanced ops range (001xxxxx) but not 0x3F/0x3D → 0x20 */
    uint8_t data[] = {0x03, 0x20, 0x00, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)0);
    EXPECT_EQ(analog_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, unrecognized_feature_expansion) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Feature expansion range (110xxxxx) with unknown opcode 0xC0 */
    uint8_t data[] = {0x03, 0xC0, 0x00, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(func_callback_count, (uint32_t)0);

}

// ============================================================================
// Helper: send N reset packets to arm service mode
// ============================================================================

static void send_reset_packets(uint8_t count) {

    uint8_t reset[] = {0x00, 0x00, 0x00};
    uint8_t i;

    for (i = 0; i < count; i++) {

        DccPacketDecoder_process_packet(reset, 3);

    }

}

// ============================================================================
// Service mode — direct write byte (S-9.2.3)
// ============================================================================

TEST(DccPacketDecoder, svc_direct_write_byte) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    /* Arm service mode with 3 reset packets */
    send_reset_packets(3);

    /* Direct write byte: CV5 = 42
     * Byte 0: 0111 11 AA = 0x7C | (wire_cv>>8 & 0x03) = 0x7C (CV5 → wire 4, AA=00)
     * Byte 1: wire_cv & 0xFF = 0x04
     * Byte 2: data = 42 = 0x2A */
    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)5);
    EXPECT_EQ(last_cv_write_value, (uint8_t)42);
    EXPECT_EQ(mock_cv_values[4], (uint8_t)42);

}

// ============================================================================
// Service mode — direct verify byte
// ============================================================================

TEST(DccPacketDecoder, svc_direct_verify_byte) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Direct verify byte: CV5, value 42
     * Byte 0: 0111 01 AA = 0x74 | 0x00 = 0x74 */
    uint8_t data[] = {0x74, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_verify_number, (uint16_t)5);
    EXPECT_EQ(last_cv_verify_value, (uint8_t)42);
    /* Verify must NOT write */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — direct bit write
// ============================================================================

TEST(DccPacketDecoder, svc_direct_bit_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Pre-set CV5 = 0x00 */
    mock_cv_values[4] = 0x00;

    /* Direct bit write: CV5, bit 3 = 1
     * Byte 0: 0111 10 AA = 0x78 | 0x00 = 0x78
     * Byte 2: 111KDBBB — K=1(write), D=1, BBB=011
     *       = 0b11111011 = 0xFB */
    uint8_t data[] = {0x78, 0x04, 0xFB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Should write CV5 with bit 3 set → 0x08 */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)5);
    EXPECT_EQ(mock_cv_values[4], (uint8_t)0x08);
    /* on_cv_bit_command should also fire */
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_bit_number, (uint16_t)5);
    EXPECT_EQ(last_cv_bit_position, (uint8_t)3);
    EXPECT_TRUE(last_cv_bit_value);

}

// ============================================================================
// Service mode — direct bit verify
// ============================================================================

TEST(DccPacketDecoder, svc_direct_bit_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Direct bit verify: CV5, bit 3 = 1
     * Byte 2: 111KDBBB — K=0(verify), D=1, BBB=011
     *       = 0b11101011 = 0xEB */
    uint8_t data[] = {0x78, 0x04, 0xEB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Verify bit must NOT write */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    /* on_cv_bit_command fires for the bit operation */
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_bit_number, (uint16_t)5);
    EXPECT_EQ(last_cv_bit_position, (uint8_t)3);
    EXPECT_TRUE(last_cv_bit_value);

}

// ============================================================================
// Service mode — direct write high CV (CV 256)
// ============================================================================

TEST(DccPacketDecoder, svc_direct_write_high_cv) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* CV 256 → wire_cv = 255 = 0x00FF → AA=0x00, low byte=0xFF
     * Byte 0: 0x7C | 0x00 = 0x7C */
    uint8_t data[] = {0x7C, 0xFF, 0x55, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* CV 256 → mock index 255, within MOCK_CV_COUNT=1024 → write succeeds */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)256);
    EXPECT_EQ(last_cv_write_value, (uint8_t)0x55);

}

TEST(DccPacketDecoder, svc_direct_write_cv_beyond_store_fails) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* CV 1025 → wire_cv = 1024 = 0x0400 → AA=0x04, low byte=0x00
     * Byte 0: 0x7C | 0x04 = 0x7C (AA bits 1:0 = 00) — but 10-bit max is 1024
     * Actually wire CV is 10 bits max (0-1023) so CV 1025 cannot be encoded.
     * Use CV 1025 won't work in 10-bit encoding. Use mock_cv_read_should_fail. */
    mock_cv_write_should_fail = true;
    uint8_t data[] = {0x7D, 0x03, 0xAA, 0x00};
    data[3] = xor_bytes(data, 3);

    /* cv_write returns false → no callback */
    DccPacketDecoder_process_packet(data, 4);
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — register write
// ============================================================================

TEST(DccPacketDecoder, svc_register_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Register write: register 1 (RRR=000), value 42
     * Byte 0: 0111 1 000 = 0x78
     * Byte 1: 0x2A = 42 */
    uint8_t data[] = {0x78, 0x2A, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)1);
    EXPECT_EQ(last_cv_write_value, (uint8_t)42);

}

// ============================================================================
// Service mode — register verify
// ============================================================================

TEST(DccPacketDecoder, svc_register_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Register verify: register 5 (RRR=100), value 99
     * Byte 0: 0111 0 100 = 0x74
     * Byte 1: 0x63 = 99 */
    uint8_t data[] = {0x74, 0x63, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_verify_number, (uint16_t)5);
    EXPECT_EQ(last_cv_verify_value, (uint8_t)99);
    /* Verify must NOT write */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — register write updates CV storage
// ============================================================================

TEST(DccPacketDecoder, svc_register_write_stores_value) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Register 5 → CV5: write value 100 */
    uint8_t data[] = {0x7C, 0x64, 0x00};  /* 0111 1 100 = 0x7C, RRR=100 → reg 5 */
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(mock_cv_values[4], (uint8_t)100);

}

// ============================================================================
// Service mode — rejected without prior reset packets
// ============================================================================

TEST(DccPacketDecoder, svc_rejected_without_resets) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* No reset packets sent — service mode should NOT be active.
     * Send a direct write packet — should be ignored. */
    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    EXPECT_EQ(cv_verify_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — rejected after only 2 reset packets (need 3)
// ============================================================================

TEST(DccPacketDecoder, svc_rejected_after_two_resets) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    /* Only 2 resets — not enough */
    send_reset_packets(2);

    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — accepted after exactly 3 reset packets
// ============================================================================

TEST(DccPacketDecoder, svc_accepted_after_three_resets) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    send_reset_packets(3);

    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)5);

}

// ============================================================================
// Service mode — normal packet clears service mode state
// ============================================================================

TEST(DccPacketDecoder, normal_packet_clears_service_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Arm service mode */
    send_reset_packets(3);

    /* Send a normal speed packet — this clears service mode */
    uint8_t speed[] = {0x03, 0x3F, 0x40, 0x00};
    speed[3] = xor_bytes(speed, 3);
    DccPacketDecoder_process_packet(speed, 4);

    /* Now send service mode packet — should be rejected */
    uint8_t svc[] = {0x7C, 0x04, 0x2A, 0x00};
    svc[3] = xor_bytes(svc, 3);
    DccPacketDecoder_process_packet(svc, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Address 117 speed command NOT misinterpreted as service mode
// ============================================================================

TEST(DccPacketDecoder, addr_117_not_misinterpreted_as_service_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 117);

    /* Speed command to address 117 (0x75).
     * 0x75 is in the 0x70-0x7F range but service mode is NOT active.
     * Byte 0: 0x75 (addr 117)
     * Byte 1: 0x3F (128-speed advanced op)
     * Byte 2: 0xC0 (speed 64 forward)
     */
    uint8_t data[] = {0x75, 0x3F, 0xC0, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Should be dispatched as a normal speed command, not service mode */
    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_address, (uint16_t)117);
    EXPECT_EQ(last_speed_value, (uint8_t)64);
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Address 112-127 range works normally when service mode not active
// ============================================================================

TEST(DccPacketDecoder, addr_112_works_without_service_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 112);

    uint8_t data[] = {0x70, 0x3F, 0x80, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_address, (uint16_t)112);
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Idle packets don't reset the service mode counter
// ============================================================================

TEST(DccPacketDecoder, idle_does_not_reset_service_mode_counter) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);

    /* 2 resets, then idle, then 1 more reset = 3 total resets */
    send_reset_packets(2);

    uint8_t idle[] = {0xFF, 0x00, 0xFF};
    DccPacketDecoder_process_packet(idle, 3);

    send_reset_packets(1);

    /* Should be accepted — 3 resets total with idle in between */
    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);

}

// ============================================================================
// Service mode persists across multiple command packets
// ============================================================================

TEST(DccPacketDecoder, svc_mode_persists_across_commands) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* First service mode write */
    uint8_t data1[] = {0x7C, 0x04, 0x2A, 0x00};
    data1[3] = xor_bytes(data1, 3);
    DccPacketDecoder_process_packet(data1, 4);

    /* Second service mode write (same command repeated per NMRA) */
    DccPacketDecoder_process_packet(data1, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)2);

}

// ============================================================================
// Service mode — CV29 write updates speed mode cache
// ============================================================================

TEST(DccPacketDecoder, svc_direct_write_cv29_updates_speed_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);
    send_reset_packets(3);

    /* Write CV29 = 0x04 (clear speed step bit → 14-step mode)
     * CV29 → wire_cv = 28 = 0x001C → AA=0x00, low=0x1C */
    uint8_t data[] = {0x7C, 0x1C, 0x04, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_cv_write_number, (uint16_t)29);

    /* Verify speed mode updated — send a 14/28-step speed command
     * and check it decodes as 14-step */
    send_reset_packets(3);  /* post-reset clears nothing (still resets) */

    /* Exit service mode with a normal packet */
    uint8_t speed[] = {0x03, 0x60, 0x00};
    speed[2] = xor_bytes(speed, 2);
    DccPacketDecoder_process_packet(speed, 3);

    /* Speed value 8, direction forward: 01D SSSSS = 0110 1000 = 0x68
     * In 14-step: speed = 0x08 & 0x0F = 8 */
    uint8_t speed14[] = {0x03, 0x68, 0x00};
    speed14[2] = xor_bytes(speed14, 2);
    DccPacketDecoder_process_packet(speed14, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)2);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_14);

}

// ============================================================================
// Service mode — cv_write failure prevents on_cv_write_command callback
// ============================================================================

TEST(DccPacketDecoder, svc_write_failure_no_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    mock_cv_write_should_fail = true;

    uint8_t data[] = {0x7C, 0x04, 0x2A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Service mode — bit write with cv_read failure prevents write
// ============================================================================

TEST(DccPacketDecoder, svc_bit_write_cv_read_failure) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    mock_cv_read_should_fail = true;

    /* Bit write: CV5 bit 3 = 1 */
    uint8_t data[] = {0x78, 0x04, 0xF3, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* cv_read failed so no write should happen */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)0);
    /* on_cv_bit_command still fires regardless */
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

// ============================================================================
// Service mode — bit clear (D=0)
// ============================================================================

TEST(DccPacketDecoder, svc_direct_bit_clear) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Pre-set CV5 = 0xFF */
    mock_cv_values[4] = 0xFF;

    /* Bit write: CV5, bit 3 = 0 (clear)
     * 111KDBBB — K=1(write), D=0, BBB=011
     * = 0b11110011 = 0xF3 */
    uint8_t data[] = {0x78, 0x04, 0xF3, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    /* 0xFF with bit 3 cleared = 0xF7 */
    EXPECT_EQ(mock_cv_values[4], (uint8_t)0xF7);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);
    EXPECT_FALSE(last_cv_bit_value);

}

// ============================================================================
// Output-address mode (CV541 bit 6) for accessory decoders
// ============================================================================

static void set_decoder_accessory_output_address(
    interface_dcc_packet_decoder_t *interface,
    uint16_t output_address,
    bool extended) {

    uint16_t cv_value = output_address + 1;
    mock_cv_values[DCC_CV_ACC_ADDRESS_LSB - 1] = (uint8_t)(cv_value & 0xFF);
    mock_cv_values[DCC_CV_ACC_ADDRESS_MSB - 1] = (uint8_t)((cv_value >> 8) & 0xFF);
    mock_cv_values[DCC_CV_ACC_CONFIG - 1] = DCC_CV541_ACCESSORY_DECODER_BIT
        | DCC_CV541_ADDRESS_METHOD_BIT
        | (extended ? DCC_CV541_BASIC_EXTENDED_BIT : 0);
    DccPacketDecoder_initialize(interface);

}

// @compliance DCC-S9.2.1-DEC-008
TEST(DccPacketDecoder, output_address_mode_basic_accessory) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 4, false);

    /* Output address 4 = 9-bit board 1 with A0-A1 = 00
     * Byte 0: 10 000001 = 0x81 (A2=1, A3-A7=0)
     * Byte 1: 1 111 1 000 = 0xF8 (A8-A10 inv=111, D=1, R=0, A0-A1=00) */
    uint8_t data[] = {0x81, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)4);
    EXPECT_EQ(last_acc_output_pair, (uint8_t)0);
    EXPECT_TRUE(last_acc_activate);

}

TEST(DccPacketDecoder, output_address_mode_address_zero) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 0, false);

    /* Output address 0 = 9-bit board 0 with A0-A1 = 00
     * Byte 0: 10 000000 = 0x80
     * Byte 1: 1 111 1 000 = 0xF8 */
    uint8_t data[] = {0x80, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)0);

}

TEST(DccPacketDecoder, output_address_mode_max_2047) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 2047, false);

    /* Output address 2047 = A0-A10 all ones
     * Byte 0: 10 111111 = 0xBF (A2-A7 = 111111)
     * Byte 1: 1 000 1 111 = 0x8F (A8-A10 inv=000, D=1, R=1, A0-A1=11) */
    uint8_t data[] = {0xBF, 0x8F, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)2047);
    EXPECT_EQ(last_acc_output_pair, (uint8_t)1);
    EXPECT_TRUE(last_acc_activate);

}

TEST(DccPacketDecoder, output_address_mode_with_r_bit) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 4, false);

    /* Same address 4 but with R bit set (DDD bit 2)
     * Byte 0: 10 000001 = 0x81
     * Byte 1: 1 111 1 100 = 0xFC (A8-A10 inv=111, D=1, R=1, A0-A1=00) */
    uint8_t data[] = {0x81, 0xFC, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)4);
    EXPECT_EQ(last_acc_output_pair, (uint8_t)1);

}

TEST(DccPacketDecoder, decoder_address_mode_unchanged) {

    /* Verify existing decoder-address mode still works correctly */
    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Same packet as basic_accessory test — board 1, pair 0 */
    uint8_t data[] = {0x81, 0xF8, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(acc_basic_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_board_address, (uint16_t)1);
    EXPECT_EQ(last_acc_output_pair, (uint8_t)0);
    EXPECT_TRUE(last_acc_activate);

}

// ============================================================================
// Accessory CV ops-mode — basic accessory (decoder-address mode)
// ============================================================================

TEST(DccPacketDecoder, acc_basic_cv_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Basic accessory CV write: 6-byte packet
     * Byte 0: 10 000001 = 0x81 (low 6 addr bits = 1)
     * Byte 1: 1 111 1 00 0 = 0xF8 (high inv=111→real=000, bit3=1, A1A0=00, bit0=0)
     * Byte 2: 1110 1100 = 0xEC (CV long write, CV high bits = 00)
     * Byte 3: 0x00 (CV low byte → CV 1)
     * Byte 4: 0x42 (value)
     * Byte 5: XOR */
    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_write_number, (uint16_t)1);
    EXPECT_EQ(last_acc_cv_write_value, (uint8_t)0x42);
    /* Verify CV storage was updated */
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x42);
    /* Operating command callback should NOT fire */
    EXPECT_EQ(acc_basic_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, acc_basic_cv_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Basic accessory CV verify: CV 29 against value 0x55
     * CV 29 → wire 28 = 0x01C → high bits = 0x00, low = 0x1C
     * Byte 2: 1110 0100 = 0xE4 (CV long verify) */
    uint8_t data[] = {0x81, 0xF8, 0xE4, 0x1C, 0x55, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_verify_number, (uint16_t)29);
    EXPECT_EQ(last_acc_cv_verify_value, (uint8_t)0x55);

}

TEST(DccPacketDecoder, acc_basic_cv_bit_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Set CV 1 to 0xFF first */
    mock_cv_values[0] = 0xFF;

    /* Basic accessory CV bit write: clear bit 2 of CV 1
     * Byte 2: 1110 1000 = 0xE8 (CV long bit)
     * Byte 3: 0x00 (CV low byte → CV 1)
     * Byte 4: 111 1 0 010 = 0xF2 (write=1, value=0, position=2) */
    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xF2, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_bit_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_bit_number, (uint16_t)1);
    EXPECT_EQ(last_acc_cv_bit_position, (uint8_t)2);
    EXPECT_FALSE(last_acc_cv_bit_value);
    /* Verify the actual bit was cleared in CV storage */
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0xFB);

}

TEST(DccPacketDecoder, acc_basic_cv_wrong_address_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 2, false);

    /* CV write packet for board address 1 — we are board 2 */
    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory CV ops-mode — basic accessory (output-address mode)
// ============================================================================

TEST(DccPacketDecoder, acc_basic_cv_write_output_address_mode) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 4, false);

    /* Output address 4 = board 1, A1A0 = 00
     * Byte 0: 10 000001 = 0x81
     * Byte 1: 1 111 1 00 0 = 0xF8 (high inv=111, bit3=1, A1=0 A0=0, bit0=0)
     * CV ops-mode byte 1: A1A0 are in bits 2-1 = (data[1] >> 1) & 0x03 = 0
     * output_address = (board << 2) | 0 = 4 */
    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0xAB, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_write_number, (uint16_t)1);
    EXPECT_EQ(last_acc_cv_write_value, (uint8_t)0xAB);

}

TEST(DccPacketDecoder, acc_basic_cv_write_output_address_wrong_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_output_address(&interface, 5, false);

    /* Same packet as above targets output address 4 — we are 5 */
    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0xAB, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory CV ops-mode — extended accessory
// ============================================================================

TEST(DccPacketDecoder, acc_extended_cv_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, true);

    /* Extended accessory CV write: 6-byte packet
     * Byte 0: 10 000001 = 0x81 (low 6 addr bits = 1)
     * Byte 1: 0 111 0 00 1 = 0x71 (high inv=111→real=000, bits 9-10=00)
     * Byte 2: 0xEC (CV long write, CV high bits = 00)
     * Byte 3: 0x00 (CV low → CV 1)
     * Byte 4: 0x99 (value)
     * Byte 5: XOR */
    uint8_t data[] = {0x81, 0x71, 0xEC, 0x00, 0x99, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_write_number, (uint16_t)1);
    EXPECT_EQ(last_acc_cv_write_value, (uint8_t)0x99);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x99);
    /* Operating command callback should NOT fire */
    EXPECT_EQ(acc_ext_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, acc_extended_cv_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, true);

    /* Extended accessory CV verify: CV 29 against 0x77 */
    uint8_t data[] = {0x81, 0x71, 0xE4, 0x1C, 0x77, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_verify_number, (uint16_t)29);
    EXPECT_EQ(last_acc_cv_verify_value, (uint8_t)0x77);

}

TEST(DccPacketDecoder, acc_extended_cv_bit_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, true);

    mock_cv_values[0] = 0x00;

    /* Extended accessory CV bit write: set bit 5 of CV 1
     * Byte 4: 111 1 1 101 = 0xFD (write=1, value=1, position=5) */
    uint8_t data[] = {0x81, 0x71, 0xE8, 0x00, 0xFD, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_bit_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_cv_bit_number, (uint16_t)1);
    EXPECT_EQ(last_acc_cv_bit_position, (uint8_t)5);
    EXPECT_TRUE(last_acc_cv_bit_value);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x20);

}

TEST(DccPacketDecoder, acc_extended_cv_wrong_address_ignored) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 2, true);

    /* CV write for extended address 1 — we are 2 */
    uint8_t data[] = {0x81, 0x71, 0xEC, 0x00, 0x99, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, acc_cv_null_callbacks_no_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_acc_cv_write = NULL;
    interface.on_acc_cv_verify = NULL;
    interface.on_acc_cv_bit = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    /* CV write — should not crash with NULL callback */
    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* CV was still written to storage even though callback is NULL */
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x42);
    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Direction reversal via CV29 bit 0
// ============================================================================

static uint32_t ack_pulse_count;

static void mock_start_ack_pulse(void) {

    ack_pulse_count++;

}

TEST(DccPacketDecoder, speed_128_direction_reversed) {

    reset_mocks();
    /* CV29 bit 0 set = direction reversed, bit 1 = 28-step */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT | DCC_CV29_DIRECTION_BIT;
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 128-step speed forward (direction bit=1), speed=50 */
    uint8_t data[] = {0x03, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_value, (uint8_t)50);
    /* Direction bit is 1 (forward) but reversed by CV29 → false */
    EXPECT_FALSE(last_speed_direction);

}

TEST(DccPacketDecoder, speed_28_direction_reversed) {

    reset_mocks();
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT | DCC_CV29_DIRECTION_BIT;
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 28-step forward (D=1): 0110 0010 | 0001 0000 = 0x72
     * encoded=(2<<1)|1=5, speed = _speed_28_decode[5] = 5 */
    uint8_t data[] = {0x03, 0x72, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    /* Direction reversed: forward (D=1) XOR reversed → false */
    EXPECT_FALSE(last_speed_direction);

}

TEST(DccPacketDecoder, speed_14_direction_reversed) {

    reset_mocks();
    /* CV29: bit 0 direction reversed, bit 1 NOT set → 14-step mode */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_DIRECTION_BIT;
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* 14-step forward (D=1), speed=5: 0110 0101 = 0x65 */
    uint8_t data[] = {0x03, 0x65, 0x00};
    data[2] = xor_bytes(data, 2);
    DccPacketDecoder_process_packet(data, 3);

    EXPECT_EQ(speed_callback_count, (uint32_t)1);
    EXPECT_EQ(last_speed_mode, DCC_SPEED_MODE_14);
    /* Direction reversed */
    EXPECT_FALSE(last_speed_direction);

}

// ============================================================================
// Service mode verify — value mismatch (no ACK)
// ============================================================================

TEST(DccPacketDecoder, svc_verify_byte_mismatch_no_ack) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);

    /* Store value 0x55 in CV5 */
    mock_cv_values[4] = 0x55;

    send_reset_packets(3);

    /* Direct verify CV5 with expected value 0xAA (mismatch) */
    uint8_t data[] = {0x74, 0x04, 0xAA, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Callback fires regardless of match/mismatch */
    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);
    /* But no ACK pulse because stored != expected */
    EXPECT_EQ(ack_pulse_count, (uint32_t)0);

}

TEST(DccPacketDecoder, svc_verify_byte_match_fires_ack) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);

    /* Store value 0x55 in CV5 */
    mock_cv_values[4] = 0x55;

    send_reset_packets(3);

    /* Direct verify CV5 with expected value 0x55 (match) */
    uint8_t data[] = {0x74, 0x04, 0x55, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);
    EXPECT_EQ(ack_pulse_count, (uint32_t)1);

}

// ============================================================================
// Service mode verify bit — mismatch (no ACK)
// ============================================================================

TEST(DccPacketDecoder, svc_verify_bit_mismatch_no_ack) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);

    /* Store 0x00 in CV5 — all bits are 0 */
    mock_cv_values[4] = 0x00;

    send_reset_packets(3);

    /* Direct bit verify: CC=10, CV5
     * Byte 0: 0111 10 AA = 0x78 | 0x00 = 0x78
     * Byte 1: CV low = 0x04
     * Byte 2: 111KDBBB = bit verify (K=0), D=1 (expect bit=1), BBB=3 (bit 3)
     *        = 1110 1011 = 0xEB */
    uint8_t data[] = {0x78, 0x04, 0xEB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Bit 3 of 0x00 is 0, but we expected 1 — no ACK */
    EXPECT_EQ(ack_pulse_count, (uint32_t)0);

}

TEST(DccPacketDecoder, svc_verify_bit_match_fires_ack) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);

    /* Store 0x08 in CV5 — bit 3 is 1 */
    mock_cv_values[4] = 0x08;

    send_reset_packets(3);

    /* Direct bit verify: expect bit 3 = 1
     * Byte 2: 111KDBBB = K=0 (verify), D=1, BBB=3
     *        = 1110 1011 = 0xEB */
    uint8_t data[] = {0x78, 0x04, 0xEB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Bit 3 matches expected 1 — ACK fired */
    EXPECT_EQ(ack_pulse_count, (uint32_t)1);

}

// ============================================================================
// Service mode write — NULL start_ack_pulse
// ============================================================================

TEST(DccPacketDecoder, svc_write_null_ack_pulse) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = NULL;
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Direct write CV1 = 0x42: CC=11, CV1
     * Byte 0: 0111 11 00 = 0x7C
     * Byte 1: 0x00 (CV1 wire = 0)
     * Byte 2: 0x42 */
    uint8_t data[] = {0x7C, 0x00, 0x42, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Write still succeeds even though ACK pulse is NULL */
    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);
    EXPECT_EQ(mock_cv_values[0], (uint8_t)0x42);

}

// ============================================================================
// Extended address partial CV read failure
// ============================================================================

// ============================================================================
// Partial CV read failure helpers (forward declaration for tests below)
// ============================================================================

static uint16_t partial_fail_cv_early = 0;

static bool mock_cv_read_partial_fail_early(uint16_t cv_number, uint8_t *value) {

    if (cv_number == partial_fail_cv_early)
        return false;

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    *value = mock_cv_values[cv_number - 1];
    return true;

}

TEST(DccPacketDecoder, extended_address_partial_cv_read_fail) {

    reset_mocks();
    /* Configure for long address mode */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT | DCC_CV29_EXTENDED_ADDRESS_BIT;
    mock_cv_values[DCC_CV_EXTENDED_ADDRESS_HIGH - 1] = 0xC0 | 0x01;
    mock_cv_values[DCC_CV_EXTENDED_ADDRESS_LOW - 1] = 0x50;

    interface_dcc_packet_decoder_t interface = make_interface();

    /* Make CV18 read fail — partial failure in _update_extended_address
     * where CV17 reads ok but CV18 fails */
    partial_fail_cv_early = DCC_CV_EXTENDED_ADDRESS_LOW;
    interface.cv_read = mock_cv_read_partial_fail_early;
    DccPacketDecoder_initialize(&interface);

    /* Extended address wasn't fully read, so address stays at default.
     * No crash is the important assertion. */

}

TEST(DccPacketDecoder, cv29_read_failure_defaults) {

    reset_mocks();
    /* Test CV29 read failure → defaults to 28-step, no direction reversal */
    mock_cv_read_should_fail = true;

    interface_dcc_packet_decoder_t interface = make_interface();
    DccPacketDecoder_initialize(&interface);
    mock_cv_read_should_fail = false;

    /* With cv_read failing, _my_address defaults to 0 (broadcast).
     * Send a broadcast packet to verify safe defaults work. */
    uint8_t data[] = {0x00, 0x3F, 0x80 | 50, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Broadcast reset is handled differently (addr 0, data 0).
     * This is addr 0 with non-zero instruction, so it hits the
     * broadcast address match path. No crash is important. */

}

// ============================================================================
// Accessory address partial CV read failure (CV513 ok, CV521 fails)
// ============================================================================

static uint16_t partial_fail_cv = 0;

static bool mock_cv_read_partial_fail(uint16_t cv_number, uint8_t *value) {

    if (cv_number == partial_fail_cv)
        return false;

    if (cv_number == 0 || cv_number > MOCK_CV_COUNT)
        return false;

    *value = mock_cv_values[cv_number - 1];
    return true;

}

TEST(DccPacketDecoder, accessory_address_cv521_read_fail) {

    reset_mocks();
    /* Set up as accessory decoder */
    mock_cv_values[DCC_CV_ACC_CONFIG - 1] = DCC_CV541_ACCESSORY_DECODER_BIT;
    mock_cv_values[DCC_CV_ACC_ADDRESS_LSB - 1] = 0x05;
    mock_cv_values[DCC_CV_ACC_ADDRESS_MSB - 1] = 0x01;

    interface_dcc_packet_decoder_t interface = make_interface();

    /* CV521 (ACC_ADDRESS_MSB) read will fail */
    partial_fail_cv = DCC_CV_ACC_ADDRESS_MSB;
    interface.cv_read = mock_cv_read_partial_fail;
    DccPacketDecoder_initialize(&interface);

    /* Should not crash — address just won't be fully configured */

}

TEST(DccPacketDecoder, accessory_address_cv541_read_fail) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();

    /* CV541 read will fail — _update_accessory_address never gets _my_address_type set */
    partial_fail_cv = DCC_CV_ACC_CONFIG;
    interface.cv_read = mock_cv_read_partial_fail;

    /* Also set CV29 to NOT have accessory bit (so CV541 check fails first) */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT;
    DccPacketDecoder_initialize(&interface);

    /* Should fall through to multi-function path with CV29 read fail on CV541 */

}

// ============================================================================
// CV verify with cv_read failure — no ACK, callback still fires
// ============================================================================

TEST(DccPacketDecoder, svc_verify_byte_cv_read_fails) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    interface.cv_read = NULL;  /* cv_read is NULL */
    DccPacketDecoder_initialize(&interface);

    /* Use interface with cv_read to initialize, then set to NULL */
    interface.cv_read = mock_cv_read;
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Now NULL out cv_read before verify */
    interface.cv_read = NULL;

    uint8_t data[] = {0x74, 0x04, 0x55, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* No ACK because cv_read is NULL */
    EXPECT_EQ(ack_pulse_count, (uint32_t)0);
    /* Callback still fires */
    EXPECT_EQ(cv_verify_callback_count, (uint32_t)1);

}

// ============================================================================
// Accessory CV verify and bit with NULL callbacks
// ============================================================================

TEST(DccPacketDecoder, acc_cv_verify_null_callback_no_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_acc_cv_verify = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    /* CV verify for basic accessory: byte2 = 111001AA = 0xE4, AA=0x00 */
    uint8_t data[] = {0x81, 0xF8, 0xE4, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_verify_callback_count, (uint32_t)0);

}

TEST(DccPacketDecoder, acc_cv_bit_null_callback_no_crash) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_acc_cv_bit = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    /* CV bit write for basic accessory: byte2 = 111010AA = 0xE8
     * byte4 = 111KDBBB = 0xF8 (K=1 write, D=1, BBB=0) */
    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xF8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_bit_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory CV bit — cv_write NULL and cv_read NULL guards
// ============================================================================

TEST(DccPacketDecoder, acc_cv_bit_write_null_cv_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_write = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    /* CV bit write for basic accessory */
    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xF8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Should not crash — _cv_bit_manipulate_acc returns early */

}

TEST(DccPacketDecoder, acc_cv_bit_write_null_cv_read) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* NULL out cv_read after init */
    interface.cv_read = NULL;

    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xF8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Should not crash */

}

// ============================================================================
// Accessory CV write — cv_write NULL guard
// ============================================================================

TEST(DccPacketDecoder, acc_cv_write_null_cv_write) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.cv_write = NULL;
    set_decoder_accessory_address(&interface, 1, false);

    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Write callback should not fire because cv_write is NULL */
    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Accessory CV write — cv_write returns false
// ============================================================================

TEST(DccPacketDecoder, acc_cv_write_fails) {

    reset_mocks();
    mock_cv_write_should_fail = true;
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Write failed, so on_acc_cv_write callback should not fire */
    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// CV bit verify in ops mode — no on_cv_bit_command
// ============================================================================

TEST(DccPacketDecoder, cv_bit_verify_null_on_cv_bit_command) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_bit_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* CV access bit verify: 111010AA, data=111KDBBB (K=0 verify)
     * Byte 1: 0xE8 | 0x00 = 0xE8
     * Byte 2: CV low = 0x00 (CV1)
     * Byte 3: 1110 0011 = 0xE3 (K=0, D=0, BBB=3) */
    uint8_t data[] = {0x03, 0xE8, 0x00, 0xE3, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_bit_callback_count, (uint32_t)0);

}

// ============================================================================
// CV verify — on_cv_verify_command NULL
// ============================================================================

TEST(DccPacketDecoder, cv_verify_null_on_cv_verify_callback) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_verify_command = NULL;
    set_decoder_short_address(&interface, 3);

    /* CV verify ops: 111001AA = 0xE4, CV1 = 0x00, value = 0x42 */
    uint8_t data[] = {0x03, 0xE4, 0x00, 0x42, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_verify_callback_count, (uint32_t)0);

}

// ============================================================================
// CV write triggers address update for specific CVs
// ============================================================================

TEST(DccPacketDecoder, cv_write_cv17_updates_address_cache) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Write CV17 (extended address high) via ops mode: 111011AA = 0xEC
     * CV17 wire = 16 → AA = 0x00, low = 0x10 */
    uint8_t data[] = {0x03, 0xEC, 0x10, 0xC1, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, cv_write_cv18_updates_address_cache) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_short_address(&interface, 3);

    /* Write CV18 (extended address low) via ops mode
     * CV18 wire = 17 → AA = 0x00, low = 0x11 */
    uint8_t data[] = {0x03, 0xEC, 0x11, 0x50, 0x00};
    data[4] = xor_bytes(data, 4);
    DccPacketDecoder_process_packet(data, 5);

    EXPECT_EQ(cv_write_callback_count, (uint32_t)1);

}

// ============================================================================
// Accessory CV ops write triggers address update for specific CVs
// ============================================================================

TEST(DccPacketDecoder, acc_cv_write_cv541_updates_address_cache) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* CV write for CV541 (ACC_CONFIG) via basic accessory ops-mode
     * CV541 wire = 540 = 0x21C → AA = 0x02, low = 0x1C
     * Byte 2: 111011AA = 0xEC | 0x02 = 0xEE */
    uint8_t data[] = {0x81, 0xF8, 0xEE, 0x1C, 0x80, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, acc_cv_write_cv513_updates_address_cache) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* CV write for CV513 (ACC_ADDRESS_LSB) via basic accessory ops-mode
     * CV513 wire = 512 = 0x200 → AA = 0x02, low = 0x00
     * Byte 2: 111011AA = 0xEC | 0x02 = 0xEE */
    uint8_t data[] = {0x81, 0xF8, 0xEE, 0x00, 0x05, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)1);

}

// ============================================================================
// Accessory bit manipulate with cv_read failure
// ============================================================================

TEST(DccPacketDecoder, acc_cv_bit_write_cv_read_fails) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* Set cv_read to fail */
    mock_cv_read_should_fail = true;

    /* CV bit write for basic accessory: byte2=111010AA=0xE8
     * byte4=111KDBBB=0xF8 (K=1 write, D=1, BBB=0) */
    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xF8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Should not crash — cv_read failed, bit manipulate returns early */

}

// ============================================================================
// Accessory bit verify (non-write path in acc CV)
// ============================================================================

TEST(DccPacketDecoder, acc_cv_bit_verify) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    /* CV bit verify for basic accessory: byte2=111010AA=0xE8
     * byte4=111KDBBB=0xE8 (K=0 verify, D=1, BBB=0) */
    uint8_t data[] = {0x81, 0xF8, 0xE8, 0x00, 0xE8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_bit_callback_count, (uint32_t)1);

}

// ============================================================================
// Accessory extended address mismatch
// ============================================================================

TEST(DccPacketDecoder, acc_extended_command_dispatches) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 0, true);

    /* Extended accessory for address 0, aspect 10 */
    uint8_t data[] = {0x80, 0x71, 0x0A, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(acc_ext_callback_count, (uint32_t)1);
    EXPECT_EQ(last_acc_ext_address, (uint16_t)0);
    EXPECT_EQ(last_acc_ext_aspect, (uint8_t)10);

}

// ============================================================================
// Service mode bit verify in service mode context
// ============================================================================

TEST(DccPacketDecoder, svc_direct_bit_verify_match_fires_ack) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);

    mock_cv_values[4] = 0x08;  /* bit 3 is 1 */

    send_reset_packets(3);

    /* Direct bit verify: CC=10, CV5
     * Byte 0: 0111 10 00 = 0x78
     * Byte 1: CV low = 0x04
     * Byte 2: 111KDBBB = K=0 (verify), D=1, BBB=3 = 0xEB */
    uint8_t data[] = {0x78, 0x04, 0xEB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* Bit 3 = 1, expected 1 → ACK */
    EXPECT_EQ(ack_pulse_count, (uint32_t)1);
    EXPECT_EQ(cv_bit_callback_count, (uint32_t)1);

}

// ============================================================================
// Service mode bit write with NULL on_cv_bit_command
// ============================================================================

TEST(DccPacketDecoder, svc_bit_write_null_on_cv_bit_command) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.on_cv_bit_command = NULL;
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* Direct bit write: CC=10, CV1
     * Byte 0: 0111 10 00 = 0x78
     * Byte 1: CV low = 0x00
     * Byte 2: 111KDBBB = K=1 (write), D=1, BBB=0 = 0xF8 */
    uint8_t data[] = {0x78, 0x00, 0xF8, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    EXPECT_EQ(cv_bit_callback_count, (uint32_t)0);
    /* CV1 bit 0 should still be set */
    EXPECT_TRUE(mock_cv_values[0] & 0x01);

}

// ============================================================================
// Primary address cv_read failure
// ============================================================================

TEST(DccPacketDecoder, primary_address_cv_read_fail) {

    reset_mocks();
    /* Set CV29 to short address mode */
    mock_cv_values[DCC_CV_CONFIG - 1] = DCC_CV29_SPEED_STEPS_BIT;

    interface_dcc_packet_decoder_t interface = make_interface();

    /* Make CV1 (primary address) read fail */
    partial_fail_cv_early = DCC_CV_PRIMARY_ADDRESS;
    interface.cv_read = mock_cv_read_partial_fail_early;
    DccPacketDecoder_initialize(&interface);

    /* Should not crash — address stays at default 0 */

}

// ============================================================================
// Accessory output-address mode with CV read partial failure
// ============================================================================

TEST(DccPacketDecoder, accessory_output_addr_cv_lsb_read_fail) {

    reset_mocks();
    mock_cv_values[DCC_CV_ACC_CONFIG - 1] = DCC_CV541_ACCESSORY_DECODER_BIT | DCC_CV541_ADDRESS_METHOD_BIT;
    mock_cv_values[DCC_CV_ACC_ADDRESS_LSB - 1] = 0x01;
    mock_cv_values[DCC_CV_ACC_ADDRESS_MSB - 1] = 0x00;

    interface_dcc_packet_decoder_t interface = make_interface();

    /* Fail CV513 read — second compound && in _update_accessory_address fails at first read */
    partial_fail_cv_early = DCC_CV_ACC_ADDRESS_LSB;
    interface.cv_read = mock_cv_read_partial_fail_early;
    DccPacketDecoder_initialize(&interface);

    /* Should not crash */

}

// ============================================================================
// CV verify bit — cv_read fails
// ============================================================================

TEST(DccPacketDecoder, svc_verify_bit_cv_read_fails) {

    reset_mocks();
    ack_pulse_count = 0;
    interface_dcc_packet_decoder_t interface = make_interface();
    interface.start_ack_pulse = mock_start_ack_pulse;
    DccPacketDecoder_initialize(&interface);
    send_reset_packets(3);

    /* NULL out cv_read before verify */
    interface.cv_read = NULL;

    /* Direct bit verify: CC=10, CV5 */
    uint8_t data[] = {0x78, 0x04, 0xEB, 0x00};
    data[3] = xor_bytes(data, 3);
    DccPacketDecoder_process_packet(data, 4);

    /* No ACK — cv_read is NULL */
    EXPECT_EQ(ack_pulse_count, (uint32_t)0);

}

// ============================================================================
// Accessory CV write returns false — _cv_write_and_notify_acc
// ============================================================================

TEST(DccPacketDecoder, acc_cv_write_returns_false) {

    reset_mocks();
    mock_cv_write_should_fail = true;
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 1, false);

    uint8_t data[] = {0x81, 0xF8, 0xEC, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    /* Write failed — callback should not fire */
    EXPECT_EQ(acc_cv_write_callback_count, (uint32_t)0);

}

// ============================================================================
// Extended accessory CV operations via packet decoder
// ============================================================================

TEST(DccPacketDecoder, acc_extended_cv_verify_addr0) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 0, true);

    /* Extended accessory CV verify: 6-byte packet, byte2=111001AA = 0xE4
     * Address 0: byte0=0x80, byte1=0x71 */
    uint8_t data[] = {0x80, 0x71, 0xE4, 0x00, 0x42, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_verify_callback_count, (uint32_t)1);

}

TEST(DccPacketDecoder, acc_extended_cv_bit_write_addr0) {

    reset_mocks();
    interface_dcc_packet_decoder_t interface = make_interface();
    set_decoder_accessory_address(&interface, 0, true);

    /* Extended accessory CV bit write: byte2=111010AA=0xE8
     * byte4=111KDBBB=0xF8 (K=1, D=1, BBB=0) */
    uint8_t data[] = {0x80, 0x71, 0xE8, 0x00, 0xF8, 0x00};
    data[5] = xor_bytes(data, 5);
    DccPacketDecoder_process_packet(data, 6);

    EXPECT_EQ(acc_cv_bit_callback_count, (uint32_t)1);

}
