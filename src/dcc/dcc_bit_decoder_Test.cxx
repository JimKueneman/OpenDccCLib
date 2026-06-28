/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Bit Decoder (Phase 5)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_bit_decoder.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

#define MAX_PACKET_BYTES 16

static uint8_t received_packet[MAX_PACKET_BYTES];
static uint8_t received_byte_count;
static uint32_t packet_callback_count;

static void mock_on_packet_received(const uint8_t *data, uint8_t byte_count) {

    uint8_t byte_index;

    received_byte_count = byte_count;
    packet_callback_count++;

    for (byte_index = 0; byte_index < byte_count && byte_index < MAX_PACKET_BYTES; byte_index++) {

        received_packet[byte_index] = data[byte_index];

    }

}

#if defined(DCC_COMPILE_RAILCOM)
static uint8_t byte_received_counts[MAX_PACKET_BYTES];
static uint32_t byte_received_call_count;
static uint8_t byte_received_snapshot[MAX_PACKET_BYTES];   /* buffer contents at the last call */

static void mock_on_byte_received(const uint8_t *data, uint8_t byte_count) {

    uint8_t byte_index;

    if (byte_received_call_count < MAX_PACKET_BYTES) {

        byte_received_counts[byte_received_call_count] = byte_count;

    }

    for (byte_index = 0; byte_index < byte_count && byte_index < MAX_PACKET_BYTES; byte_index++) {

        byte_received_snapshot[byte_index] = data[byte_index];

    }

    byte_received_call_count++;

}
#endif /* DCC_COMPILE_RAILCOM */

static void reset_mocks(void) {

    memset(received_packet, 0, sizeof(received_packet));
    received_byte_count = 0;
    packet_callback_count = 0;
#if defined(DCC_COMPILE_RAILCOM)
    memset(byte_received_counts, 0, sizeof(byte_received_counts));
    byte_received_call_count = 0;
    memset(byte_received_snapshot, 0, sizeof(byte_received_snapshot));
#endif

}

static interface_dcc_bit_decoder_t make_interface(void) {

    interface_dcc_bit_decoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.on_packet_received = mock_on_packet_received;
#if defined(DCC_COMPILE_RAILCOM)
    interface.on_byte_received = mock_on_byte_received;
#endif

    return interface;

}

// ============================================================================
// Helpers: generate edge timestamps for one/zero bits
// ============================================================================

static uint32_t current_time;

    /**
     * @brief Feed a single one-bit (two short half-periods of 58 µs each).
     */
static void feed_one_bit(void) {

    current_time += 58;
    DccBitDecoder_edge(current_time);
    current_time += 58;
    DccBitDecoder_edge(current_time);

}

    /**
     * @brief Feed a single zero-bit (two long half-periods of 100 µs each).
     */
static void feed_zero_bit(void) {

    current_time += 100;
    DccBitDecoder_edge(current_time);
    current_time += 100;
    DccBitDecoder_edge(current_time);

}

    /**
     * @brief Feed a preamble of the specified number of one-bits.
     */
static void feed_preamble(uint8_t count) {

    uint8_t bit_index;

    for (bit_index = 0; bit_index < count; bit_index++) {

        feed_one_bit();

    }

}

    /**
     * @brief Feed 8 data bits (MSB first) for one byte.
     */
static void feed_byte(uint8_t value) {

    int8_t bit_index;

    for (bit_index = 7; bit_index >= 0; bit_index--) {

        if (value & (1 << bit_index)) {

            feed_one_bit();

        } else {

            feed_zero_bit();

        }

    }

}

    /**
     * @brief Feed a complete simple packet: preamble + [byte0] 0 [byte1] 1
     *
     * @details This feeds a 2-byte packet (data + XOR) with proper framing:
     * preamble, start bit (0), byte0, separator (0), byte1, end bit (1).
     */
static void feed_simple_packet(uint8_t byte0, uint8_t byte1) {

    feed_preamble(12);
    feed_zero_bit();    /* start bit */
    feed_byte(byte0);
    feed_zero_bit();    /* separator */
    feed_byte(byte1);
    feed_one_bit();     /* end bit */

}

    /**
     * @brief Feed a 3-byte packet: addr + data + XOR.
     */
static void feed_3byte_packet(uint8_t addr, uint8_t data, uint8_t xor_byte) {

    feed_preamble(12);
    feed_zero_bit();    /* start bit */
    feed_byte(addr);
    feed_zero_bit();    /* separator */
    feed_byte(data);
    feed_zero_bit();    /* separator */
    feed_byte(xor_byte);
    feed_one_bit();     /* end bit */

}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccBitDecoder, initialize_does_not_crash) {

    reset_mocks();
    current_time = 0;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

}

// ============================================================================
// Basic bit classification tests
// ============================================================================

