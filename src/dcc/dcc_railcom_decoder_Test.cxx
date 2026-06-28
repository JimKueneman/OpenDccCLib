/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC RailCom Encoder (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_decoder.h"
#include "dcc/dcc_railcom_utilities.h"
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

// ============================================================================
// Pure recognizer: DccRailcomDecoder_packet_length (no init needed)
// ============================================================================

TEST(DccRailcomDecoder, packet_length_short_addr_speed) {

    /* short addr 0x03 + speed/dir (01xxxxxx) = 1 instr byte + XOR = 3 */
    uint8_t data[] = { 0x03, 0x60 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 2), (uint8_t)3);

}

TEST(DccRailcomDecoder, packet_length_short_addr_pom_write) {

    /* short addr + POM long-form write (0xEC + CV-low + data) = 3 instr + XOR = 5 */
    uint8_t data[] = { 0x03, 0xEC, 0x00, 0x05 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 4), (uint8_t)5);

}

TEST(DccRailcomDecoder, packet_length_short_form_cv) {

    /* short addr + short-form CV access (1111GGGG) = 2 instr + XOR = 4 */
    uint8_t data[] = { 0x03, 0xF5, 0x00 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 3), (uint8_t)4);

}

TEST(DccRailcomDecoder, packet_length_long_addr_speed) {

    /* long addr (2 bytes) + speed = 2 + 1 + XOR = 4 */
    uint8_t data[] = { 0xC1, 0x00, 0x60 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 3), (uint8_t)4);

}

TEST(DccRailcomDecoder, packet_length_func_group1) {

    uint8_t data[] = { 0x03, 0x80 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 2), (uint8_t)3);

}

TEST(DccRailcomDecoder, packet_length_consist) {

    /* consist control 0x12 + consist address = 2 instr + XOR = 4 */
    uint8_t data[] = { 0x03, 0x12, 0x05 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 3), (uint8_t)4);

}

TEST(DccRailcomDecoder, packet_length_analog_function) {

    /* advanced ops analog 0x3D + output + data = 3 instr + XOR = 5 */
    uint8_t data[] = { 0x03, 0x3D, 0x01, 0x80 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 4), (uint8_t)5);

}

TEST(DccRailcomDecoder, packet_length_incomplete_returns_zero) {

    /* only the address byte: opcode not yet received */
    uint8_t data[] = { 0x03 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 1), (uint8_t)0);

    /* long address byte 0 only: still need byte 1 + opcode */
    uint8_t longp[] = { 0xC1, 0x00 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(longp, 2), (uint8_t)0);

}

TEST(DccRailcomDecoder, packet_length_xpom_not_sized) {

    /* 0xE0 family that is not standard POM (XPOM 1110GGSS) -> 0, not a guess */
    uint8_t data[] = { 0x03, 0xE0, 0x00, 0x00, 0x00 };
    EXPECT_EQ(DccRailcomDecoder_packet_length(data, 5), (uint8_t)0);

}

TEST(DccRailcomDecoder, packet_length_accessory_and_reserved_zero) {

    uint8_t acc[] = { 0x90, 0x01 };       /* accessory leading byte */
    EXPECT_EQ(DccRailcomDecoder_packet_length(acc, 2), (uint8_t)0);

    uint8_t rsv[] = { 0xE8, 0x00 };       /* reserved leading byte */
    EXPECT_EQ(DccRailcomDecoder_packet_length(rsv, 2), (uint8_t)0);

}

// ============================================================================
// Pure recognizer: DccRailcomDecoder_packet_address
// ============================================================================

TEST(DccRailcomDecoder, packet_address_short) {

    uint8_t data[] = { 0x03, 0x60 };
    dcc_address_t address = 0xFFFF;
    dcc_address_type_enum type = DCC_ADDRESS_IDLE;
    EXPECT_TRUE(DccRailcomDecoder_packet_address(data, 2, &address, &type));
    EXPECT_EQ(address, (dcc_address_t)3);
    EXPECT_EQ(type, DCC_ADDRESS_SHORT);

}

TEST(DccRailcomDecoder, packet_address_long) {

    /* 0xC1 0x2A -> ((0x01)<<8)|0x2A = 0x012A = 298 */
    uint8_t data[] = { 0xC1, 0x2A, 0x60 };
    dcc_address_t address = 0;
    dcc_address_type_enum type = DCC_ADDRESS_SHORT;
    EXPECT_TRUE(DccRailcomDecoder_packet_address(data, 3, &address, &type));
    EXPECT_EQ(address, (dcc_address_t)298);
    EXPECT_EQ(type, DCC_ADDRESS_LONG);

}

TEST(DccRailcomDecoder, packet_address_broadcast_and_idle) {

    dcc_address_t address = 0xFFFF;
    dcc_address_type_enum type = DCC_ADDRESS_SHORT;

    uint8_t bc[] = { 0x00, 0x00 };
    EXPECT_TRUE(DccRailcomDecoder_packet_address(bc, 2, &address, &type));
    EXPECT_EQ(type, DCC_ADDRESS_BROADCAST);

    uint8_t idle[] = { 0xFF, 0x00 };
    EXPECT_TRUE(DccRailcomDecoder_packet_address(idle, 2, &address, &type));
    EXPECT_EQ(type, DCC_ADDRESS_IDLE);

}

TEST(DccRailcomDecoder, packet_address_rejects_accessory_and_incomplete) {

    dcc_address_t address = 0;
    dcc_address_type_enum type = DCC_ADDRESS_SHORT;

    uint8_t acc[] = { 0x90, 0x01 };       /* accessory leading byte */
    EXPECT_FALSE(DccRailcomDecoder_packet_address(acc, 2, &address, &type));

    uint8_t longp[] = { 0xC1 };           /* long address, only 1 byte present */
    EXPECT_FALSE(DccRailcomDecoder_packet_address(longp, 1, &address, &type));

}

// ============================================================================
// Dispatch: recognize addressed command before XOR, answer with ADR (Ch1)
// ============================================================================

TEST(DccRailcomDecoder, dispatch_adr_on_addressed_packet) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    /* short addr 0x03 + speed 0x60, XOR = 0x03 ^ 0x60 = 0x63 */
    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);   /* last data byte -> recognize + arm */
    DccRailcomDecoder_on_byte_received(data, 3);   /* XOR byte -> validate + transmit ADR1 */

    EXPECT_EQ(uart_byte_count, (uint8_t)2);        /* ADR is one Channel 1 datagram (2 bytes) */

}

TEST(DccRailcomDecoder, dispatch_no_response_when_not_addressed) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(7, DCC_ADDRESS_SHORT);   /* not address 3 */

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_on_byte_received(data, 3);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

TEST(DccRailcomDecoder, dispatch_no_response_on_bad_xor) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    /* correct XOR would be 0x63; feed a corrupt one */
    uint8_t data[] = { 0x03, 0x60, 0xFF };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_on_byte_received(data, 3);

    EXPECT_EQ(uart_byte_count, (uint8_t)0);

}

TEST(DccRailcomDecoder, dispatch_adr_alternates_high_then_low) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    uint8_t data[] = { 0x03, 0x60, 0x63 };

    /* packet 1 -> ADR1 (HIGH) */
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_on_byte_received(data, 3);

    /* packet 2 -> ADR2 (LOW) */
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_on_byte_received(data, 3);

    EXPECT_EQ(uart_byte_count, (uint8_t)4);
    /* ADR1 and ADR2 encode different datagram IDs -> first byte differs */
    EXPECT_NE(uart_bytes[0], uart_bytes[2]);

}
