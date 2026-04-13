/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Encoder (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_encoder.h"
#include "dcc/dcc_railcom_decoder.h"
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

static interface_dcc_railcom_encoder_t make_interface(void) {

    interface_dcc_railcom_encoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.uart_write = mock_uart_write;

    return interface;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccRailcomEncoder, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

}

// ============================================================================
// Encode byte tests
// ============================================================================

TEST(DccRailcomEncoder, encode_byte_value_0x00) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    EXPECT_EQ(DccRailcomEncoder_encode_byte(0x00), (uint8_t)0xAC);

}

TEST(DccRailcomEncoder, encode_byte_value_0x01) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    EXPECT_EQ(DccRailcomEncoder_encode_byte(0x01), (uint8_t)0xAB);

}

TEST(DccRailcomEncoder, encode_byte_value_0x3F) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    EXPECT_EQ(DccRailcomEncoder_encode_byte(0x3F), (uint8_t)0xC4);

}

TEST(DccRailcomEncoder, encode_byte_out_of_range) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    EXPECT_EQ(DccRailcomEncoder_encode_byte(0x40), (uint8_t)0x00);
    EXPECT_EQ(DccRailcomEncoder_encode_byte(0xFF), (uint8_t)0x00);

}

// ============================================================================
// Encode/decode round-trip: verify encode table is inverse of decode table
// ============================================================================

TEST(DccRailcomEncoder, round_trip_all_values) {

    reset_mocks();
    interface_dcc_railcom_encoder_t enc_interface = make_interface();
    DccRailcomEncoder_initialize(&enc_interface);

    /* Also initialize decoder to use its decode function */
    dcc_railcom_decoder_context_t decoder_context;
    interface_dcc_railcom_decoder_t dec_interface;
    memset(&decoder_context, 0, sizeof(decoder_context));
    memset(&dec_interface, 0, sizeof(dec_interface));
    DccRailcomDecoder_initialize(&decoder_context, &dec_interface);

    uint8_t value;

    for (value = 0; value < 64; value++) {

        uint8_t encoded = DccRailcomEncoder_encode_byte(value);
        uint8_t decoded = DccRailcomDecoder_decode_byte(encoded);
        EXPECT_EQ(decoded, value) << "Round-trip failed for value " << (int)value;

    }

}

// ============================================================================
// Channel 1 send tests
// ============================================================================

TEST(DccRailcomEncoder, send_ch1_basic) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    /* datagram_id=0, data=0x01 → combined=0x001 → high_six_bits=0x00, low_six_bits=0x01 */
    DccRailcomEncoder_send_ch1(0, 0x01);

    EXPECT_EQ(uart_byte_count, (uint8_t)2);
    /* encoded(0x00) = 0xAC, encoded(0x01) = 0xAB */
    EXPECT_EQ(uart_bytes[0], (uint8_t)0xAC);
    EXPECT_EQ(uart_bytes[1], (uint8_t)0xAB);

}

TEST(DccRailcomEncoder, send_ch1_with_datagram_id) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    /* datagram_id=15, data=0x00 → combined=0xF00 → high_six_bits=0x3C, low_six_bits=0x00 */
    DccRailcomEncoder_send_ch1(15, 0x00);

    EXPECT_EQ(uart_byte_count, (uint8_t)2);

}

TEST(DccRailcomEncoder, send_ch1_null_uart_no_crash) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.uart_write = NULL;
    DccRailcomEncoder_initialize(&interface);

    DccRailcomEncoder_send_ch1(0, 0x42);
    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

// ============================================================================
// Channel 2 send tests
// ============================================================================

TEST(DccRailcomEncoder, send_ch2_single_data_byte) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.count = 1;

    DccRailcomEncoder_send_ch2(&response);

    /* 2 encoded bytes for ID + data[0] */
    EXPECT_EQ(uart_byte_count, (uint8_t)2);

}

TEST(DccRailcomEncoder, send_ch2_multiple_data_bytes) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 0;
    response.data[0] = 0x01;
    response.data[1] = 0x02;
    response.data[2] = 0x03;
    response.count = 3;

    DccRailcomEncoder_send_ch2(&response);

    /* 2 bytes for ID+data[0], then 1 byte each for data[1] and data[2] */
    EXPECT_EQ(uart_byte_count, (uint8_t)4);

}

TEST(DccRailcomEncoder, send_ch2_zero_count_no_send) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface = make_interface();
    DccRailcomEncoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.count = 0;

    DccRailcomEncoder_send_ch2(&response);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

TEST(DccRailcomEncoder, send_ch2_null_uart_no_crash) {

    reset_mocks();
    interface_dcc_railcom_encoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.uart_write = NULL;
    DccRailcomEncoder_initialize(&interface);

    dcc_railcom_response_t response;
    memset(&response, 0, sizeof(response));
    response.datagram_id = 7;
    response.data[0] = 0xFF;
    response.count = 1;

    DccRailcomEncoder_send_ch2(&response);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}
