/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Scheduler (Phase 2)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_scheduler.h"
#include "dcc/dcc_application_command_station_packet.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_packet_t last_loaded_packet;
static uint32_t load_packet_count = 0;
static bool encoder_idle = true;
static uint32_t on_packet_sent_count = 0;
static dcc_packet_t last_sent_packet;
static void mock_load_packet(const dcc_packet_t *packet) {
    uint8_t i;
    for (i = 0; i < packet->byte_count && i < DCC_PACKET_MAX_BYTES; i++)
        last_loaded_packet.data[i] = packet->data[i];
    last_loaded_packet.byte_count = packet->byte_count;
    last_loaded_packet.preamble_bits = packet->preamble_bits;
    last_loaded_packet.repeat_count = packet->repeat_count;
    load_packet_count++;
}

static bool mock_is_encoder_idle(void) {
    return encoder_idle;
}

static void mock_on_packet_sent(const dcc_packet_t *packet) {
    uint8_t i;
    for (i = 0; i < packet->byte_count && i < DCC_PACKET_MAX_BYTES; i++)
        last_sent_packet.data[i] = packet->data[i];
    last_sent_packet.byte_count = packet->byte_count;
    on_packet_sent_count++;
}

static void reset_mocks(void) {
    memset(&last_loaded_packet, 0, sizeof(last_loaded_packet));
    load_packet_count = 0;
    encoder_idle = true;
    on_packet_sent_count = 0;
    memset(&last_sent_packet, 0, sizeof(last_sent_packet));
}

static interface_dcc_scheduler_t make_interface(void) {
    interface_dcc_scheduler_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.load_packet = mock_load_packet;
    interface.is_encoder_idle = mock_is_encoder_idle;
    interface.build_idle_packet = &DccApplicationCommandStationPacket_load_idle;
    interface.on_packet_sent = mock_on_packet_sent;

    return interface;
}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccScheduler, initialize_does_not_crash) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);
}

// ============================================================================
// Idle behavior tests
// ============================================================================

// @compliance DCC-S9.2-CS-007
TEST(DccScheduler, run_sends_idle_when_empty) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    DccScheduler_run(&context);

    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Verify idle packet was loaded */
    EXPECT_EQ(last_loaded_packet.data[0], DCC_IDLE_ADDR_BYTE);
    EXPECT_EQ(last_loaded_packet.data[1], DCC_IDLE_DATA_BYTE);
    EXPECT_EQ(last_loaded_packet.data[2], DCC_IDLE_XOR_BYTE);
}

TEST(DccScheduler, run_waits_for_encoder_idle) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    encoder_idle = false;
    DccScheduler_run(&context);

    EXPECT_EQ(load_packet_count, (uint32_t)0);
}

// ============================================================================
// One-shot packet tests
// ============================================================================

TEST(DccScheduler, insert_and_send_one_shot) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 1;

    bool ok = DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);
    EXPECT_TRUE(ok);

    DccScheduler_run(&context);

    EXPECT_EQ(load_packet_count, (uint32_t)1);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)1);

    /* Verify the loaded packet matches */
    EXPECT_EQ(last_loaded_packet.data[0], 3);
    EXPECT_EQ(last_loaded_packet.data[1], (DCC_INST_ADVANCED_OPS | DCC_ADV_OPS_128_SPEED));
}

TEST(DccScheduler, one_shot_removed_after_repeat_exhausted) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 1;

    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* First run sends the packet */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Simulate packet complete from ISR */
    DccScheduler_on_packet_complete(&context);

    /* Second run — slot is now exhausted, should get idle */
    DccScheduler_run(&context);
}

TEST(DccScheduler, one_shot_repeat_count_2) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 2;

    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* First send */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    DccScheduler_on_packet_complete(&context);

    /* Second send */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);

    DccScheduler_on_packet_complete(&context);

    /* Third run — should be idle (exhausted after 2 repeats) */
    DccScheduler_run(&context);
}

// ============================================================================
// 5 ms same-address spacing (S-9.2 Section C fn.11) — short addr 112-127
// ============================================================================

// @compliance DCC-S9.2-CS-008
TEST(DccScheduler, run_inserts_idle_spacer_between_same_short_addr_112_127) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 115, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 2;
    DccScheduler_insert(&context, &pkt, 115, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* Run 1: real packet to short address 115 (first byte 0x73) */
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)115);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)1);

    /* Run 2: the same address would be <5 ms away -> idle spacer, not the packet,
     * and the spacer must NOT fire on_packet_sent */
    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)DCC_IDLE_ADDR_BYTE);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)1);

    /* Run 3: guard cleared by the idle -> deferred repeat goes out (repeat_count preserved) */
    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)115);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)2);
}

// @compliance DCC-S9.2-CS-008
TEST(DccScheduler, run_no_spacer_for_short_addr_below_112) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 100, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 2;
    DccScheduler_insert(&context, &pkt, 100, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)100);

    /* Address 100 (0x64) does not alias a service-mode byte -> consecutive send, no spacer */
    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)100);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)2);
}

