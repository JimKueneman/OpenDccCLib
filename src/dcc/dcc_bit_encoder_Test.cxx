/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Bit Encoder (Phase 2)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_bit_encoder.h"
#include "dcc/dcc_application_command_station_packet.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static bool cutout_begin_called = false;
static bool packet_complete_called = false;

static void mock_railcom_cutout_begin(void) {
    cutout_begin_called = true;
}

static void mock_on_packet_complete(void) {
    packet_complete_called = true;
}

static void reset_mocks(void) {
    cutout_begin_called = false;
    packet_complete_called = false;
}

// ============================================================================
// Tick ISR tests (fixed-period shared-timer architecture)
// ============================================================================

static uint32_t toggle_call_count = 0;

/* Record of all pin_toggle calls for waveform analysis.
 * Each entry is the tick number at which pin_toggle was called. */
#define MAX_TOGGLE_LOG 512
static uint32_t toggle_log[MAX_TOGGLE_LOG];
static uint32_t toggle_log_count = 0;
static uint32_t tick_number = 0;

static void mock_pin_toggle(void) {

    toggle_call_count++;
    if (toggle_log_count < MAX_TOGGLE_LOG) {

        toggle_log[toggle_log_count++] = tick_number;

    }

}

static void reset_tick_mocks(void) {

    reset_mocks();
    toggle_call_count = 0;
    toggle_log_count = 0;
    tick_number = 0;

}

static interface_dcc_bit_encoder_t make_tick_interface(bool with_railcom) {

    interface_dcc_bit_encoder_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.pin_toggle = mock_pin_toggle;
    interface.on_packet_complete = mock_on_packet_complete;

    if (with_railcom) {

        interface.railcom_cutout_begin = mock_railcom_cutout_begin;

    }

    return interface;

}

/**
 * @brief Pump the tick ISR for N ticks, tracking tick numbers.
 *
 * Mirrors the real ISR flow: check toggle_next and fire pin_toggle
 * before calling DccBitEncoder_tick_isr(), just as
 * DccConfig_58us_timer_isr() does.
 */
static void pump_tick_isr(dcc_bit_encoder_context_t *context, uint32_t n) {

    uint32_t tick_index;

    for (tick_index = 0; tick_index < n; tick_index++) {

        if (context->toggle_next && context->interface->pin_toggle) {

            context->interface->pin_toggle();

        }

        DccBitEncoder_tick_isr(context);
        tick_number++;

    }

}

TEST(DccBitEncoder, tick_idle_toggles_every_tick) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    /* Pump 10 ticks while idle — should toggle on every tick (one-bits) */
    pump_tick_isr(&context, 10);

    EXPECT_EQ(toggle_call_count, (uint32_t)10);

}

TEST(DccBitEncoder, tick_transmits_idle_packet) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    /* Pump enough ticks: each one-bit = 2 ticks, each zero-bit = 4 ticks.
     * Worst case all zeros in 3 bytes = ~200 ticks. Use 400 for margin. */
    pump_tick_isr(&context, 400);

    EXPECT_TRUE(packet_complete_called);
    EXPECT_TRUE(DccBitEncoder_is_idle(&context));

}

// @compliance DCC-S9.1-CS-001, DCC-S9.1-CS-003
TEST(DccBitEncoder, tick_preamble_is_all_one_bits) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    /* Idle packet: 16-bit preamble (DCC_PREAMBLE_BITS_OPS).
     * IDLE→PREAMBLE transition = 1 one-bit (2 ticks), then 16 preamble
     * one-bits count down = 32 more ticks. Total = 34 ticks before start bit.
     * Pump 28 ticks to stay well within the preamble (all one-bits). */
    pump_tick_isr(&context, 28);

    /* Every tick should have a toggle (all one-bits) */
    EXPECT_EQ(toggle_call_count, (uint32_t)28);

    /* Verify toggles are on consecutive ticks (one per tick = one-bit) */
    uint32_t toggle_index;
    for (toggle_index = 0; toggle_index < toggle_log_count; toggle_index++) {

        EXPECT_EQ(toggle_log[toggle_index], toggle_index)
            << "toggle " << toggle_index;

    }

}

