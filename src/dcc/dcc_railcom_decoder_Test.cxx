/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Encoder (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_decoder.h"
#include "dcc/dcc_railcom_command_station.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

#define MAX_UART_BYTES 16

static uint8_t uart_bytes[MAX_UART_BYTES];
static uint8_t uart_byte_count;

static void mock_uart_write(uint8_t byte) {

    if (uart_byte_count < MAX_UART_BYTES) {

        uart_bytes[uart_byte_count] = byte;
        uart_byte_count++;

    }

}

static void reset_mocks(void) {

    memset(uart_bytes, 0, sizeof(uart_bytes));
    uart_byte_count = 0;

}

static interface_dcc_railcom_decoder_t make_interface(void) {

    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.uart_write = mock_uart_write;

    return interface;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccRailcomDecoder, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

}

// ============================================================================
// Encode byte tests
// ============================================================================

// @compliance DCC-S9.3.2-DEC-008
TEST(DccRailcomDecoder, encode_byte_value_0x00) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    EXPECT_EQ(DccRailcomDecoder_encode_byte(0x00), (uint8_t)0xAC);

}

TEST(DccRailcomDecoder, encode_byte_value_0x01) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    EXPECT_EQ(DccRailcomDecoder_encode_byte(0x01), (uint8_t)0xAB);

}

TEST(DccRailcomDecoder, encode_byte_value_0x3F) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    EXPECT_EQ(DccRailcomDecoder_encode_byte(0x3F), (uint8_t)0xC4);

}

TEST(DccRailcomDecoder, encode_byte_out_of_range) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    EXPECT_EQ(DccRailcomDecoder_encode_byte(0x40), (uint8_t)0x00);
    EXPECT_EQ(DccRailcomDecoder_encode_byte(0xFF), (uint8_t)0x00);

}

// ============================================================================
// Encode/decode round-trip: verify encode table is inverse of decode table
// ============================================================================

// @compliance DCC-S9.3.2-DEC-008
TEST(DccRailcomDecoder, round_trip_all_values) {

    reset_mocks();
    interface_dcc_railcom_decoder_t enc_interface = make_interface();
    DccRailcomDecoder_initialize(&enc_interface);

    /* Also initialize the command-station receiver to use its decode function */
    dcc_railcom_command_station_context_t decoder_context;
    interface_dcc_railcom_command_station_t dec_interface;
    memset(&decoder_context, 0, sizeof(decoder_context));
    memset(&dec_interface, 0, sizeof(dec_interface));
    DccRailcomCommandStation_initialize(&decoder_context, &dec_interface);

    uint8_t value;

    for (value = 0; value < 64; value++) {

        uint8_t encoded = DccRailcomDecoder_encode_byte(value);
        uint8_t decoded = DccRailcomCommandStation_decode_byte(encoded);
        EXPECT_EQ(decoded, value) << "Round-trip failed for value " << (int)value;

    }

}

// ============================================================================
// Raw special code word tests (ACK/NACK, 2026 draft S-9.3.2)
// ============================================================================

// @compliance DCC-S9.3.2-CS-011
TEST(DccRailcomDecoder, send_code_word_ack_raw) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    /* ACK code word 0xF0 is transmitted raw, bypassing the 4/8 table */
    DccRailcomDecoder_send_code_word(DCC_RAILCOM_CODE_WORD_ACK);

    EXPECT_EQ(uart_byte_count, (uint8_t)1);
    EXPECT_EQ(uart_bytes[0], (uint8_t)0xF0);

}

// @compliance DCC-S9.3.2-CS-012
TEST(DccRailcomDecoder, send_code_word_nack_raw) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    /* NACK code word 0x3C is transmitted raw, bypassing the 4/8 table */
    DccRailcomDecoder_send_code_word(DCC_RAILCOM_CODE_WORD_NACK);

    EXPECT_EQ(uart_byte_count, (uint8_t)1);
    EXPECT_EQ(uart_bytes[0], (uint8_t)0x3C);

}

TEST(DccRailcomDecoder, send_code_word_null_uart_no_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.uart_write = NULL;
    DccRailcomDecoder_initialize(&interface);

    DccRailcomDecoder_send_code_word(DCC_RAILCOM_CODE_WORD_ACK);
    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

// ============================================================================
// Channel 1 send tests
// ============================================================================

// @compliance DCC-S9.3.2-CS-013
TEST(DccRailcomDecoder, send_ch1_basic) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    /* datagram_id=0, data=0x01 → combined=0x001 → high_six_bits=0x00, low_six_bits=0x01 */
    DccRailcomDecoder_send_ch1(0, 0x01);

    EXPECT_EQ(uart_byte_count, (uint8_t)2);
    /* encoded(0x00) = 0xAC, encoded(0x01) = 0xAB */
    EXPECT_EQ(uart_bytes[0], (uint8_t)0xAC);
    EXPECT_EQ(uart_bytes[1], (uint8_t)0xAB);

}

TEST(DccRailcomDecoder, send_ch1_with_datagram_id) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    /* datagram_id=15, data=0x00 → combined=0xF00 → high_six_bits=0x3C, low_six_bits=0x00 */
    DccRailcomDecoder_send_ch1(15, 0x00);

    EXPECT_EQ(uart_byte_count, (uint8_t)2);

}

TEST(DccRailcomDecoder, send_ch1_null_uart_no_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.uart_write = NULL;
    DccRailcomDecoder_initialize(&interface);

    DccRailcomDecoder_send_ch1(0, 0x42);
    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

// ============================================================================
// Channel 2 send tests
// ============================================================================

TEST(DccRailcomDecoder, send_ch2_single_data_byte) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.count = 1;

    DccRailcomDecoder_send_ch2(&response);

    /* 2 encoded bytes for ID + data[0] */
    EXPECT_EQ(uart_byte_count, (uint8_t)2);

}

// @compliance DCC-S9.3.2-CS-014
TEST(DccRailcomDecoder, send_ch2_multiple_data_bytes) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.data[1] = 0x02;
    response.data[2] = 0x03;
    response.count = 3;

    DccRailcomDecoder_send_ch2(&response);

    /* 2 bytes for ID+data[0], then 1 byte each for data[1] and data[2] */
    EXPECT_EQ(uart_byte_count, (uint8_t)4);

}

TEST(DccRailcomDecoder, send_ch2_zero_count_no_send) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.count = 0;

    DccRailcomDecoder_send_ch2(&response);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

TEST(DccRailcomDecoder, send_ch2_null_uart_no_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.uart_write = NULL;
    DccRailcomDecoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 7;
    response.data[0] = 0xFF;
    response.count = 1;

    DccRailcomDecoder_send_ch2(&response);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}