// @compliance DCC-S9.2-CS-008
TEST(DccScheduler, run_no_spacer_for_long_address) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 1000, DCC_ADDRESS_LONG, 50, true);
    pkt.repeat_count = 2;
    DccScheduler_insert(&context, &pkt, 1000, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    DccScheduler_run(&context);
    uint8_t first = last_loaded_packet.data[0];
    EXPECT_NE(first, (uint8_t)DCC_IDLE_ADDR_BYTE);

    /* Long-address first byte is 0xC0-0xFF, never 0x70-0x7F -> no spacer */
    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], first);
    EXPECT_EQ(on_packet_sent_count, (uint32_t)2);
}

// ============================================================================
// Priority tests
// ============================================================================

// @compliance DCC-Library-CS-001
TEST(DccScheduler, estop_has_higher_priority_than_speed) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* Insert speed packet first */
    dcc_packet_t speed_pkt;
    DccApplicationCommandStationPacket_load_speed_128(&speed_pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    speed_pkt.repeat_count = 1;
    DccScheduler_insert(&context, &speed_pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* Insert e-stop second */
    dcc_packet_t estop_pkt;
    DccApplicationCommandStationPacket_load_estop_all(&estop_pkt, true);
    estop_pkt.repeat_count = 1;
    DccScheduler_insert(&context, &estop_pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_ESTOP, false);

    DccScheduler_run(&context);

    /* E-stop should be sent first (lower priority enum = higher priority) */
    EXPECT_EQ(last_loaded_packet.data[0], 0x00);  /* broadcast addr for e-stop */
}

// ============================================================================
// Duplicate combining tests
// ============================================================================

// @compliance DCC-Library-CS-003
TEST(DccScheduler, duplicate_combining_overwrites_packet) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* Insert speed=50 for address 3 */
    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 1;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* Insert speed=80 for same address 3, same tag — should overwrite */
    dcc_packet_t pkt2;
    DccApplicationCommandStationPacket_load_speed_128(&pkt2, 3, DCC_ADDRESS_SHORT, 80, true);
    pkt2.repeat_count = 1;
    DccScheduler_insert(&context, &pkt2, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    DccScheduler_run(&context);

    /* The loaded packet should have speed=80, not speed=50 */
    EXPECT_EQ(last_loaded_packet.data[2], 0x80 | 80);
}

// @compliance DCC-Library-CS-003
TEST(DccScheduler, different_tags_do_not_combine) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 1;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* Different tag (function group 1) for same address — should NOT overwrite */
    dcc_packet_t pkt2;
    memset(&pkt2, 0, sizeof(pkt2));
    pkt2.data[0] = 0x03;
    pkt2.data[1] = 0x80;
    pkt2.byte_count = 3;
    pkt2.preamble_bits = DCC_PREAMBLE_BITS_OPS;
    pkt2.repeat_count = 1;
    DccScheduler_insert(&context, &pkt2, 3, DCC_TAG_FUNC_GROUP_1, DCC_PRIORITY_FUNCTION, false);

    /* Both should be present — send first (speed has higher priority) */
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[2], 0x80 | 50);  /* speed packet */
}

TEST(DccScheduler, different_addresses_do_not_combine) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 1;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    dcc_packet_t pkt2;
    DccApplicationCommandStationPacket_load_speed_128(&pkt2, 5, DCC_ADDRESS_SHORT, 80, true);
    pkt2.repeat_count = 1;
    DccScheduler_insert(&context, &pkt2, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* First run should send one packet */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    DccScheduler_on_packet_complete(&context);

    /* Second run should send the other */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);
}

// ============================================================================
// Auto-refresh tests
// ============================================================================

// @compliance DCC-Library-CS-002
TEST(DccScheduler, auto_refresh_keeps_sending) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);
    context.refresh_cold_cycles = 0;   /* flat ring: the classic-mode regression */

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 0;  /* no one-shot repeats */

    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* Run multiple times — should keep sending the refresh packet */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);

    DccScheduler_on_packet_complete(&context);
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)3);
}

// @compliance DCC-Library-CS-002
TEST(DccScheduler, auto_refresh_round_robin) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);
    context.refresh_cold_cycles = 0;   /* flat ring: the classic-mode regression */

    /* Insert two refresh slots for different addresses */
    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 0;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    dcc_packet_t pkt2;
    DccApplicationCommandStationPacket_load_speed_128(&pkt2, 5, DCC_ADDRESS_SHORT, 80, true);
    pkt2.repeat_count = 0;
    DccScheduler_insert(&context, &pkt2, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* First run — should get one of the two */
    DccScheduler_run(&context);
    uint8_t first_addr = last_loaded_packet.data[0];
    EXPECT_TRUE(first_addr == 3 || first_addr == 5);

    DccScheduler_on_packet_complete(&context);

    /* Second run — should get the other one */
    DccScheduler_run(&context);
    uint8_t second_addr = last_loaded_packet.data[0];
    EXPECT_TRUE(second_addr == 3 || second_addr == 5);
    EXPECT_NE(first_addr, second_addr);
}

