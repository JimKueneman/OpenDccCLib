/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Decoder (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_decoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint8_t uart_buffer[16];
static uint8_t uart_buffer_count;
static uint8_t uart_read_index;

static uint16_t last_datagram_address;
static uint8_t last_datagram_channel;
static dcc_railcom_datagram_t last_datagram;
static uint32_t datagram_callback_count;

static bool mock_uart_read(uint8_t *byte) {

    if (uart_read_index >= uart_buffer_count)
        return false;

    *byte = uart_buffer[uart_read_index];
    uart_read_index++;

    return true;

}

static void mock_on_datagram(uint16_t address, uint8_t channel,
                              const dcc_railcom_datagram_t *datagram) {

    last_datagram_address = address;
    last_datagram_channel = channel;
    memcpy(&last_datagram, datagram, sizeof(dcc_railcom_datagram_t));
    datagram_callback_count++;

}

static dcc_railcom_decoder_context_t test_context;

static void reset_mocks(void) {

    memset(uart_buffer, 0, sizeof(uart_buffer));
    uart_buffer_count = 0;
    uart_read_index = 0;

    last_datagram_address = 0;
    last_datagram_channel = 0xFF;
    memset(&last_datagram, 0, sizeof(last_datagram));
    datagram_callback_count = 0;

    memset(&test_context, 0, sizeof(test_context));

}

static interface_dcc_railcom_decoder_t make_interface(void) {

    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.uart_read = mock_uart_read;
    interface.on_datagram = mock_on_datagram;

    return interface;

}

// ============================================================================
// Helper: load UART bytes for a cutout
// ============================================================================

static void load_uart_bytes(const uint8_t *bytes, uint8_t count) {

    uint8_t byte_index;

    for (byte_index = 0; byte_index < count && byte_index < 16; byte_index++) {

        uart_buffer[byte_index] = bytes[byte_index];

    }

    uart_buffer_count = count;
    uart_read_index = 0;

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccRailcomDecoder, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

}

TEST(DccRailcomDecoder, initialize_clears_buffer) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)0);

}

// ============================================================================
// 4/8 decode table tests
// ============================================================================

TEST(DccRailcomDecoder, decode_byte_invalid_0x00) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomDecoder_decode_byte(0x00), DCC_RAILCOM_DECODE_INVALID);

}

TEST(DccRailcomDecoder, decode_byte_ack_0xF0) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomDecoder_decode_byte(0xF0), DCC_RAILCOM_DECODE_ACK);

}

TEST(DccRailcomDecoder, decode_byte_ack_0x0F) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 0x0F is the alternate ACK special code word (2026 draft S-9.3.2) */
    EXPECT_EQ(DccRailcomDecoder_decode_byte(0x0F), DCC_RAILCOM_DECODE_ACK);

}

TEST(DccRailcomDecoder, decode_byte_nack_0x3C) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 0x3C is the NACK special code word (2026 draft S-9.3.2) */
    EXPECT_EQ(DccRailcomDecoder_decode_byte(0x3C), DCC_RAILCOM_DECODE_NACK);

}

TEST(DccRailcomDecoder, decode_byte_value_0x00_codeword_0xAC) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 6-bit value 0x00 maps to codeword 0xAC */
    EXPECT_EQ(DccRailcomDecoder_decode_byte(0xAC), (uint8_t)0x00);

}

TEST(DccRailcomDecoder, decode_byte_value_0x01_codeword_0xAB) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomDecoder_decode_byte(0xAB), (uint8_t)0x01);

}

TEST(DccRailcomDecoder, decode_byte_value_0x3F_codeword_0xC4) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    EXPECT_EQ(DccRailcomDecoder_decode_byte(0xC4), (uint8_t)0x3F);

}

// ============================================================================
// Channel 1 decode tests (2 bytes -> 12 bits)
// ============================================================================

TEST(DccRailcomDecoder, ch1_valid_2_bytes) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* Encode two 6-bit values: 0x00, 0x01 -> combined = 0x001 */
    /* datagram_id = (0x001 >> 8) & 0x0F = 0 */
    /* data[0] = 0x001 & 0xFF = 0x01 */
    uint8_t raw[2];
    raw[0] = 0xAC;  /* decodes to 0x00 */
    raw[1] = 0xAB;  /* decodes to 0x01 */

    load_uart_bytes(raw, 2);
    DccRailcomDecoder_begin_cutout(&test_context,42);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(last_datagram_address, (uint16_t)42);
    EXPECT_EQ(last_datagram_channel, (uint8_t)DCC_RAILCOM_CH1);
    EXPECT_TRUE(last_datagram.valid);
    EXPECT_EQ(last_datagram.datagram_id, (uint8_t)0);
    EXPECT_EQ(last_datagram.data[0], (uint8_t)0x01);
    EXPECT_EQ(last_datagram.count, (uint8_t)1);

}

