/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Decoder (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_command_station.h"
#include "dcc/dcc_railcom_utilities.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Codewords used below (S-9.3.2 draft Table 2)
// ============================================================================

#define CW_00    0xAC   /* data 0x00 */
#define CW_01    0xAA   /* data 0x01 */
#define CW_02    0xA9   /* data 0x02 */
#define CW_03    0xA5   /* data 0x03 */
#define CW_04    0xA3   /* data 0x04 */
#define CW_05    0xA6   /* data 0x05 */
#define CW_07    0x9A   /* data 0x07 */
#define CW_08    0x99   /* data 0x08 */
#define CW_2A    0xC9   /* data 0x2A */
#define CW_ACK   0xF0   /* primary ACK */
#define CW_ACK2  0x0F   /* alternate ACK */
#define CW_NACK  0x3C   /* NACK */
#define CW_BAD   0x00   /* not a codeword */

#define CH1      DCC_RAILCOM_CH1
#define CH2      DCC_RAILCOM_CH2

// ============================================================================
// Mock / tracking state
// ============================================================================

#define MOCK_UART_DEPTH     32
#define MOCK_CALLBACK_DEPTH 8

static uint8_t uart_buffer[MOCK_UART_DEPTH];
static dcc_railcom_channel_enum uart_channel[MOCK_UART_DEPTH];
static uint8_t uart_buffer_count;
static uint8_t uart_read_index;
static uint32_t uart_read_calls;
static bool uart_never_empty;

static uint16_t callback_address[MOCK_CALLBACK_DEPTH];
static uint8_t callback_channel[MOCK_CALLBACK_DEPTH];
static dcc_railcom_datagram_t callback_datagram[MOCK_CALLBACK_DEPTH];
static uint32_t datagram_callback_count;

static bool mock_uart_read(uint8_t *byte, dcc_railcom_channel_enum *channel) {

    uart_read_calls++;

    if (uart_never_empty) {

        *byte = CW_00;
        *channel = CH2;

        return true;

    }

    if (uart_read_index >= uart_buffer_count)
        return false;

    *byte = uart_buffer[uart_read_index];
    *channel = uart_channel[uart_read_index];
    uart_read_index++;

    return true;

}

static void mock_on_datagram(uint16_t address, uint8_t channel,
                              const dcc_railcom_datagram_t *datagram) {

    if (datagram_callback_count < MOCK_CALLBACK_DEPTH) {

        callback_address[datagram_callback_count] = address;
        callback_channel[datagram_callback_count] = channel;
        memcpy(&callback_datagram[datagram_callback_count], datagram, sizeof(dcc_railcom_datagram_t));

    }

    datagram_callback_count++;

}

static dcc_railcom_command_station_context_t test_context;

static void reset_mocks(void) {

    memset(uart_buffer, 0, sizeof(uart_buffer));
    memset(uart_channel, 0, sizeof(uart_channel));
    uart_buffer_count = 0;
    uart_read_index = 0;
    uart_read_calls = 0;
    uart_never_empty = false;

    memset(callback_address, 0, sizeof(callback_address));
    memset(callback_channel, 0xFF, sizeof(callback_channel));
    memset(callback_datagram, 0, sizeof(callback_datagram));
    datagram_callback_count = 0;

    memset(&test_context, 0, sizeof(test_context));

}

static interface_dcc_railcom_command_station_t make_interface(void) {

    interface_dcc_railcom_command_station_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.uart_read = mock_uart_read;
    interface.on_datagram = mock_on_datagram;

    return interface;

}

// ============================================================================
// Helpers: queue tagged UART bytes for a cutout, run one cutout
// ============================================================================

static void queue_byte(uint8_t byte, dcc_railcom_channel_enum channel) {

    if (uart_buffer_count < MOCK_UART_DEPTH) {

        uart_buffer[uart_buffer_count] = byte;
        uart_channel[uart_buffer_count] = channel;
        uart_buffer_count++;

    }

}

static void queue_bytes(const uint8_t *bytes, uint8_t count, dcc_railcom_channel_enum channel) {

    uint8_t byte_index;

    for (byte_index = 0; byte_index < count; byte_index++) {

        queue_byte(bytes[byte_index], channel);

    }

}

static void run_cutout(uint16_t address) {

    uart_read_index = 0;
    DccRailcomCommandStation_begin_cutout(&test_context, address);
    DccRailcomCommandStation_run(&test_context);

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccRailcomCommandStation, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

}

TEST(DccRailcomCommandStation, initialize_clears_buffer) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

// ============================================================================
// Channel 1 decode tests (2 bytes -> 12 bits)
// ============================================================================

// @compliance DCC-S9.3.2-CS-013
TEST(DccRailcomCommandStation, ch1_valid_2_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* 0x00, 0x01 -> combined 0x001: datagram_id 0, data[0] 0x01 */
    uint8_t raw[2] = {CW_00, CW_01};
    queue_bytes(raw, 2, CH1);
    run_cutout(42);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_address[0], (uint16_t)42);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].channel, DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[0].datagram_id, (uint8_t)0);
    EXPECT_EQ(callback_datagram[0].data[0], (uint8_t)0x01);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)1);

}