// A changed command for a refresh slot goes out on the very next cycle, not when the
// ring comes round to it (issue #5, phase 1).
TEST(DccScheduler, changed_refresh_slot_is_sent_next_cycle) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* Addresses 1..8 fill slots 0..7 */
    dcc_packet_t pkt;
    for (dcc_address_t address = 1; address <= 8; address++) {
        DccApplicationCommandStationPacket_load_speed_128(&pkt, address, DCC_ADDRESS_SHORT, 10, true);
        DccScheduler_insert(&context, &pkt, address, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);
    }

    /* Serve slots 0..4, so the ring's next turn is slot 5, far from slot 2 */
    for (int cycle = 0; cycle < 5; cycle++) {
        DccScheduler_run(&context);
        DccScheduler_on_packet_complete(&context);
    }
    ASSERT_EQ(last_loaded_packet.data[0], (uint8_t)5);

    /* Change address 3's speed (slot 2): it is the very next packet */
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 90, true);
    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], (uint8_t)3);
    EXPECT_EQ(last_loaded_packet.data[2], (uint8_t)(0x80 | 90));
}

// @compliance DCC-Library-CS-001
TEST(DccScheduler, one_shot_takes_priority_over_refresh) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* Insert refresh slot */
    dcc_packet_t refresh_pkt;
    DccApplicationCommandStationPacket_load_speed_128(&refresh_pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    refresh_pkt.repeat_count = 0;
    DccScheduler_insert(&context, &refresh_pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* Insert one-shot CV write */
    dcc_packet_t cv_pkt;
    memset(&cv_pkt, 0, sizeof(cv_pkt));
    cv_pkt.data[0] = 0x05;  /* address 5 */
    cv_pkt.data[1] = 0xEC;  /* CV long write */
    cv_pkt.data[2] = 0x00;
    cv_pkt.data[3] = 0x42;
    cv_pkt.byte_count = 5;
    cv_pkt.preamble_bits = DCC_PREAMBLE_BITS_OPS;
    cv_pkt.repeat_count = 1;
    DccScheduler_insert(&context, &cv_pkt, 5, DCC_TAG_CV, DCC_PRIORITY_CV, false);

    /* Run — one-shot should be sent first */
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], 0x05);

    DccScheduler_on_packet_complete(&context);

    /* Next run — one-shot exhausted, should get refresh */
    DccScheduler_run(&context);
    EXPECT_EQ(last_loaded_packet.data[0], 3);
}

// ============================================================================
// Remove and clear tests
// ============================================================================

TEST(DccScheduler, remove_address_clears_slots) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 0;
    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    DccScheduler_remove_address(&context, 3);

    DccScheduler_run(&context);
    /* Should get idle, not the removed speed packet */
    EXPECT_EQ(load_packet_count, (uint32_t)1);  /* idle packet is still loaded */
}

TEST(DccScheduler, clear_removes_all_slots) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 0;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    dcc_packet_t pkt2;
    DccApplicationCommandStationPacket_load_speed_128(&pkt2, 5, DCC_ADDRESS_SHORT, 80, true);
    pkt2.repeat_count = 0;
    DccScheduler_insert(&context, &pkt2, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    DccScheduler_clear(&context);

    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);  /* idle packet is still loaded */
}

// ============================================================================
// Slot exhaustion test
// ============================================================================

TEST(DccScheduler, insert_returns_false_when_full) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_idle(&pkt);
    pkt.repeat_count = 1;

    /* Fill all slots with unique (address, tag) pairs */
    bool ok = true;
    for (uint16_t i = 0; i < USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT; i++) {
        ok = DccScheduler_insert(&context, &pkt, i, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);
        EXPECT_TRUE(ok) << "slot " << i;
    }

    /* Next insert should fail */
    ok = DccScheduler_insert(&context, &pkt, 9999, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);
    EXPECT_FALSE(ok);
}

// ============================================================================
// Packet complete handoff test
// ============================================================================

TEST(DccScheduler, waits_for_packet_complete_after_first) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* First run loads idle immediately */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Second run without packet_complete — should NOT load another */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Signal packet complete */
    DccScheduler_on_packet_complete(&context);

    /* Now it should load another */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);
}

// ============================================================================
// Null callback safety tests
// ============================================================================

TEST(DccScheduler, null_on_packet_sent_does_not_crash) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    interface.on_packet_sent = NULL;
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 1;
    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);
}

// ============================================================================
// Coverage gap: _select_one_shot skips active non-refresh slot with
// repeat_count==0, and _select_refresh scans past active non-refresh slot
// ============================================================================