// @compliance DCC-S9.1-CS-002
TEST(DccBitEncoder, tick_zero_bit_takes_four_ticks) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    /* Idle packet: 16-bit preamble (DCC_PREAMBLE_BITS_OPS).
     * The IDLE→PREAMBLE transition itself produces 1 one-bit, then
     * preamble_count counts down from 16 producing 16 more one-bits.
     * Total one-bits before start bit: 17 = 34 ticks.
     * Start bit (zero-bit) = 4 ticks. Pump 38 ticks total. */
    pump_tick_isr(&context, 38);

    /* 34 preamble toggles + 2 start bit toggles = 36 total */
    EXPECT_EQ(toggle_call_count, (uint32_t)36);

    /* Verify the start bit toggles are spaced 2 ticks apart */
    ASSERT_GE(toggle_log_count, (uint32_t)36);
    uint32_t first_zero_toggle = toggle_log[34];   /* first half of start bit */
    uint32_t second_zero_toggle = toggle_log[35];   /* second half of start bit */
    EXPECT_EQ(second_zero_toggle - first_zero_toggle, (uint32_t)2);

}

// @compliance DCC-Library-CS-004
TEST(DccBitEncoder, tick_packet_complete_without_railcom) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    pump_tick_isr(&context, 400);

    EXPECT_TRUE(packet_complete_called);
    EXPECT_FALSE(cutout_begin_called);

}

TEST(DccBitEncoder, tick_railcom_cutout_arms_timer_continuously) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(true);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    /* Continuous-clock model: the end bit ARMS the cutout timer (cutout_begin) AND
     * completes the packet immediately -- no stall, no RAILCOM_CUTOUT state. The
     * cutout fires whenever the output stage is RailCom-capable (begin hook wired);
     * there is no runtime enable gate -- the application/hardware decides whether to
     * act on the cutout-active strobe. The encoder keeps clocking either way. */
    pump_tick_isr(&context, 400);

    EXPECT_TRUE(cutout_begin_called);     /* cutout timer armed at the end bit */
    EXPECT_TRUE(packet_complete_called);  /* completed immediately, did NOT stall */

    /* Bit stream never pauses: encoder keeps producing toggles after the end bit. */
    uint32_t toggles_before = toggle_call_count;
    pump_tick_isr(&context, 4);
    EXPECT_GT(toggle_call_count, toggles_before);

}

TEST(DccBitEncoder, tick_no_crash_when_not_running) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);

    /* Don't call start() */
    pump_tick_isr(&context, 10);
    EXPECT_EQ(toggle_call_count, (uint32_t)0);

}

// @compliance DCC-Library-CS-004
TEST(DccBitEncoder, tick_consecutive_packets) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);

    /* First packet */
    DccBitEncoder_load_packet(&context, &pkt);
    pump_tick_isr(&context, 400);
    EXPECT_TRUE(packet_complete_called);

    /* Second packet */
    packet_complete_called = false;
    DccBitEncoder_load_packet(&context, &pkt);
    pump_tick_isr(&context, 400);
    EXPECT_TRUE(packet_complete_called);

}

TEST(DccBitEncoder, tick_speed_packet_long_addr_completes) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(false);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 1234, DCC_ADDRESS_LONG, 100, true);
    DccBitEncoder_load_packet(&context, &pkt);

    pump_tick_isr(&context, 600);
    EXPECT_TRUE(packet_complete_called);

}

TEST(DccBitEncoder, tick_isr_with_null_interface_does_nothing) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    DccBitEncoder_initialize(&context, NULL);
    DccBitEncoder_start(&context);

    DccBitEncoder_tick_isr(&context);

    EXPECT_EQ(toggle_call_count, (uint32_t)0);

}

TEST(DccBitEncoder, tick_railcom_continuous_through_cutout_window) {

    reset_tick_mocks();
    dcc_bit_encoder_context_t context;
    interface_dcc_bit_encoder_t interface = make_tick_interface(true);
    DccBitEncoder_initialize(&context, &interface);
    DccBitEncoder_start(&context);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    DccBitEncoder_load_packet(&context, &pkt);

    /* Run past the end bit (arms the cutout timer). */
    pump_tick_isr(&context, 400);
    EXPECT_TRUE(cutout_begin_called);

    /* Continuous clock: across the whole cutout window (~8 bit-times) the encoder
     * keeps toggling -- the bit stream never pauses. The DRIVER blanks its own
     * output between begin/end; the library no longer stalls the clock. */
    uint32_t toggles_before = toggle_call_count;
    pump_tick_isr(&context, 50);
    EXPECT_GT(toggle_call_count, toggles_before);

}
