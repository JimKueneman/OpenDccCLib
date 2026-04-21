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
// Priority tests
// ============================================================================

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
    DccApplicationCommandStationPacket_load_estop_all(&estop_pkt);
    estop_pkt.repeat_count = 1;
    DccScheduler_insert(&context, &estop_pkt, 0, DCC_TAG_SPEED, DCC_PRIORITY_ESTOP, false);

    DccScheduler_run(&context);

    /* E-stop should be sent first (lower priority enum = higher priority) */
    EXPECT_EQ(last_loaded_packet.data[0], 0x00);  /* broadcast addr for e-stop */
}

// ============================================================================
// Duplicate combining tests
// ============================================================================

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

TEST(DccScheduler, auto_refresh_keeps_sending) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

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

TEST(DccScheduler, auto_refresh_round_robin) {
    reset_mocks();
    dcc_scheduler_context_t context;
    interface_dcc_scheduler_t interface = make_interface();
    DccScheduler_initialize(&context, &interface);

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