TEST(DccScheduler, one_shot_skips_zero_repeat_and_refresh_scans_non_refresh) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    /* Insert a non-refresh slot with repeat_count=0 — active but exhausted */
    dcc_packet_t pkt_zero;
    DccApplicationCommandStationPacket_load_speed_128(&pkt_zero, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt_zero.repeat_count = 0;
    DccScheduler_insert(&context, &pkt_zero, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, false);

    /* Insert a refresh slot for a different address */
    dcc_packet_t pkt_refresh;
    DccApplicationCommandStationPacket_load_speed_128(&pkt_refresh, 5, DCC_ADDRESS_SHORT, 80, true);
    pkt_refresh.repeat_count = 0;
    DccScheduler_insert(&context, &pkt_refresh, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* _select_one_shot should skip the zero-repeat slot (line 126-128),
     * then _select_refresh should scan past the non-refresh slot (line 157) */
    DccScheduler_run(&context);

    /* The refresh packet for address 5 should be sent */
    EXPECT_EQ(load_packet_count, (uint32_t)1);
    EXPECT_EQ(last_loaded_packet.data[0], 5);
}

// ============================================================================
// Coverage gap: remove_address iterates past active slot with different address
// ============================================================================

TEST(DccScheduler, remove_address_with_multiple_addresses) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt1;
    DccApplicationCommandStationPacket_load_speed_128(&pkt1, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt1.repeat_count = 0;
    DccScheduler_insert(&context, &pkt1, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    dcc_packet_t pkt2;
    DccApplicationCommandStationPacket_load_speed_128(&pkt2, 5, DCC_ADDRESS_SHORT, 80, true);
    pkt2.repeat_count = 0;
    DccScheduler_insert(&context, &pkt2, 5, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* Remove only address 3 — must iterate past active address 5 slot */
    DccScheduler_remove_address(&context, 3);

    DccScheduler_run(&context);

    /* Only address 5 should remain */
    EXPECT_EQ(last_loaded_packet.data[0], 5);
}

// ============================================================================
// Coverage gap: DccScheduler_run with NULL interface
// ============================================================================

TEST(DccScheduler, run_with_null_interface) {
    reset_mocks();
    dcc_scheduler_context_t context;
    DccScheduler_initialize(&context, NULL);

    /* Should return early without crashing */
    DccScheduler_run(&context);

    EXPECT_EQ(load_packet_count, (uint32_t)0);
}

// ============================================================================
// Coverage gap: NULL on_packet_sent in refresh path
// ============================================================================

TEST(DccScheduler, null_on_packet_sent_in_refresh_path) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    interface.on_packet_sent = NULL;
    DccScheduler_initialize(&context, &interface);

    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 0;
    DccScheduler_insert(&context, &pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);

    /* Should not crash despite NULL on_packet_sent */
    DccScheduler_run(&context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);
}

// ============================================================================
// Builder -> scheduler integration: a builder's packet handed over UNTOUCHED
// (no repeat_count override) is transmitted exactly repeat_count times, and at
// least once. This is the path the RP2350 port broke on: the builders used to
// set repeat_count = 0, which the scheduler treats as "nothing left to send",
// and every test and firmware overwrote the field before it could show. The
// tests above set the count on purpose (they test the scheduler); this one
// must NOT.
// ============================================================================

/* Insert `pkt` as-is and count how many times the scheduler loads THAT packet
 * before it falls back to idle. */
static uint32_t sends_of_untouched(const dcc_packet_t *pkt, dcc_address_t address,
                                   dcc_tag_enum tag, dcc_priority_enum priority) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

    if (!DccScheduler_insert(&context, pkt, address, tag, priority, false)) {
        return 0;
    }

    uint32_t sends = 0;
    for (int run = 0; run < 8; run++) {
        DccScheduler_run(&context);
        DccScheduler_on_packet_complete(&context);
        if (last_loaded_packet.byte_count == pkt->byte_count &&
            memcmp(last_loaded_packet.data, pkt->data, pkt->byte_count) == 0) {
            sends++;
        } else {
            break;      /* idle went out: the one-shot slot is exhausted */
        }
    }
    return sends;
}

#define EXPECT_TRANSMITS_UNTOUCHED(pkt, addr, tag, prio, label)                         \
    do {                                                                                 \
        uint32_t sends = sends_of_untouched(&(pkt), (addr), (tag), (prio));              \
        EXPECT_GE(sends, (uint32_t)1) << label << ": never transmitted";                 \
        EXPECT_EQ(sends, (uint32_t)(pkt).repeat_count) << label << ": repeat_count " \
            << (unsigned)(pkt).repeat_count << " but sent " << sends << " time(s)";     \
    } while (0)

TEST(DccScheduler, builder_packets_transmit_untouched_exactly_repeat_count_times) {
    dcc_packet_t pkt;

    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, "speed_128");
    DccApplicationCommandStationPacket_load_speed_28(&pkt, 3, DCC_ADDRESS_SHORT, 10, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, "speed_28");
    DccApplicationCommandStationPacket_load_speed_14(&pkt, 3, DCC_ADDRESS_SHORT, 5, true, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, "speed_14");
    DccApplicationCommandStationPacket_load_estop_all(&pkt, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_ESTOP, "estop_all");
    DccApplicationCommandStationPacket_load_reset(&pkt);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_ESTOP, "reset");
    DccApplicationCommandStationPacket_load_func_group_1(&pkt, 3, DCC_ADDRESS_SHORT, 0x01);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_FUNC_GROUP_1, DCC_PRIORITY_FUNCTION, "func_group_1");
    DccApplicationCommandStationPacket_load_func_f13_f20(&pkt, 3, DCC_ADDRESS_SHORT, 0x80);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_FUNC_F13_F20, DCC_PRIORITY_FUNCTION, "func_f13_f20");
    DccApplicationCommandStationPacket_load_accessory_basic(&pkt, 5, 2, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 5, DCC_TAG_ACCESSORY, DCC_PRIORITY_ACCESSORY, "accessory_basic");
    DccApplicationCommandStationPacket_load_accessory_extended(&pkt, 5, 3);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 5, DCC_TAG_ACCESSORY, DCC_PRIORITY_ACCESSORY, "accessory_extended");
    DccApplicationCommandStationPacket_load_accessory_nop(&pkt, 5, false);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 5, DCC_TAG_ACCESSORY, DCC_PRIORITY_ACCESSORY, "accessory_nop");
    DccApplicationCommandStationPacket_load_consist_set(&pkt, 3, DCC_ADDRESS_SHORT, 10, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_CONSIST, DCC_PRIORITY_FUNCTION, "consist_set");
    DccApplicationCommandStationPacket_load_binary_state_short(&pkt, 3, DCC_ADDRESS_SHORT, 1, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_BINARY_STATE, DCC_PRIORITY_FUNCTION, "binary_state_short");
    DccApplicationCommandStationPacket_load_analog_function(&pkt, 3, DCC_ADDRESS_SHORT, 1, 100);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_ANALOG_FUNC, DCC_PRIORITY_FUNCTION, "analog_function");
    DccApplicationCommandStationPacket_load_system_time(&pkt, 1000);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_FUNCTION, "system_time");
    DccApplicationCommandStationPacket_load_model_time(&pkt, 30, DCC_DAY_OF_WEEK_MONDAY, 12, false, 1);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_FUNCTION, "model_time");
    DccApplicationCommandStationPacket_load_model_date(&pkt, 23, 9, 2026);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_FUNCTION, "model_date");
    DccApplicationCommandStationPacket_load_cv_write_pom(&pkt, 3, DCC_ADDRESS_SHORT, 1, 8);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_CV, DCC_PRIORITY_CV, "cv_write_pom");
    DccApplicationCommandStationPacket_load_cv_verify_pom(&pkt, 3, DCC_ADDRESS_SHORT, 1, 8);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_CV, DCC_PRIORITY_CV, "cv_verify_pom");
    DccApplicationCommandStationPacket_load_cv_bit_pom(&pkt, 3, DCC_ADDRESS_SHORT, 1, 5, true, true);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 3, DCC_TAG_CV, DCC_PRIORITY_CV, "cv_bit_pom write");
    DccApplicationCommandStationPacket_load_accessory_basic_cv_write(&pkt, 1, 0, 7, 42);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 1, DCC_TAG_CV, DCC_PRIORITY_CV, "accessory_basic_cv_write");
    DccApplicationCommandStationPacket_load_accessory_basic_cv_verify(&pkt, 1, 0, 7, 42);
    EXPECT_TRANSMITS_UNTOUCHED(pkt, 1, DCC_TAG_CV, DCC_PRIORITY_CV, "accessory_basic_cv_verify");
}