TEST(DccBitDecoder, first_edge_ignored) {

    reset_mocks();
    current_time = 0;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    /* First edge establishes baseline — no callback */
    DccBitDecoder_edge(1000);
    EXPECT_EQ(packet_callback_count, (uint32_t)0);

}

// ============================================================================
// Preamble detection
// ============================================================================

// @compliance DCC-S9.1-DEC-002
TEST(DccBitDecoder, preamble_too_short_no_packet) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    /* Feed initial edge */
    DccBitDecoder_edge(current_time);

    /* Only 9 preamble bits (need 10 minimum) */
    feed_preamble(9);
    feed_zero_bit();    /* start bit — but preamble too short */
    feed_byte(0xAA);
    feed_zero_bit();
    feed_byte(0xAA);
    feed_one_bit();     /* end bit */

    EXPECT_EQ(packet_callback_count, (uint32_t)0);

}

// @compliance DCC-S9.1-DEC-002
TEST(DccBitDecoder, preamble_exactly_10_produces_packet) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    feed_preamble(10);
    feed_zero_bit();    /* start bit */
    feed_byte(0x03);
    feed_zero_bit();    /* separator */
    feed_byte(0x03);    /* XOR */
    feed_one_bit();     /* end bit */

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_byte_count, (uint8_t)2);
    EXPECT_EQ(received_packet[0], (uint8_t)0x03);
    EXPECT_EQ(received_packet[1], (uint8_t)0x03);

}

// ============================================================================
// Packet assembly
// ============================================================================

TEST(DccBitDecoder, two_byte_packet) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);
    feed_simple_packet(0xFF, 0xFF);

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_byte_count, (uint8_t)2);
    EXPECT_EQ(received_packet[0], (uint8_t)0xFF);
    EXPECT_EQ(received_packet[1], (uint8_t)0xFF);

}

TEST(DccBitDecoder, three_byte_packet) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);
    feed_3byte_packet(0x03, 0x60, 0x63);

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_byte_count, (uint8_t)3);
    EXPECT_EQ(received_packet[0], (uint8_t)0x03);
    EXPECT_EQ(received_packet[1], (uint8_t)0x60);
    EXPECT_EQ(received_packet[2], (uint8_t)0x63);

}

TEST(DccBitDecoder, single_byte_packet_discarded) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    feed_preamble(12);
    feed_zero_bit();
    feed_byte(0xAA);
    feed_one_bit();     /* end bit after only 1 byte */

    /* Minimum packet size is 2 bytes */
    EXPECT_EQ(packet_callback_count, (uint32_t)0);

}

// ============================================================================
// Consecutive packets
// ============================================================================

TEST(DccBitDecoder, two_consecutive_packets) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    /* First packet */
    feed_3byte_packet(0x03, 0x60, 0x63);
    EXPECT_EQ(packet_callback_count, (uint32_t)1);

    /* End bit of first packet counts as first preamble bit of next.
     * So we need 11 more (total 12 with the end bit = 1). */
    feed_preamble(11);
    feed_zero_bit();
    feed_byte(0xFF);
    feed_zero_bit();
    feed_byte(0x00);
    feed_zero_bit();
    feed_byte(0xFF);
    feed_one_bit();

    EXPECT_EQ(packet_callback_count, (uint32_t)2);
    EXPECT_EQ(received_packet[0], (uint8_t)0xFF);
    EXPECT_EQ(received_packet[1], (uint8_t)0x00);
    EXPECT_EQ(received_packet[2], (uint8_t)0xFF);

}

// ============================================================================
// Half-bit mismatch recovery
// ============================================================================

TEST(DccBitDecoder, mismatched_halves_recovers) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    /* Feed one short half followed by one long half (mismatch) */
    current_time += 58;
    DccBitDecoder_edge(current_time);
    current_time += 100;
    DccBitDecoder_edge(current_time);

    /* Now feed a valid packet — should still work */
    feed_simple_packet(0xAA, 0xAA);

    EXPECT_EQ(packet_callback_count, (uint32_t)1);

}

// ============================================================================
// Too-long half period resets
// ============================================================================

// @compliance DCC-S9.1-DEC-001
TEST(DccBitDecoder, too_long_period_resets) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    /* Start a preamble */
    feed_preamble(8);

    /* Feed an edge with > 10000 µs gap — should reset */
    current_time += 11000;
    DccBitDecoder_edge(current_time);

    /* Now the prior preamble bits are lost; need a full new preamble */
    feed_preamble(12);
    feed_zero_bit();
    feed_byte(0x55);
    feed_zero_bit();
    feed_byte(0x55);
    feed_one_bit();

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_packet[0], (uint8_t)0x55);

}

// ============================================================================
// Null callback safety
// ============================================================================

TEST(DccBitDecoder, null_callback_no_crash) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface;
    memset(&interface, 0, sizeof(interface));
    interface.on_packet_received = NULL;
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);
    feed_simple_packet(0xFF, 0xFF);

    /* No crash, no callback */
    EXPECT_EQ(packet_callback_count, (uint32_t)0);

}