TEST(DccRailcomCommandStation, ch1_invalid_first_byte_reports_invalid_codeword) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_BAD, CW_01};
    queue_bytes(raw, 2, CH1);
    run_cutout(10);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_INVALID_CODEWORD);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch1_invalid_second_byte_reports_invalid_codeword) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_00, CW_BAD};
    queue_bytes(raw, 2, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_INVALID_CODEWORD);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch1_single_byte_reports_too_few_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    queue_byte(CW_00, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_TOO_FEW_BYTES);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch1_three_bytes_reports_too_many_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[3] = {CW_00, CW_01, CW_02};
    queue_bytes(raw, 3, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_TOO_MANY_BYTES);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch1_ack_filler_reports_ack) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_ACK, CW_ACK2};
    queue_bytes(raw, 2, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_ACK);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

// ============================================================================
// Channel 2 decode tests
// ============================================================================

// @compliance DCC-S9.3.2-CS-014
TEST(DccRailcomCommandStation, ch2_valid_4_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    uint8_t ch2[4] = {CW_02, CW_03, CW_04, CW_05};
    queue_bytes(ch1, 2, CH1);
    queue_bytes(ch2, 4, CH2);
    run_cutout(100);

    /* 0x02, 0x03 -> combined 0x083: datagram_id 0, data[0] 0x83; then 0x04, 0x05 */
    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_channel[1], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[1].channel, DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[1].datagram_id, (uint8_t)0);
    EXPECT_EQ(callback_datagram[1].data[0], (uint8_t)0x83);
    EXPECT_EQ(callback_datagram[1].data[1], (uint8_t)0x04);
    EXPECT_EQ(callback_datagram[1].data[2], (uint8_t)0x05);
    EXPECT_EQ(callback_datagram[1].count, (uint8_t)3);

}

TEST(DccRailcomCommandStation, ch2_valid_2_bytes_minimum) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    uint8_t ch2[2] = {CW_02, CW_03};
    queue_bytes(ch1, 2, CH1);
    queue_bytes(ch2, 2, CH2);
    run_cutout(200);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)2);

}

// @compliance DCC-S9.3.2-CS-014
TEST(DccRailcomCommandStation, ch2_only_reply_decodes_as_channel_2) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* Issue #7: Channel 1 silent, POM read-back 0x2A in Channel 2 */
    uint8_t ch2[2] = {CW_00, CW_2A};
    queue_bytes(ch2, 2, CH2);
    run_cutout(3);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_address[0], (uint16_t)3);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[0].channel, DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[0].datagram_id, (uint8_t)0);
    EXPECT_EQ(callback_datagram[0].data[0], (uint8_t)0x2A);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_TRUE(DccRailcomCommandStation_read(&test_context, &read_datagram));
    EXPECT_EQ(read_datagram.channel, DCC_RAILCOM_CH2);
    EXPECT_FALSE(DccRailcomCommandStation_read(&test_context, &read_datagram));

}

// @compliance DCC-S9.3.2-CS-013
TEST(DccRailcomCommandStation, ch1_only_reply_has_no_channel_2) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    queue_bytes(ch1, 2, CH1);
    run_cutout(3);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

}

TEST(DccRailcomCommandStation, ch2_single_data_byte_reports_too_few_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    queue_bytes(ch1, 2, CH1);
    queue_byte(CW_02, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_channel[1], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_TOO_FEW_BYTES);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

}

TEST(DccRailcomCommandStation, ch2_single_data_byte_then_ack_reports_too_few_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[2] = {CW_02, CW_ACK};
    queue_bytes(ch2, 2, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_TOO_FEW_BYTES);

}