// An application that sets repeat_count AFTER the builder returns must win over
// the builder's default -- in both directions. The bench firmware and the
// RP2350 port both rely on this.
TEST(DccScheduler, application_override_of_repeat_count_wins) {
    dcc_packet_t pkt;

    /* builder default 2, overridden down to 1: exactly one send, then idle */
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    ASSERT_EQ(pkt.repeat_count, 2);
    pkt.repeat_count = 1;
    EXPECT_EQ(sends_of_untouched(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED), (uint32_t)1);

    /* builder default 2, overridden up to 4 */
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 4;
    EXPECT_EQ(sends_of_untouched(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED), (uint32_t)4);

    /* builder default 1 (CV verify), overridden up to 3 */
    DccApplicationCommandStationPacket_load_cv_verify_pom(&pkt, 3, DCC_ADDRESS_SHORT, 1, 8);
    ASSERT_EQ(pkt.repeat_count, 1);
    pkt.repeat_count = 3;
    EXPECT_EQ(sends_of_untouched(&pkt, 3, DCC_TAG_CV, DCC_PRIORITY_CV), (uint32_t)3);

    /* and an explicit 0 still means "never sent" -- the contract the defaults exist for */
    DccApplicationCommandStationPacket_load_speed_128(&pkt, 3, DCC_ADDRESS_SHORT, 50, true);
    pkt.repeat_count = 0;
    EXPECT_EQ(sends_of_untouched(&pkt, 3, DCC_TAG_SPEED, DCC_PRIORITY_SPEED), (uint32_t)0);
}

// ============================================================================
// Refresh pacing (issue #5, phase 2): a burst at full rate after each insert,
// then a keep-alive every refresh_cold_cycles, never later than
// refresh_cold_max_cycles. Selection order: overdue cold slot, then a slot in
// its burst, then a merely-due cold slot.
// ============================================================================

static void pacing_init(dcc_scheduler_context_t *context, interface_dcc_scheduler_t *interface) {
    reset_mocks();
    *interface = make_interface();
    DccScheduler_initialize(context, interface);
}

/* A speed refresh slot for a short address (keep clear of 112-127: no spacer). */
static void insert_refresh(dcc_scheduler_context_t *context, dcc_address_t address, uint8_t speed) {
    dcc_packet_t pkt;
    DccApplicationCommandStationPacket_load_speed_128(&pkt, address, DCC_ADDRESS_SHORT, speed, true);
    DccScheduler_insert(context, &pkt, address, DCC_TAG_SPEED, DCC_PRIORITY_SPEED, true);
}