TEST(DccRailcomDecoder, ch1_invalid_byte_skipped) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* First byte invalid */
    uint8_t raw[2];
    raw[0] = 0x00;  /* invalid */
    raw[1] = 0xAB;  /* valid */

    load_uart_bytes(raw, 2);
    DccRailcomDecoder_begin_cutout(&test_context,10);
    DccRailcomDecoder_run(&test_context);

    /* CH1 should not decode because first byte is invalid */
    /* No CH2 either since we only have 2 bytes total */
    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

// ============================================================================
// Channel 2 decode tests (bytes 3+ after CH1)
// ============================================================================

TEST(DccRailcomDecoder, ch2_valid_4_bytes) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 2 CH1 bytes + 4 CH2 bytes */
    uint8_t raw[6];
    raw[0] = 0xAC;  /* CH1 byte 0: decodes to 0x00 */
    raw[1] = 0xAB;  /* CH1 byte 1: decodes to 0x01 */
    raw[2] = 0xD3;  /* CH2 byte 0: decodes to 0x02 */
    raw[3] = 0xA3;  /* CH2 byte 1: decodes to 0x03 */
    raw[4] = 0xC5;  /* CH2 byte 2: decodes to 0x04 */
    raw[5] = 0xCA;  /* CH2 byte 3: decodes to 0x05 */

    load_uart_bytes(raw, 6);
    DccRailcomDecoder_begin_cutout(&test_context,100);
    DccRailcomDecoder_run(&test_context);

    /* Should get CH1 + CH2 callbacks */
    EXPECT_EQ(datagram_callback_count, (uint32_t)2);

}

TEST(DccRailcomDecoder, ch2_valid_2_bytes_minimum) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 2 CH1 bytes + 2 CH2 bytes (minimum for valid CH2) */
    uint8_t raw[4];
    raw[0] = 0xAC;  /* CH1: decodes to 0x00 */
    raw[1] = 0xAB;  /* CH1: decodes to 0x01 */
    raw[2] = 0xD3;  /* CH2: decodes to 0x02 */
    raw[3] = 0xA3;  /* CH2: decodes to 0x03 */

    load_uart_bytes(raw, 4);
    DccRailcomDecoder_begin_cutout(&test_context,200);
    DccRailcomDecoder_run(&test_context);

    /* CH1 + CH2 */
    EXPECT_EQ(datagram_callback_count, (uint32_t)2);

    /* Verify buffer has 2 entries */
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)2);

}

// ============================================================================
// Buffer tests
// ============================================================================

TEST(DccRailcomDecoder, read_returns_buffered_datagram) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* Send 2 CH1 bytes */
    uint8_t raw[2];
    raw[0] = 0xAC;
    raw[1] = 0xAB;

    load_uart_bytes(raw, 2);
    DccRailcomDecoder_begin_cutout(&test_context,42);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)1);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_TRUE(DccRailcomDecoder_read(&test_context, &read_datagram));
    EXPECT_TRUE(read_datagram.valid);
    EXPECT_EQ(read_datagram.datagram_id, (uint8_t)0);

    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)0);

}

TEST(DccRailcomDecoder, read_empty_returns_false) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_FALSE(DccRailcomDecoder_read(&test_context, &read_datagram));

}

// ============================================================================
// Run guard tests
// ============================================================================

TEST(DccRailcomDecoder, run_without_cutout_does_nothing) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* Load bytes but don't begin cutout */
    uint8_t raw[2] = {0xAC, 0xAB};
    load_uart_bytes(raw, 2);

    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

TEST(DccRailcomDecoder, run_with_null_uart_does_nothing) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.uart_read = NULL;
    DccRailcomDecoder_initialize(&test_context, &interface);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

// ============================================================================
// No UART data in cutout
// ============================================================================

TEST(DccRailcomDecoder, cutout_no_data) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* No bytes loaded */
    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)0);

}

// ============================================================================
// Address correlation
// ============================================================================

TEST(DccRailcomDecoder, cutout_address_tagged_correctly) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    uint8_t raw[2] = {0xAC, 0xAB};
    load_uart_bytes(raw, 2);

    DccRailcomDecoder_begin_cutout(&test_context,9999);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(last_datagram_address, (uint16_t)9999);

}

// ============================================================================
// Single byte (CH1 only, no CH2 — 1 byte is insufficient for CH1)
// ============================================================================

TEST(DccRailcomDecoder, single_byte_no_decode) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    uint8_t raw[1] = {0xAC};
    load_uart_bytes(raw, 1);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    /* 1 byte is not enough for CH1 (needs 2) */
    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

