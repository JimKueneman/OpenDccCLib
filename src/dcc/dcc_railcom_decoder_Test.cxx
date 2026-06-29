/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for the DCC RailCom decoder-side Tx engine.
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_railcom_decoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

#define MAX_TX_LEVELS 256

static bool tx_levels[MAX_TX_LEVELS];
static uint16_t tx_level_count;
static uint32_t lock_count;
static uint32_t unlock_count;

static void mock_tx_pin_set(bool level) {

    if (tx_level_count < MAX_TX_LEVELS) {

        tx_levels[tx_level_count] = level;
        tx_level_count++;

    }

}

static void mock_delay_us(uint16_t us) {

    /* No-op: the busy-wait is the app's job. The unit test checks the pin-level sequence,
     * not real timing (timing is verified on the HIL bench). */
    (void)us;

}

static void mock_lock(void) { lock_count++; }
static void mock_unlock(void) { unlock_count++; }

static dcc_railcom_reply_status_enum mock_request_status;
static dcc_railcom_response_t mock_request_response;
static uint32_t request_callback_count;

static dcc_railcom_reply_status_enum mock_on_railcom_request(const uint8_t *instruction,
        uint8_t instruction_count, dcc_railcom_response_t *out) {

    (void)instruction;
    (void)instruction_count;
    request_callback_count++;
    *out = mock_request_response;
    return mock_request_status;

}

static void reset_mocks(void) {

    memset(tx_levels, 0, sizeof(tx_levels));
    tx_level_count = 0;
    lock_count = 0;
    unlock_count = 0;
    mock_request_status = DCC_RAILCOM_REPLY_NONE;
    memset(&mock_request_response, 0, sizeof(mock_request_response));
    request_callback_count = 0;

}

// Reconstruct the transmitted bytes from the bit-banged pin levels. The Tx clocks
// [idle "1"], then per byte a UART frame (start "0", 8 data LSB-first, stop "1"),
// then a trailing idle "1". So byte count = (levels - 2) / 10.
static uint8_t decode_tx_frames(uint8_t *out, uint8_t max) {

    if (tx_level_count < 2) {

        return 0;

    }

    uint8_t n = (uint8_t)((tx_level_count - 2) / 10);

    for (uint8_t i = 0; i < n && i < max; i++) {

        uint16_t base = (uint16_t)(1 + i * 10);   /* skip leading idle; base = start bit */
        uint8_t b = 0;

        for (uint8_t bit = 0; bit < 8; bit++) {

            if (tx_levels[base + 1 + bit]) {

                b = (uint8_t)(b | (1 << bit));

            }

        }

        out[i] = b;

    }

    return n;

}

static interface_dcc_railcom_decoder_t make_interface(void) {

    interface_dcc_railcom_decoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.tx_pin_set = mock_tx_pin_set;
    interface.delay_us = mock_delay_us;
    interface.lock_shared_resources = mock_lock;
    interface.unlock_shared_resources = mock_unlock;

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
// Tx engine: recognize an addressed command before XOR, answer with ADR (Ch1)
// in the cutout at the end-bit edge (DccRailcomDecoder_transmit)
// ============================================================================

TEST(DccRailcomDecoder, transmit_adr_on_addressed_packet) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    /* short addr 0x03 + speed 0x60, XOR = 0x03 ^ 0x60 = 0x63 */
    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);   /* last data byte -> recognize + arm */
    DccRailcomDecoder_on_byte_received(data, 3);   /* XOR byte (no-op; already armed) */
    DccRailcomDecoder_transmit(data, 3);           /* end-bit edge -> bit-bang ADR */

    uint8_t frames[16];
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)2);   /* ADR is one Ch1 datagram (2 bytes) */

}

TEST(DccRailcomDecoder, transmit_brackets_with_lock) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    /* The cutout bit-bang is bracketed by the shared-resource lock, which also masks the
     * DCC edge IRQ so the decoder's self-edges cannot fire during the bit timing. */
    EXPECT_EQ(lock_count, (uint32_t)1);
    EXPECT_EQ(unlock_count, (uint32_t)1);

}

TEST(DccRailcomDecoder, transmit_no_response_when_not_addressed) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(7, DCC_ADDRESS_SHORT);   /* not address 3 */

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    uint8_t frames[16];
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)0);
    EXPECT_EQ(lock_count, (uint32_t)0);            /* never entered the cutout */

}

TEST(DccRailcomDecoder, transmit_no_response_on_bad_xor) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    /* correct XOR would be 0x63; feed a corrupt one */
    uint8_t data[] = { 0x03, 0x60, 0xFF };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    uint8_t frames[16];
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)0);

}

TEST(DccRailcomDecoder, transmit_adr_alternates_high_then_low) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    uint8_t data[] = { 0x03, 0x60, 0x63 };

    /* packet 1 -> ADR1 (HIGH) */
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);
    uint8_t first[16];
    uint8_t n1 = decode_tx_frames(first, 16);

    /* packet 2 -> ADR2 (LOW); reset only the capture, not the engine's alternation state */
    reset_mocks();
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);
    uint8_t second[16];
    uint8_t n2 = decode_tx_frames(second, 16);

    EXPECT_EQ(n1, (uint8_t)2);
    EXPECT_EQ(n2, (uint8_t)2);
    /* ADR1 and ADR2 encode different datagram IDs -> first byte differs */
    EXPECT_NE(first[0], second[0]);

}

// ============================================================================
// Tx engine: Channel 2 from the app's on_railcom_request reply
// ============================================================================

TEST(DccRailcomDecoder, transmit_ch2_data_after_adr) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.on_railcom_request = mock_on_railcom_request;
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    /* app answers with a 1-byte Ch2 datagram -> 2 encoded bytes */
    mock_request_status = DCC_RAILCOM_REPLY_DATA;
    mock_request_response.datagram_id = 0;
    mock_request_response.data[0] = 0x01;
    mock_request_response.count = 1;

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    uint8_t frames[16];
    EXPECT_EQ(request_callback_count, (uint32_t)1);   /* fired once, before the XOR */
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)4);   /* 2 ADR (Ch1) + 2 Ch2 */

}

TEST(DccRailcomDecoder, transmit_ch2_none_only_adr) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.on_railcom_request = mock_on_railcom_request;
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    mock_request_status = DCC_RAILCOM_REPLY_NONE;

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    uint8_t frames[16];
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)2);   /* ADR only */

}

TEST(DccRailcomDecoder, transmit_ch2_ack_token) {

    reset_mocks();
    interface_dcc_railcom_decoder_t interface = make_interface();
    interface.on_railcom_request = mock_on_railcom_request;
    DccRailcomDecoder_initialize(&interface);
    DccRailcomDecoder_set_address(3, DCC_ADDRESS_SHORT);

    mock_request_status = DCC_RAILCOM_REPLY_ACK;

    uint8_t data[] = { 0x03, 0x60, 0x63 };
    DccRailcomDecoder_on_byte_received(data, 1);
    DccRailcomDecoder_on_byte_received(data, 2);
    DccRailcomDecoder_transmit(data, 3);

    uint8_t frames[16];
    EXPECT_EQ(decode_tx_frames(frames, 16), (uint8_t)3);   /* 2 ADR + 1 raw token */
    EXPECT_EQ(frames[2], (uint8_t)DCC_RAILCOM_CODE_WORD_ACK);

}