/* One packet cycle. Returns the first byte loaded: the short address, or DCC_IDLE_ADDR_BYTE. */
static uint8_t pacing_cycle(dcc_scheduler_context_t *context) {
    DccScheduler_run(context);
    DccScheduler_on_packet_complete(context);
    return last_loaded_packet.data[0];
}

/* Runs cycles until every slot has spent its burst (none left prompt). */
static void spend_bursts(dcc_scheduler_context_t *context, int slot_count) {
    for (int cycle = 0; cycle < slot_count * context->refresh_prompt_sends; cycle++) {
        pacing_cycle(context);
    }
}

// Invariant 1 (plus the settle): a fresh slot is sent PROMPT_SENDS times back to
// back, then once per COLD_CYCLES.
TEST(DccScheduler, prompt_burst_then_settle) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    const int prompt = context.refresh_prompt_sends;
    const int cold = context.refresh_cold_cycles;
    ASSERT_GE(prompt, 1);
    ASSERT_GE(cold, 2);

    insert_refresh(&context, 3, 50);

    int sends[16];
    int count = 0;
    for (int cycle = 0; cycle <= prompt + 3 * cold && count < 16; cycle++) {
        if (pacing_cycle(&context) == 3) {
            sends[count++] = cycle;
        }
    }

    ASSERT_EQ(count, prompt + 3);
    for (int i = 0; i < prompt; i++) {
        EXPECT_EQ(sends[i], i) << "burst send " << i;
    }
    for (int i = prompt; i < count; i++) {
        EXPECT_EQ(sends[i] - sends[i - 1], cold) << "keep-alive " << (i - prompt + 1);
    }
}

// A slot updated every cycle never drops into the cold tier.
TEST(DccScheduler, continuous_changes_stay_at_full_rate) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);

    for (int cycle = 0; cycle < 3 * context.refresh_cold_cycles; cycle++) {
        uint8_t speed = (uint8_t)(10 + cycle % 100);
        insert_refresh(&context, 3, speed);
        ASSERT_EQ(pacing_cycle(&context), (uint8_t)3) << "cycle " << cycle;
        EXPECT_EQ(last_loaded_packet.data[2], (uint8_t)(0x80 | speed)) << "cycle " << cycle;
    }
}

// 15 idle slots + 1 changing every cycle: the changing slot gets every cycle except
// those owed to a cold slot that has reached COLD_MAX_CYCLES; merely-due cold slots
// never take a cycle from it.
TEST(DccScheduler, active_slot_full_rate_among_idle_pool) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);

    for (dcc_address_t address = 1; address <= 16; address++) {
        insert_refresh(&context, address, 20);
    }
    spend_bursts(&context, 16);

    const int window = 4 * context.refresh_cold_max_cycles;
    int changer_sends = 0;
    int idle_sends = 0;

    for (int cycle = 0; cycle < window; cycle++) {
        bool overdue[15];
        for (int slot = 0; slot < 15; slot++) {
            /* the scheduler ages a cold slot by one before it chooses */
            overdue[slot] = context.slots[slot].unsent_cycles + 1 >= context.refresh_cold_max_cycles;
        }

        insert_refresh(&context, 16, (uint8_t)(10 + cycle % 100));
        uint8_t sent = pacing_cycle(&context);

        if (sent == 16) {
            changer_sends++;
        } else {
            ASSERT_GE(sent, (uint8_t)1) << "cycle " << cycle;
            ASSERT_LE(sent, (uint8_t)15) << "cycle " << cycle;
            EXPECT_TRUE(overdue[sent - 1]) << "cycle " << cycle << ": address " << (int)sent
                                           << " was sent without being overdue";
            idle_sends++;
        }
    }

    const int owed = 15 * (window / context.refresh_cold_max_cycles + 1);
    EXPECT_EQ(changer_sends + idle_sends, window);
    EXPECT_LE(idle_sends, owed);
    EXPECT_GE(changer_sends, window - owed);
}

// Invariant 2: 15 cold slots due on the same cycle, then a real update -- the update
// goes first (its whole burst), whatever the ring position, then the batch in ring order.
TEST(DccScheduler, prompt_never_waits_behind_due_cold_batch) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);

    for (dcc_address_t address = 1; address <= 16; address++) {
        insert_refresh(&context, address, 20);
    }
    spend_bursts(&context, 16);

    /* slots 0..14 all due on the next cycle, none overdue; slot 15 just sent */
    for (int slot = 0; slot < 15; slot++) {
        context.slots[slot].unsent_cycles = context.refresh_cold_cycles;
    }
    context.slots[15].unsent_cycles = 0;
    ASSERT_LT(context.refresh_cold_cycles + context.refresh_prompt_sends + 15, context.refresh_cold_max_cycles);

    insert_refresh(&context, 16, 99);
    context.refresh_index = 0;        /* the due batch sits ahead of the update in the ring, */
    context.refresh_cold_index = 0;   /* for both cursors */

    for (int i = 0; i < context.refresh_prompt_sends; i++) {
        EXPECT_EQ(pacing_cycle(&context), (uint8_t)16) << "burst send " << i;
    }
    for (dcc_address_t address = 1; address <= 15; address++) {
        EXPECT_EQ(pacing_cycle(&context), (uint8_t)address);
    }
}