TEST(DccRailcomCommandStation, ch2_seven_bytes_reports_too_many_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[7] = {CW_00, CW_01, CW_02, CW_03, CW_04, CW_05, CW_07};
    queue_bytes(ch2, 7, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_TOO_MANY_BYTES);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch2_invalid_first_byte_reports_invalid_codeword) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    uint8_t ch2[2] = {CW_BAD, CW_03};
    queue_bytes(ch1, 2, CH1);
    queue_bytes(ch2, 2, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_INVALID_CODEWORD);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

}

TEST(DccRailcomCommandStation, ch2_invalid_byte_after_data_reports_invalid_codeword) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* A corrupted 4th byte of a longer datagram must not yield a shorter good one */
    uint8_t ch2[6] = {CW_00, CW_01, CW_02, CW_BAD, CW_04, CW_05};
    queue_bytes(ch2, 6, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_INVALID_CODEWORD);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch2_datagram_with_ack_filler_reports_ok) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[6] = {CW_00, CW_2A, CW_ACK, CW_ACK, CW_ACK2, CW_ACK};
    queue_bytes(ch2, 6, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[0].data[0], (uint8_t)0x2A);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)1);

}

TEST(DccRailcomCommandStation, ch2_datagram_then_nack_reports_ok) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[3] = {CW_00, CW_2A, CW_NACK};
    queue_bytes(ch2, 3, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[0].data[0], (uint8_t)0x2A);

}

TEST(DccRailcomCommandStation, ch2_data_after_control_word_reports_error) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[4] = {CW_00, CW_2A, CW_ACK, CW_05};
    queue_bytes(ch2, 4, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_DATA_AFTER_CONTROL_WORD);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch2_ack_only_reports_ack) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    queue_byte(CW_ACK, CH2);
    run_cutout(5);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_address[0], (uint16_t)5);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_ACK);
    EXPECT_EQ(callback_datagram[0].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch2_nack_only_reports_nack) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    queue_byte(CW_NACK, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_NACK);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, ch2_ack_and_nack_together_reports_nack) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch2[2] = {CW_ACK, CW_NACK};
    queue_bytes(ch2, 2, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_NACK);

}

// ============================================================================
// Channel tagging tests
// ============================================================================

TEST(DccRailcomCommandStation, interleaved_tags_sort_into_their_channels) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* Read order does not decide the channel; the tag does */
    queue_byte(CW_02, CH2);
    queue_byte(CW_00, CH1);
    queue_byte(CW_03, CH2);
    queue_byte(CW_01, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_channel[0], (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_EQ(callback_datagram[0].data[0], (uint8_t)0x01);
    EXPECT_EQ(callback_channel[1], (uint8_t)DCC_RAILCOM_CH2);
    EXPECT_EQ(callback_datagram[1].data[0], (uint8_t)0x83);

}

TEST(DccRailcomCommandStation, invalid_channel_tag_reported_once_and_discarded) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    queue_bytes(ch1, 2, CH1);
    queue_byte(CW_02, (dcc_railcom_channel_enum)7);
    queue_byte(CW_03, (dcc_railcom_channel_enum)9);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_INVALID_CHANNEL);
    EXPECT_EQ(callback_channel[1], (uint8_t)7);
    EXPECT_EQ(callback_datagram[1].count, (uint8_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

}

TEST(DccRailcomCommandStation, invalid_channel_tag_alone_reports_only_invalid_channel) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    queue_byte(CW_00, (dcc_railcom_channel_enum)2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_INVALID_CHANNEL);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

// ============================================================================
// Buffer tests
// ============================================================================

// @compliance DCC-S9.3.2-CS-015
TEST(DccRailcomCommandStation, read_returns_buffered_datagram) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_00, CW_01};
    queue_bytes(raw, 2, CH1);
    run_cutout(42);

    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_TRUE(DccRailcomCommandStation_read(&test_context, &read_datagram));
    EXPECT_EQ(read_datagram.result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(read_datagram.channel, DCC_RAILCOM_CH1);
    EXPECT_EQ(read_datagram.datagram_id, (uint8_t)0);

    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, read_empty_returns_false) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_FALSE(DccRailcomCommandStation_read(&test_context, &read_datagram));

}