// ============================================================================
// 3 bytes: CH1 (2 bytes) + CH2 insufficient (1 byte)
// ============================================================================

TEST(DccRailcomDecoder, three_bytes_ch1_only) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    uint8_t raw[3] = {0xAC, 0xAB, 0xD3};
    load_uart_bytes(raw, 3);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    /* CH1 should decode (2 bytes), CH2 needs at least 2 bytes */
    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(last_datagram_channel, (uint8_t)DCC_RAILCOM_CH1);

}

// ============================================================================
// End cutout tests
// ============================================================================

TEST(DccRailcomDecoder, end_cutout_does_not_crash) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    DccRailcomDecoder_end_cutout(&test_context);

}

// ============================================================================
// Buffer overflow tests
// ============================================================================

TEST(DccRailcomDecoder, buffer_overflow_drops_oldest) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* Push 5 datagrams into depth-4 buffer — oldest should be dropped */
    uint8_t second_bytes[5] = {0xAB, 0xD3, 0xA3, 0xC5, 0xCA};
    uint8_t expected_data[4] = {0x02, 0x03, 0x04, 0x05};
    uint8_t cutout_index;
    uint8_t read_index;

    for (cutout_index = 0; cutout_index < 5; cutout_index++) {

        uint8_t raw[2];
        raw[0] = 0xAC;
        raw[1] = second_bytes[cutout_index];

        load_uart_bytes(raw, 2);
        DccRailcomDecoder_begin_cutout(&test_context,cutout_index + 1);
        DccRailcomDecoder_run(&test_context);

    }

    EXPECT_EQ(datagram_callback_count, (uint32_t)5);
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)4);

    for (read_index = 0; read_index < 4; read_index++) {

        dcc_railcom_datagram_t read_datagram;
        EXPECT_TRUE(DccRailcomDecoder_read(&test_context, &read_datagram));
        EXPECT_EQ(read_datagram.data[0], expected_data[read_index]);

    }

    dcc_railcom_datagram_t read_datagram;
    EXPECT_FALSE(DccRailcomDecoder_read(&test_context, &read_datagram));

}

// ============================================================================
// CH1 partial invalid tests
// ============================================================================

TEST(DccRailcomDecoder, ch1_first_valid_second_invalid) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* First byte valid (0xAC->0x00), second byte invalid (0x00->0xFF) */
    uint8_t raw[2] = {0xAC, 0x00};
    load_uart_bytes(raw, 2);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);

}

// ============================================================================
// CH2 invalid byte tests
// ============================================================================

TEST(DccRailcomDecoder, ch2_invalid_byte_stops_decode) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* CH1 valid, CH2 first byte invalid */
    uint8_t raw[4] = {0xAC, 0xAB, 0x00, 0xA3};
    load_uart_bytes(raw, 4);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    /* Only CH1 should decode, CH2 aborted due to invalid byte */
    EXPECT_EQ(datagram_callback_count, (uint32_t)1);
    EXPECT_EQ(last_datagram_channel, (uint8_t)DCC_RAILCOM_CH1);

}

// ============================================================================
// NULL callback tests
// ============================================================================

TEST(DccRailcomDecoder, ch1_null_on_datagram_still_buffers) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.on_datagram = NULL;
    DccRailcomDecoder_initialize(&test_context, &interface);

    uint8_t raw[2] = {0xAC, 0xAB};
    load_uart_bytes(raw, 2);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)1);

    dcc_railcom_datagram_t read_datagram;
    EXPECT_TRUE(DccRailcomDecoder_read(&test_context, &read_datagram));
    EXPECT_TRUE(read_datagram.valid);

}

TEST(DccRailcomDecoder, ch2_null_on_datagram_still_buffers) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.on_datagram = NULL;
    DccRailcomDecoder_initialize(&test_context, &interface);

    uint8_t raw[4] = {0xAC, 0xAB, 0xD3, 0xA3};
    load_uart_bytes(raw, 4);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)0);
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)2);

}

// ============================================================================
// Max UART byte limit tests
// ============================================================================

TEST(DccRailcomDecoder, max_8_uart_bytes_hits_loop_limit) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&test_context, &interface);

    /* 2 CH1 + 6 CH2 = 8 bytes (max), loop exits via condition not break */
    uint8_t raw[8] = {0xAC, 0xAB, 0xD3, 0xA3, 0xC5, 0xCA, 0xB3, 0x95};
    load_uart_bytes(raw, 8);

    DccRailcomDecoder_begin_cutout(&test_context,1);
    DccRailcomDecoder_run(&test_context);

    EXPECT_EQ(datagram_callback_count, (uint32_t)2);
    EXPECT_EQ(DccRailcomDecoder_available(&test_context), (uint8_t)2);

}