// Each cold slot keeps its own cadence: exactly one send per COLD_CYCLES, no clumping.
TEST(DccScheduler, cold_slots_keep_independent_cadence) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);

    for (dcc_address_t address = 1; address <= 8; address++) {
        insert_refresh(&context, address, 20);
    }
    spend_bursts(&context, 8);

    const int cold = context.refresh_cold_cycles;
    int last_send[9];
    int sends[9];
    for (int a = 0; a <= 8; a++) {
        last_send[a] = -1;
        sends[a] = 0;
    }

    for (int cycle = 0; cycle < 5 * cold; cycle++) {
        uint8_t sent = pacing_cycle(&context);
        if (sent == DCC_IDLE_ADDR_BYTE) {
            continue;
        }
        ASSERT_GE(sent, (uint8_t)1);
        ASSERT_LE(sent, (uint8_t)8);
        if (last_send[sent] >= 0) {
            EXPECT_EQ(cycle - last_send[sent], cold) << "address " << (int)sent;
        }
        last_send[sent] = cycle;
        sends[sent]++;
    }

    for (int a = 1; a <= 8; a++) {
        EXPECT_GE(sends[a], 4) << "address " << a;
        EXPECT_LE(sends[a], 5) << "address " << a;
    }
}

// Invariant 3: 15 slots kept permanently in their burst cannot hold a cold slot
// past COLD_MAX_CYCLES.
TEST(DccScheduler, cold_slot_never_starves_past_max) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    const int cold_max = context.refresh_cold_max_cycles;

    insert_refresh(&context, 1, 20);
    int last_send = -1;
    for (int cycle = 0; cycle < context.refresh_prompt_sends; cycle++) {
        if (pacing_cycle(&context) == 1) {
            last_send = cycle;
        }
    }
    ASSERT_EQ(last_send, context.refresh_prompt_sends - 1);

    int starved_sends = 0;
    for (int cycle = context.refresh_prompt_sends; cycle < 3 * cold_max; cycle++) {
        for (dcc_address_t address = 2; address <= 16; address++) {
            insert_refresh(&context, address, (uint8_t)(10 + cycle % 100));
        }
        if (pacing_cycle(&context) == 1) {
            EXPECT_LE(cycle - last_send, cold_max) << "cycle " << cycle;
            last_send = cycle;
            starved_sends++;
        }
        ASSERT_LE(cycle - last_send, cold_max) << "address 1 unsent for more than COLD_MAX_CYCLES";
    }
    EXPECT_GE(starved_sends, 2);
}

// Invariant 4: COLD_CYCLES = 0 is today's flat round-robin, exactly.
TEST(DccScheduler, cold_interval_zero_is_classic_round_robin) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    context.refresh_cold_cycles = 0;

    for (dcc_address_t address = 1; address <= 5; address++) {
        insert_refresh(&context, address, 20);
    }

    for (int cycle = 0; cycle < 100; cycle++) {
        EXPECT_EQ(pacing_cycle(&context), (uint8_t)(cycle % 5 + 1)) << "cycle " << cycle;
    }
}

// Invariant 5: a one-shot inserted mid-burst goes out first; the burst resumes after.
TEST(DccScheduler, one_shot_still_beats_prompt_refresh) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    ASSERT_EQ(context.refresh_prompt_sends, 3);

    insert_refresh(&context, 3, 50);
    EXPECT_EQ(pacing_cycle(&context), (uint8_t)3);

    dcc_packet_t cv_pkt;
    DccApplicationCommandStationPacket_load_cv_write_pom(&cv_pkt, 5, DCC_ADDRESS_SHORT, 1, 8);
    ASSERT_EQ(cv_pkt.repeat_count, DCC_REPEAT_CV_WRITE);
    DccScheduler_insert(&context, &cv_pkt, 5, DCC_TAG_CV, DCC_PRIORITY_CV, false);

    for (int i = 0; i < DCC_REPEAT_CV_WRITE; i++) {
        EXPECT_EQ(pacing_cycle(&context), (uint8_t)5) << "one-shot send " << i;
    }
    EXPECT_EQ(pacing_cycle(&context), (uint8_t)3);   /* burst send 2 of 3 */
    EXPECT_EQ(pacing_cycle(&context), (uint8_t)3);   /* burst send 3 of 3 */
    EXPECT_EQ(pacing_cycle(&context), (uint8_t)DCC_IDLE_ADDR_BYTE);
}

// Invariant 5: the S-9.2 5 ms same-address spacer still applies inside a burst, and a
// spacer does not use up a burst send.
TEST(DccScheduler, same_address_spacing_survives_prompt_burst) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    ASSERT_EQ(context.refresh_prompt_sends, 3);

    insert_refresh(&context, 115, 50);   /* first byte 0x73 aliases a service-mode command */

    const uint8_t expected[6] = { 115, DCC_IDLE_ADDR_BYTE, 115, DCC_IDLE_ADDR_BYTE, 115, DCC_IDLE_ADDR_BYTE };
    for (int cycle = 0; cycle < 6; cycle++) {
        EXPECT_EQ(pacing_cycle(&context), expected[cycle]) << "cycle " << cycle;
    }
    EXPECT_EQ(on_packet_sent_count, (uint32_t)3);
}