// ============================================================================
// Asymmetric zero-bit (stretched first half)
// ============================================================================

// @compliance DCC-S9.1-DEC-003, DCC-S9.1-ACC-001
TEST(DccBitDecoder, asymmetric_zero_accepted) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);
    feed_preamble(12);

    /* Start bit as asymmetric zero: first half 150µs, second half 100µs */
    /* Both are >= 80µs threshold so both classify as LONG = zero */
    current_time += 150;
    DccBitDecoder_edge(current_time);
    current_time += 100;
    DccBitDecoder_edge(current_time);

    feed_byte(0x03);
    feed_zero_bit();
    feed_byte(0x03);
    feed_one_bit();

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_packet[0], (uint8_t)0x03);

}

// ============================================================================
// Idle packet round-trip (0xFF 0x00 0xFF)
// ============================================================================

TEST(DccBitDecoder, idle_packet) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);
    feed_3byte_packet(0xFF, 0x00, 0xFF);

    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_byte_count, (uint8_t)3);
    EXPECT_EQ(received_packet[0], (uint8_t)0xFF);
    EXPECT_EQ(received_packet[1], (uint8_t)0x00);
    EXPECT_EQ(received_packet[2], (uint8_t)0xFF);

}

// ============================================================================
// Packet too long (exceeds DCC_PACKET_MAX_BYTES = 6)
// ============================================================================

TEST(DccBitDecoder, packet_too_long_discarded) {

    reset_mocks();
    current_time = 1000;
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    DccBitDecoder_edge(current_time);

    /* Feed 7 bytes: 6 fill the buffer, the 7th triggers overflow.
     * After byte 6 is accumulated (byte_count == 6 == DCC_PACKET_MAX_BYTES),
     * the next zero separator hits the "packet too long" branch (line 166)
     * and resets to preamble search. */
    feed_preamble(12);
    feed_zero_bit();    /* start bit */
    feed_byte(0x01);    /* byte 0 */
    feed_zero_bit();    /* separator */
    feed_byte(0x02);    /* byte 1 */
    feed_zero_bit();    /* separator */
    feed_byte(0x03);    /* byte 2 */
    feed_zero_bit();    /* separator */
    feed_byte(0x04);    /* byte 3 */
    feed_zero_bit();    /* separator */
    feed_byte(0x05);    /* byte 4 */
    feed_zero_bit();    /* separator */
    feed_byte(0x06);    /* byte 5 — byte_count is now 6 (DCC_PACKET_MAX_BYTES) */
    feed_zero_bit();    /* separator — triggers "packet too long", resets */

    /* The packet was discarded, no callback */
    EXPECT_EQ(packet_callback_count, (uint32_t)0);

    /* A valid packet after the reset should still work */
    feed_simple_packet(0xAA, 0xAA);
    EXPECT_EQ(packet_callback_count, (uint32_t)1);
    EXPECT_EQ(received_packet[0], (uint8_t)0xAA);

}

#if defined(DCC_COMPILE_RAILCOM)
// ============================================================================
// RailCom Tx foundation: on_byte_received fires per byte, before the XOR byte
// ============================================================================

TEST(DccBitDecoder, on_byte_received_fires_each_byte_before_xor) {

    reset_mocks();
    interface_dcc_bit_decoder_t interface = make_interface();
    DccBitDecoder_initialize(&interface);

    /* 3-byte packet (addr, data, XOR), fed by hand so we can inspect state mid-packet. */
    feed_preamble(12);
    feed_zero_bit();             /* start bit of byte 1 */
    feed_byte(0x03);             /* byte 1 (address) */
    EXPECT_EQ(byte_received_call_count, (uint32_t)1);
    EXPECT_EQ(byte_received_counts[0], (uint8_t)1);
    EXPECT_EQ(packet_callback_count, (uint32_t)0);    /* no packet yet */

    feed_zero_bit();             /* separator */
    feed_byte(0x3F);             /* byte 2 (last DATA byte) */
    EXPECT_EQ(byte_received_call_count, (uint32_t)2);
    EXPECT_EQ(byte_received_counts[1], (uint8_t)2);
    EXPECT_EQ(byte_received_snapshot[1], (uint8_t)0x3F);
    EXPECT_EQ(packet_callback_count, (uint32_t)0);    /* fired BEFORE the XOR byte */

    feed_zero_bit();             /* separator */
    feed_byte(0x3C);             /* byte 3 (XOR) */
    EXPECT_EQ(byte_received_call_count, (uint32_t)3);
    EXPECT_EQ(byte_received_counts[2], (uint8_t)3);

    feed_one_bit();              /* end bit -> packet complete */
    EXPECT_EQ(packet_callback_count, (uint32_t)1);    /* packet fires only now */

}
#endif /* DCC_COMPILE_RAILCOM */