// @compliance DCC-S9.3.2-CS-015
TEST(DccRailcomCommandStation, buffer_overflow_drops_oldest) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* Push 5 datagrams into depth-4 buffer — oldest should be dropped */
    uint8_t second_bytes[5] = {CW_01, CW_02, CW_03, CW_04, CW_05};
    uint8_t expected_data[4] = {0x02, 0x03, 0x04, 0x05};
    uint8_t cutout_index;
    uint8_t read_index;

    for (cutout_index = 0; cutout_index < 5; cutout_index++) {

        uart_buffer_count = 0;
        queue_byte(CW_00, CH1);
        queue_byte(second_bytes[cutout_index], CH1);
        run_cutout(cutout_index + 1);

    }

    EXPECT_EQ(datagram_callback_count, (uint32_t)5);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)4);

    for (read_index = 0; read_index < 4; read_index++) {

        dcc_railcom_datagram_t read_datagram;
        EXPECT_TRUE(DccRailcomCommandStation_read(&test_context, &read_datagram));
        EXPECT_EQ(read_datagram.data[0], expected_data[read_index]);

    }

    dcc_railcom_datagram_t read_datagram;
    EXPECT_FALSE(DccRailcomCommandStation_read(&test_context, &read_datagram));

}

// ============================================================================
// Run guard tests
// ============================================================================

TEST(DccRailcomCommandStation, run_without_cutout_does_nothing) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* Load bytes but don't begin cutout */
    uint8_t raw[2] = {CW_00, CW_01};
    queue_bytes(raw, 2, CH1);

    DccRailcomCommandStation_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(uart_read_calls, (uint32_t)0);

}

TEST(DccRailcomCommandStation, run_with_null_uart_does_nothing) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    interface.uart_read = NULL;
    DccRailcomCommandStation_initialize(&test_context, &interface);

    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

TEST(DccRailcomCommandStation, cutout_no_data) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    /* Both channels silent: nothing to report */
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomCommandStation, cutout_address_tagged_correctly) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_00, CW_01};
    queue_bytes(raw, 2, CH1);
    run_cutout(9999);

    EXPECT_EQ(callback_address[0], (uint16_t)9999);

}

TEST(DccRailcomCommandStation, end_cutout_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    DccRailcomCommandStation_end_cutout(&test_context);

}

// ============================================================================
// NULL callback tests
// ============================================================================

TEST(DccRailcomCommandStation, ch1_null_on_datagram_still_buffers) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    interface.on_datagram = NULL;
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t raw[2] = {CW_00, CW_01};
    queue_bytes(raw, 2, CH1);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)1);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_TRUE(DccRailcomCommandStation_read(&test_context, &read_datagram));
    EXPECT_EQ(read_datagram.result, DCC_RAILCOM_RESULT_OK);

}

TEST(DccRailcomCommandStation, ch2_null_on_datagram_still_buffers) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    interface.on_datagram = NULL;
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    uint8_t ch2[2] = {CW_02, CW_03};
    queue_bytes(ch1, 2, CH1);
    queue_bytes(ch2, 2, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)2);

}

TEST(DccRailcomCommandStation, null_on_datagram_with_error_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    interface.on_datagram = NULL;
    DccRailcomCommandStation_initialize(&test_context, &interface);

    queue_byte(CW_BAD, CH1);
    queue_byte(CW_00, (dcc_railcom_channel_enum)5);
    run_cutout(1);

    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)0);

}

// ============================================================================
// Read limit tests
// ============================================================================

TEST(DccRailcomCommandStation, full_cutout_2_plus_6_bytes) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uint8_t ch1[2] = {CW_00, CW_01};
    uint8_t ch2[6] = {CW_02, CW_03, CW_04, CW_05, CW_07, CW_08};
    queue_bytes(ch1, 2, CH1);
    queue_bytes(ch2, 6, CH2);
    run_cutout(1);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(callback_datagram[1].result, DCC_RAILCOM_RESULT_OK);
    EXPECT_EQ(callback_datagram[1].count, (uint8_t)5);
    EXPECT_EQ(DccRailcomCommandStation_available(&test_context), (uint8_t)2);

}

TEST(DccRailcomCommandStation, runaway_uart_read_stops_at_read_limit) {

    reset_mocks();
    interface_dcc_railcom_command_station_t interface = make_interface();
    DccRailcomCommandStation_initialize(&test_context, &interface);

    uart_never_empty = true;
    run_cutout(1);

    EXPECT_EQ(uart_read_calls, (uint32_t)DCC_RAILCOM_MAX_READS_PER_CUTOUT);
    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(callback_datagram[0].result, DCC_RAILCOM_RESULT_TOO_MANY_BYTES);

}