// More refresh slots than the ceiling can serve (some always overdue): changes still
// get every other cycle, and the overdue slots share the rest evenly.
TEST(DccScheduler, changes_keep_flowing_when_idle_slots_overload) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);

    for (dcc_address_t address = 1; address <= 16; address++) {
        insert_refresh(&context, address, 20);
    }
    spend_bursts(&context, 16);

    /* 15 idle slots against an 8-cycle ceiling: they cannot all be served in time */
    context.refresh_cold_cycles = 4;
    context.refresh_cold_max_cycles = 8;

    const int window = 300;
    int changer_sends = 0;
    int last_send[16];
    int idle_sends[16];
    for (int a = 0; a < 16; a++) {
        last_send[a] = -1;
        idle_sends[a] = 0;
    }

    for (int cycle = 0; cycle < window; cycle++) {
        insert_refresh(&context, 16, (uint8_t)(10 + cycle % 100));
        uint8_t sent = pacing_cycle(&context);
        if (sent == 16) {
            changer_sends++;
            continue;
        }
        ASSERT_GE(sent, (uint8_t)1) << "cycle " << cycle;
        ASSERT_LE(sent, (uint8_t)15) << "cycle " << cycle;
        if (last_send[sent] >= 0) {
            EXPECT_LE(cycle - last_send[sent], 2 * 15) << "address " << (int)sent;
        }
        last_send[sent] = cycle;
        idle_sends[sent]++;
    }

    EXPECT_GE(changer_sends, window / 2 - 1);
    for (int a = 1; a <= 15; a++) {
        EXPECT_GE(idle_sends[a], window / (2 * 15) - 1) << "address " << a;
    }
}

// A burst that keeps losing the burst pass to a slot changed every cycle still falls
// overdue, so it is sent no less often than COLD_MAX_CYCLES and its burst completes.
TEST(DccScheduler, burst_slot_is_not_starved_by_a_slot_changing_every_cycle) {
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface;
    pacing_init(&context, &interface);
    const int cold_max = context.refresh_cold_max_cycles;

    insert_refresh(&context, 1, 20);

    int sends = 0;
    int last_send = -1;
    for (int cycle = 0; cycle < 3 * cold_max + 1 && sends < context.refresh_prompt_sends; cycle++) {
        insert_refresh(&context, 2, (uint8_t)(10 + cycle % 100));
        if (pacing_cycle(&context) == 1) {
            if (last_send >= 0) {
                EXPECT_LE(cycle - last_send, cold_max) << "cycle " << cycle;
            }
            last_send = cycle;
            sends++;
        }
    }
    EXPECT_EQ(sends, (int)context.refresh_prompt_sends);
}

// The keep-alive ceiling against the packet time-out (CV11, S-9.2.4 sec 4), at
// compile time. The smallest CV11 this library's fail-safe honours is 1 = 0.1 s,
// less than one pass over a 16-slot ring, so no refresh scheme can bound that; this
// checks the ceiling against a documented floor instead (REFRESH_CV11_FLOOR, in CV11
// units), with every cycle taken at its worst: longest packet, all zero bits as this
// library's encoder sends them (two 58 us ticks per half), plus a RailCom cutout.
#define REFRESH_CV11_FLOOR              20   /* 2.0 s with DCC_FAILSAFE_CV11_UNIT_US = 0.1 s */
#define REFRESH_WORST_ZERO_BIT_US       (4u * DCC_ONE_BIT_HALF_PERIOD_US)
#define REFRESH_WORST_CUTOUT_US         500u /* T_CE max 488 us, S-9.3.2 Table 1 */
#define REFRESH_WORST_CYCLE_US                                                      \
    ((uint32_t)USER_DEFINED_DCC_PREAMBLE_BITS_OPS * 2u * DCC_ONE_BIT_HALF_PERIOD_US \
     + (uint32_t)DCC_PACKET_MAX_BYTES * 9u * REFRESH_WORST_ZERO_BIT_US              \
     + 2u * DCC_ONE_BIT_HALF_PERIOD_US + REFRESH_WORST_CUTOUT_US)

TEST(DccScheduler, cold_max_cycles_is_below_cv11_minimum) {
    static_assert((uint64_t)DCC_REFRESH_COLD_MAX_CYCLES * REFRESH_WORST_CYCLE_US <
                      (uint64_t)REFRESH_CV11_FLOOR * DCC_FAILSAFE_CV11_UNIT_US,
                  "DCC_REFRESH_COLD_MAX_CYCLES of worst-case packets exceeds the documented CV11 floor");
    EXPECT_LT((uint64_t)DCC_REFRESH_COLD_MAX_CYCLES * REFRESH_WORST_CYCLE_US,
              (uint64_t)REFRESH_CV11_FLOOR * DCC_FAILSAFE_CV11_UNIT_US);
}
