/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Test suite for DCC Service Mode Common (Phase 4)
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_service_mode_common.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

// ============================================================================
// Mock / tracking state
// ============================================================================

static dcc_service_mode_common_context_t test_context;

static dcc_packet_t last_loaded_packet;
static uint32_t load_packet_count;
static bool encoder_idle_value;
static unsigned min_loaded_preamble;   /* smallest preamble_bits over all loaded packets */

static void mock_load_packet(const dcc_packet_t *packet) {

    memcpy(&last_loaded_packet, packet, sizeof(dcc_packet_t));
    load_packet_count++;
    if (packet->preamble_bits < min_loaded_preamble) {
        min_loaded_preamble = packet->preamble_bits;
    }

    /* Simulate ISR: packet transmission completes instantly in tests. */
    DccServiceModeCommon_on_packet_complete(&test_context);

}

static bool mock_is_encoder_idle(void) {

    return encoder_idle_value;

}

static void reset_mocks(void) {

    memset(&test_context, 0, sizeof(test_context));
    memset(&last_loaded_packet, 0, sizeof(last_loaded_packet));
    load_packet_count = 0;
    min_loaded_preamble = 0xFFFFu;
    encoder_idle_value = true;

}

static interface_dcc_service_mode_common_t make_interface(void) {

    interface_dcc_service_mode_common_t interface;
    memset(&interface, 0, sizeof(interface));

    interface.load_packet = mock_load_packet;
    interface.is_encoder_idle = mock_is_encoder_idle;

    return interface;

}

// ============================================================================
// Step callback tracking
// ============================================================================

static dcc_service_mode_result_t step_result;
static uint32_t step_callback_count;

static void mock_step_callback(dcc_service_mode_result_t result) {

    step_result = result;
    step_callback_count++;

}

static void reset_step_callback(void) {

    step_result = DCC_SERVICE_MODE_ERROR;
    step_callback_count = 0;

}

// ============================================================================
// Helper: build a simple command packet
// ============================================================================

static dcc_packet_t make_command_packet(void) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    packet.data[0] = 0x7C;
    packet.data[1] = 0x00;
    packet.data[2] = 0x55;
    packet.data[3] = 0x7C ^ 0x00 ^ 0x55;
    packet.byte_count = 4;
    packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    packet.repeat_count = 0;

    return packet;

}

// ============================================================================
// Helper: simulate ACK pulse via ack_sample calls
// ============================================================================

    /**
     * @brief Feed N consecutive above-threshold samples to simulate an ACK pulse.
     */
/* Trailing low samples needed to cross the dropout filter and end a run. */
#define TEST_ACK_END_LOWS \
    ((USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US / DCC_ONE_BIT_HALF_PERIOD_US) + 1)

static void feed_ack_samples(uint16_t count) {

    uint16_t sample_index;

    /* Tests inject the ACK directly via ack_sample(); open the scan window so
     * the samples are counted (window-opening logic is tested separately). */
    test_context.ack_window_open = true;

    for (sample_index = 0; sample_index < count; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* End of pulse -- enough below-threshold samples to pass the dropout filter. */
    for (sample_index = 0; sample_index < TEST_ACK_END_LOWS; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

}

// ============================================================================
// Helper: run the state machine through a full cycle
// Drive pre-reset -> command -> post-reset with ACK or no ACK
// ============================================================================

static void drive_full_cycle_with_ack(bool provide_ack) {

    uint8_t packet_index;

    /* Pre-reset phase: 3 reset packets */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* One more run to transition to COMMAND state */
    DccServiceModeCommon_run(&test_context);

    /* Command phase */
    if (provide_ack) {

        /* Send first command packet */
        DccServiceModeCommon_run(&test_context);

        /* Simulate ACK pulse: ~6ms at 58us per sample ≈ 104 samples.
         * We need at least ACK_MIN_DURATION / 58 samples. Using a
         * comfortable margin above the minimum. */
        feed_ack_samples(104);

        /* Next run detects ACK and transitions directly to RESET_POST
         * (early termination — remaining command packets are skipped). */
        DccServiceModeCommon_run(&test_context);

    } else {

        for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

            DccServiceModeCommon_run(&test_context);

        }

        /* One more run to evaluate (no ACK → retry or fail) */
        DccServiceModeCommon_run(&test_context);

    }

    /* Post-reset phase: 6 reset packets */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_POST_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Final run to transition to IDLE and fire callback */
    DccServiceModeCommon_run(&test_context);

}

// ============================================================================
// Long preamble (S-9.2.3 §D)
// ============================================================================

// @compliance DCC-S9.2.3-CS-010
TEST(DccServiceModeCommon, service_mode_packets_use_long_preamble) {

    /* S-9.2.3 §D: service-mode packets use a long (>= 20-bit) preamble. Pin the
     * constant to the spec minimum, then confirm every packet this module loads
     * during an operation carries it -- the reset packets are stamped here, and the
     * command packet is supplied with the same DCC_PREAMBLE_BITS_SERVICE the task
     * modules use. */
    EXPECT_GE(DCC_PREAMBLE_BITS_SERVICE, 20u);

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.data[0] = 0x74;            /* a register verify command (representative) */
    cmd.data[1] = 0x05;
    cmd.byte_count = 2;
    cmd.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    DccServiceModeCommon_begin_operation(&test_context, &cmd, NULL, false,
                                         DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    drive_full_cycle_with_ack(false);

    EXPECT_GT(load_packet_count, 0u);
    EXPECT_GE(min_loaded_preamble, (unsigned)DCC_PREAMBLE_BITS_SERVICE);
}

// ============================================================================
// Initialization tests
// ============================================================================

TEST(DccServiceModeCommon, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

}

TEST(DccServiceModeCommon, initialize_sets_idle) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));
    EXPECT_FALSE(DccServiceModeCommon_is_active(&test_context));

}

// ============================================================================
// Enter / exit tests
// ============================================================================

TEST(DccServiceModeCommon, enter_activates_service_mode) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    EXPECT_TRUE(DccServiceModeCommon_enter(&test_context));
    EXPECT_TRUE(DccServiceModeCommon_is_active(&test_context));

}

TEST(DccServiceModeCommon, exit_deactivates_service_mode) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    DccServiceModeCommon_enter(&test_context);
    DccServiceModeCommon_exit(&test_context);

    EXPECT_FALSE(DccServiceModeCommon_is_active(&test_context));

}

// ============================================================================
// begin_operation guard tests
// ============================================================================

TEST(DccServiceModeCommon, begin_operation_fails_not_in_service_mode) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    dcc_packet_t packet = make_command_packet();
    EXPECT_FALSE(DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0));

}

TEST(DccServiceModeCommon, begin_operation_succeeds_when_ready) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    EXPECT_TRUE(DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0));
    EXPECT_FALSE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// State machine: pre-reset phase
// ============================================================================

TEST(DccServiceModeCommon, pre_reset_sends_reset_packets) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* First run should send a reset packet */
    DccServiceModeCommon_run(&test_context);

    EXPECT_EQ(load_packet_count, (uint32_t)1);
    EXPECT_EQ(last_loaded_packet.data[0], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.data[1], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.data[2], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

// @compliance DCC-S9.2.3-CS-007
TEST(DccServiceModeCommon, pre_reset_sends_correct_count) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_RESET_PRE_COUNT);

}

// ============================================================================
// State machine: command phase
// ============================================================================

TEST(DccServiceModeCommon, command_phase_sends_command_packets) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* Drive through pre-reset */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to COMMAND state */
    DccServiceModeCommon_run(&test_context);
    load_packet_count = 0;

    /* First command packet */
    DccServiceModeCommon_run(&test_context);

    EXPECT_EQ(load_packet_count, (uint32_t)1);
    EXPECT_EQ(last_loaded_packet.data[0], packet.data[0]);
    EXPECT_EQ(last_loaded_packet.data[1], packet.data[1]);
    EXPECT_EQ(last_loaded_packet.data[2], packet.data[2]);

}

// ============================================================================
// State machine: ACK detection via ISR sampling
// ============================================================================

TEST(DccServiceModeCommon, ack_detected_returns_success) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    drive_full_cycle_with_ack(true);

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// @compliance DCC-S9.2.3-CS-009
TEST(DccServiceModeCommon, ack_detected_terminates_command_phase_early) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* Drive through pre-reset */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to COMMAND state */
    DccServiceModeCommon_run(&test_context);
    load_packet_count = 0;

    /* Send first command packet */
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Simulate ACK pulse after first packet */
    feed_ack_samples(104);

    /* Next run should detect ACK and skip to RESET_POST — no more
     * command packets loaded. */
    DccServiceModeCommon_run(&test_context);

    /* Only 1 command packet should have been loaded (not 5) */
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Drive through post-reset to completion */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_POST_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* transition to IDLE */

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

TEST(DccServiceModeCommon, no_ack_retries_then_fails) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* Drive through initial attempt + retries, all without ACK */
    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

TEST(DccServiceModeCommon, ack_sample_ignored_when_idle) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    /* Should not crash or change state when called outside service mode */
    DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

}

TEST(DccServiceModeCommon, ack_too_short_not_detected) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* Drive through pre-reset to command phase */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* transition to COMMAND */
    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* Feed only 2 above-threshold samples — way too short for ACK */
    DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);
    DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);
    DccServiceModeCommon_ack_sample(&test_context, 0);  /* end of pulse */

    /* Drive remaining command packets and post-reset to completion */
    for (packet_index = 1; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* transition to post-reset or retry */

    /* Should retry, not succeed — drive through all retries */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// State machine: idle after completion
// ============================================================================

TEST(DccServiceModeCommon, idle_after_successful_operation) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    drive_full_cycle_with_ack(true);

    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// run() does nothing when not in service mode
// ============================================================================

TEST(DccServiceModeCommon, run_does_nothing_when_not_in_service_mode) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);

    DccServiceModeCommon_run(&test_context);

    EXPECT_EQ(load_packet_count, (uint32_t)0);

}

// ============================================================================
// Encoder not idle blocks state machine
// ============================================================================

TEST(DccServiceModeCommon, encoder_not_idle_blocks_progress) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* First run sends reset packet */
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Encoder busy -- should not advance */
    encoder_idle_value = false;
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)1);

    /* Encoder idle again -- should advance */
    encoder_idle_value = true;
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);

}

// ============================================================================
// Exit does nothing if busy
// ============================================================================

TEST(DccServiceModeCommon, exit_blocked_while_busy) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    DccServiceModeCommon_exit(&test_context);
    EXPECT_TRUE(DccServiceModeCommon_is_active(&test_context));

}

// ============================================================================
// begin_operation rejected when busy
// ============================================================================

TEST(DccServiceModeCommon, begin_operation_fails_when_busy) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    EXPECT_TRUE(DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0));

    /* Second call while first operation is in progress */
    EXPECT_FALSE(DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0));

}

// ============================================================================
// NULL step callback completes without crash
// ============================================================================

TEST(DccServiceModeCommon, null_step_callback_completes_without_crash) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, NULL, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    drive_full_cycle_with_ack(true);

    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));
    EXPECT_EQ(step_callback_count, (uint32_t)0);

}

// ============================================================================
// run() in service mode while IDLE is a no-op
// ============================================================================

TEST(DccServiceModeCommon, run_in_idle_state_is_noop) {

    reset_mocks();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    DccServiceModeCommon_run(&test_context);

    EXPECT_EQ(load_packet_count, (uint32_t)0);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// Helper: drive pre-reset + transition to COMMAND state
// ============================================================================

static void drive_to_command_phase(void) {

    uint8_t packet_index;

    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* One more run to transition from RESET_PRE to COMMAND */
    DccServiceModeCommon_run(&test_context);

}

// ============================================================================
// Helper: drive post-reset to completion
// ============================================================================

static void drive_post_reset_to_idle(void) {

    uint8_t packet_index;

    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_POST_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Final run transitions to IDLE and fires callback */
    DccServiceModeCommon_run(&test_context);

}

// ============================================================================
// Helper: standard test setup (init, enter, begin_operation)
// ============================================================================

static interface_dcc_service_mode_common_t _test_interface;

static void setup_and_begin(void) {

    reset_mocks();
    reset_step_callback();
    _test_interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &_test_interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

}

// ============================================================================
// Helper: drive one no-ACK attempt (pre-reset + command + evaluation)
// After this, state machine is at RESET_PRE of next retry (or RESET_POST
// if retries exhausted). Does NOT drive post-reset.
// ============================================================================

static void drive_one_no_ack_attempt(void) {

    uint8_t packet_index;

    /* Pre-reset */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to COMMAND */
    DccServiceModeCommon_run(&test_context);

    /* Command packets */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Evaluation run: no ACK → retry or fail */
    DccServiceModeCommon_run(&test_context);

}

// ============================================================================
// ACK boundary: exact min-sample count (87) → SUCCESS
// ============================================================================

// @compliance DCC-S9.2.3-CS-001
TEST(DccServiceModeCommon, ack_exact_min_samples_detected) {

    setup_and_begin();
    drive_to_command_phase();

    /* Send first command packet */
    DccServiceModeCommon_run(&test_context);

    /* Feed exactly ACK_MIN_SAMPLES (85 = 5000/58 - 1) above-threshold samples */
    feed_ack_samples(85);

    /* Next run detects ACK → RESET_POST */
    DccServiceModeCommon_run(&test_context);
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK boundary: one below min-sample count (84) → NO_ACK
// ============================================================================

// @compliance DCC-S9.2.3-CS-002
TEST(DccServiceModeCommon, ack_one_below_min_samples_not_detected) {

    setup_and_begin();
    drive_to_command_phase();

    /* Send first command packet */
    DccServiceModeCommon_run(&test_context);

    /* Feed 84 samples — one short of ACK_MIN_SAMPLES (85) */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 84; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    DccServiceModeCommon_ack_sample(&test_context, 0);  /* end of pulse */

    /* Drive remaining command packets (4 more) */
    uint8_t packet_index;
    for (packet_index = 1; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Evaluate — no ACK → retry */
    DccServiceModeCommon_run(&test_context);

    /* Drive through all retries without ACK */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// ACK threshold boundary: sample at exactly threshold (60 mA) → detected
// ============================================================================

TEST(DccServiceModeCommon, ack_at_exact_threshold_detected) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */
    test_context.ack_window_open = true;      /* isolate detection: force the scan window open */

    /* Feed samples at exactly the threshold value */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 104; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA);

    }

    /* Falling edge: enough lows to pass the dropout filter. */
    for (sample_index = 0; sample_index < TEST_ACK_END_LOWS; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK threshold boundary: sample one below threshold (59 mA) → not detected
// ============================================================================

TEST(DccServiceModeCommon, ack_one_below_threshold_not_detected) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* Feed many samples at threshold - 1: should never trigger ACK */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 200; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA - 1);

    }

    /* Drive remaining command packets */
    uint8_t packet_index;
    for (packet_index = 1; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* evaluate → retry */

    /* Drive through all retries */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// ACK upper bound: pulse within [MIN, MAX] then low → SUCCESS
// (S-9.2.3 p.2: ACK is 6 ms +/- 1 ms, a two-sided window. MIN=85, MAX=120.)
// ============================================================================

// @compliance DCC-S9.2.3-CS-003
TEST(DccServiceModeCommon, ack_within_max_window_detected) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* 110 samples within [MIN(85), MAX(120)] -> valid ACK (helper opens the scan
     * window and ends the run past the dropout filter). */
    feed_ack_samples(110);

    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK upper bound: pulse longer than MAX then low → NO_ACK
// (S-9.2.3 p.3: a run longer than the upper bound is over-current, not an ACK.)
// ============================================================================

// @compliance DCC-S9.2.3-CS-004
TEST(DccServiceModeCommon, ack_beyond_max_window_not_detected) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* Feed 130 above-threshold samples: beyond MAX(120). This flags an
     * over-current run; the falling edge must NOT latch an ACK. */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 130; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Falling edge: over-current run is rejected, not accepted as ACK. */
    DccServiceModeCommon_ack_sample(&test_context, 0);

    /* Drive remaining command packets — no ACK should be present. */
    uint8_t packet_index;
    for (packet_index = 1; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* evaluate → retry */

    /* Drive through all retries without ACK */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// ACK during retry attempt → SUCCESS (retries stop)
// ============================================================================

TEST(DccServiceModeCommon, ack_during_retry_returns_success) {

    setup_and_begin();

    /* First attempt: no ACK — state machine retries (→ RESET_PRE) */
    drive_one_no_ack_attempt();

    /* Now cleanly at start of retry #1's RESET_PRE */
    drive_to_command_phase();

    /* ACK on first command packet of retry */
    DccServiceModeCommon_run(&test_context);
    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK on last retry attempt → SUCCESS
// ============================================================================

TEST(DccServiceModeCommon, ack_on_last_retry_returns_success) {

    setup_and_begin();

    /* Drive through initial attempt + all retries except the last, no ACK.
     * Each call leaves state machine cleanly at RESET_PRE of next retry. */
    uint8_t attempt;
    for (attempt = 0; attempt < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_one_no_ack_attempt();

    }

    /* Last retry: provide ACK */
    drive_to_command_phase();
    DccServiceModeCommon_run(&test_context);  /* first command packet */
    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Early termination after 2nd command packet → 2 packets loaded
// ============================================================================

TEST(DccServiceModeCommon, ack_after_second_command_packet_loads_two) {

    setup_and_begin();
    drive_to_command_phase();
    load_packet_count = 0;

    /* Send 2 command packets */
    DccServiceModeCommon_run(&test_context);
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);

    /* ACK detected after 2nd packet */
    feed_ack_samples(104);

    /* Next run detects ACK → RESET_POST */
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)2);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Early termination after 3rd command packet → 3 packets loaded
// ============================================================================

TEST(DccServiceModeCommon, ack_after_third_command_packet_loads_three) {

    setup_and_begin();
    drive_to_command_phase();
    load_packet_count = 0;

    /* Send 3 command packets */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < 3; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)3);

    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    EXPECT_EQ(load_packet_count, (uint32_t)3);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK after last (5th) command packet → SUCCESS, all 5 loaded
// ============================================================================

TEST(DccServiceModeCommon, ack_after_last_command_packet_loads_all) {

    setup_and_begin();
    drive_to_command_phase();
    load_packet_count = 0;

    /* Send all 5 command packets */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_COMMAND_REPEAT);

    /* ACK arrives after the last packet but before evaluation run */
    feed_ack_samples(104);

    /* Evaluation run: ACK detected → RESET_POST (not retry) */
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_COMMAND_REPEAT);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK samples during pre-reset phase → ignored
// ============================================================================

// @compliance DCC-S9.2.3-CS-006
TEST(DccServiceModeCommon, ack_during_pre_reset_ignored) {

    setup_and_begin();

    /* Feed ACK samples during pre-reset — should be ignored because
     * state is RESET_PRE, not COMMAND. */
    feed_ack_samples(200);

    /* Drive through pre-reset */
    drive_to_command_phase();

    /* Command phase: no ACK provided */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* evaluate → retry */

    /* Drive through all retries with no ACK */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// ACK samples during post-reset phase → do not change result
// ============================================================================

// @compliance DCC-S9.2.3-CS-006
TEST(DccServiceModeCommon, ack_during_post_reset_does_not_change_result) {

    setup_and_begin();

    /* Drive through with no ACK — all retries exhausted */
    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_to_command_phase();

        uint8_t packet_index;
        for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

            DccServiceModeCommon_run(&test_context);

        }

        DccServiceModeCommon_run(&test_context);  /* evaluate → retry or RESET_POST */

    }

    /* Now in RESET_POST with result = NO_ACK.
     * Feed ACK samples — should be ignored. */
    feed_ack_samples(200);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// Interrupted pulse (gap resets high counter) → NO_ACK
// ============================================================================

// @compliance DCC-S9.2.3-CS-005
TEST(DccServiceModeCommon, interrupted_pulse_resets_counter) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* Feed 80 samples (just under 87 min), then drop, then 80 more.
     * Neither burst should trigger ACK because the gap resets the counter. */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 80; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Gap — resets _ack_high_count to 0 */
    DccServiceModeCommon_ack_sample(&test_context, 0);

    for (sample_index = 0; sample_index < 80; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    DccServiceModeCommon_ack_sample(&test_context, 0);

    /* Drive remaining command packets */
    uint8_t packet_index;
    for (packet_index = 1; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* evaluate → retry */

    /* Drive through all retries with no ACK */
    uint8_t retry;
    for (retry = 0; retry < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; retry++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// Second operation after first completes → succeeds
// ============================================================================

TEST(DccServiceModeCommon, second_operation_after_completion_succeeds) {

    setup_and_begin();

    /* Complete first operation with ACK */
    drive_full_cycle_with_ack(true);

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

    /* Start second operation */
    reset_step_callback();
    dcc_packet_t packet = make_command_packet();
    EXPECT_TRUE(DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0));

    /* Complete second operation with ACK */
    drive_full_cycle_with_ack(true);

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// Second operation completes with NO_ACK after first succeeded
// ============================================================================

TEST(DccServiceModeCommon, second_operation_no_ack_after_first_succeeded) {

    setup_and_begin();

    /* First operation: ACK → SUCCESS */
    drive_full_cycle_with_ack(true);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

    /* Second operation: no ACK → NO_ACK */
    reset_step_callback();
    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// NULL callback with NO_ACK completes without crash
// ============================================================================

TEST(DccServiceModeCommon, null_callback_no_ack_completes_without_crash) {

    reset_mocks();
    reset_step_callback();
    interface_dcc_service_mode_common_t interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, NULL, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    /* Drive all attempts with no ACK */
    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_full_cycle_with_ack(false);

    }

    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));
    EXPECT_EQ(step_callback_count, (uint32_t)0);

}

// ============================================================================
// Duplicate ACK samples after detection are ignored
// ============================================================================

TEST(DccServiceModeCommon, duplicate_ack_samples_after_detection_ignored) {

    setup_and_begin();
    drive_to_command_phase();

    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* Trigger ACK */
    feed_ack_samples(104);

    /* Feed more samples — should be harmless (ack_sample returns early
     * once _ack_detected is true). */
    feed_ack_samples(200);
    feed_ack_samples(200);

    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Encoder stall mid-command: ACK still detected when encoder resumes
// ============================================================================

TEST(DccServiceModeCommon, encoder_stall_mid_command_ack_still_detected) {

    setup_and_begin();
    drive_to_command_phase();

    /* Send first command packet */
    DccServiceModeCommon_run(&test_context);

    /* Encoder stalls — run() should not advance */
    encoder_idle_value = false;
    DccServiceModeCommon_run(&test_context);
    DccServiceModeCommon_run(&test_context);

    /* Meanwhile, ACK arrives from decoder (ack_sample is ISR-driven,
     * independent of encoder state). */
    feed_ack_samples(104);

    /* Encoder resumes — state machine should see ACK and go to RESET_POST */
    encoder_idle_value = true;
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Post-reset sends correct packet count
// ============================================================================

// @compliance DCC-S9.2.3-CS-008
TEST(DccServiceModeCommon, post_reset_sends_correct_packet_count) {

    setup_and_begin();
    drive_to_command_phase();

    /* ACK on first command */
    DccServiceModeCommon_run(&test_context);
    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);  /* → RESET_POST */

    load_packet_count = 0;

    /* Drive post-reset */
    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_POST_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_RESET_POST_COUNT);

    /* Verify they are reset packets */
    EXPECT_EQ(last_loaded_packet.data[0], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.data[1], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

    /* Final run → IDLE */
    DccServiceModeCommon_run(&test_context);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// Total retry count verified: initial + retries = 4 full attempts
// ============================================================================

TEST(DccServiceModeCommon, total_attempt_count_is_initial_plus_retries) {

    setup_and_begin();

    /* Drive all attempts (1 initial + 3 retries = 4) without ACK */
    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_full_cycle_with_ack(false);

    }

    /* Each attempt: PRE_RESET(3) + COMMAND(5) + POST_RESET(6) on last,
     * or PRE_RESET(3) + COMMAND(5) for retries that loop back.
     * Total command packets = 5 * 4 = 20.
     * Total packets overall: first 3 attempts each produce 3+5=8 packets
     * that feed into retry. Last attempt: 3+5+6=14. Plus transitions.
     * Just verify the callback result. */
    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);

}

// ============================================================================
// ACK pulse spanning two command packets still detected
// ============================================================================

TEST(DccServiceModeCommon, ack_spanning_command_packets_detected) {

    setup_and_begin();
    drive_to_command_phase();

    /* Send first command packet */
    DccServiceModeCommon_run(&test_context);
    test_context.ack_window_open = true;   /* isolate detection: force the scan window open */

    /* Start ACK pulse — 50 samples (not enough yet) */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 50; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Second command packet loads while ACK pulse continues */
    DccServiceModeCommon_run(&test_context);

    /* Continue ACK pulse — 50 more samples (total 100, above min) */
    for (sample_index = 0; sample_index < 50; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Falling edge: enough lows to pass the dropout filter. */
    for (sample_index = 0; sample_index < TEST_ACK_END_LOWS; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

    /* Next run detects ACK → RESET_POST */
    DccServiceModeCommon_run(&test_context);
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write operation helpers
// ============================================================================

static void setup_and_begin_write(void) {

    reset_mocks();
    reset_step_callback();
    _test_interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &_test_interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback,
                                         true, DCC_SERVICE_MODE_COMMAND_REPEAT, DCC_SERVICE_MODE_RECOVERY_COUNT);

}

static void drive_one_write_no_ack_attempt(void) {

    uint8_t packet_index;

    /* Pre-reset */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to COMMAND */
    DccServiceModeCommon_run(&test_context);

    /* Command packets */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to RECOVERY (write operation) */
    DccServiceModeCommon_run(&test_context);

    /* Recovery packets */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RECOVERY_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Evaluation run: no ACK → retry or fail */
    DccServiceModeCommon_run(&test_context);

}

static void drive_write_full_cycle_with_ack(bool provide_ack) {

    uint8_t packet_index;

    /* Pre-reset */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to COMMAND */
    DccServiceModeCommon_run(&test_context);

    if (provide_ack) {

        DccServiceModeCommon_run(&test_context);
        feed_ack_samples(104);
        DccServiceModeCommon_run(&test_context);

    } else {

        for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

            DccServiceModeCommon_run(&test_context);

        }

        /* Transition to RECOVERY */
        DccServiceModeCommon_run(&test_context);

        for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RECOVERY_COUNT; packet_index++) {

            DccServiceModeCommon_run(&test_context);

        }

        /* Evaluation: no ACK → retry or fail */
        DccServiceModeCommon_run(&test_context);

    }

    /* Post-reset */
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_POST_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);

}

// ============================================================================
// Write: ACK during command phase → SUCCESS (recovery skipped)
// ============================================================================

TEST(DccServiceModeCommon, write_ack_during_command_returns_success) {

    setup_and_begin_write();

    drive_write_full_cycle_with_ack(true);

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// Write: no ACK during command, ACK on 1st recovery packet → SUCCESS
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_first_recovery_packet) {

    setup_and_begin_write();
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to RECOVERY */
    DccServiceModeCommon_run(&test_context);

    /* First recovery packet */
    DccServiceModeCommon_run(&test_context);

    /* ACK arrives during first recovery packet */
    feed_ack_samples(104);

    /* Detect ACK → RESET_POST */
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: no ACK during command, ACK on 3rd recovery packet → SUCCESS
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_third_recovery_packet) {

    setup_and_begin_write();
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to RECOVERY */
    DccServiceModeCommon_run(&test_context);

    for (packet_index = 0; packet_index < 3; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: ACK on last recovery packet → SUCCESS
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_last_recovery_packet) {

    setup_and_begin_write();
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to RECOVERY */
    DccServiceModeCommon_run(&test_context);

    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RECOVERY_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* ACK after last recovery packet */
    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: no ACK through command + recovery → retries → NO_ACK
// ============================================================================

TEST(DccServiceModeCommon, write_no_ack_through_recovery_retries_then_fails) {

    setup_and_begin_write();

    uint8_t attempt;
    for (attempt = 0; attempt <= USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_write_full_cycle_with_ack(false);

    }

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_NO_ACK);
    EXPECT_TRUE(DccServiceModeCommon_is_idle(&test_context));

}

// ============================================================================
// Write: no ACK 1st attempt, ACK on retry command phase
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_retry_command_phase) {

    setup_and_begin_write();

    drive_one_write_no_ack_attempt();

    /* Retry #1: ACK during command phase */
    drive_to_command_phase();
    DccServiceModeCommon_run(&test_context);
    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: no ACK 1st attempt, ACK on retry recovery phase
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_retry_recovery_phase) {

    setup_and_begin_write();

    drive_one_write_no_ack_attempt();

    /* Retry #1: no ACK during command, ACK during recovery */
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Transition to RECOVERY */
    DccServiceModeCommon_run(&test_context);

    /* 2 recovery packets, then ACK */
    DccServiceModeCommon_run(&test_context);
    DccServiceModeCommon_run(&test_context);
    feed_ack_samples(104);

    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: ACK on last retry's recovery phase → SUCCESS
// ============================================================================

TEST(DccServiceModeCommon, write_ack_on_last_retry_recovery_phase) {

    setup_and_begin_write();

    uint8_t attempt;
    for (attempt = 0; attempt < USER_DEFINED_DCC_SERVICE_MODE_RETRIES; attempt++) {

        drive_one_write_no_ack_attempt();

    }

    /* Last retry: no ACK during command, ACK on 4th recovery packet */
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* → RECOVERY */

    for (packet_index = 0; packet_index < 4; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Write: recovery sends correct number of reset packets
// ============================================================================

TEST(DccServiceModeCommon, write_recovery_sends_correct_packet_count) {

    setup_and_begin_write();
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* → RECOVERY */
    load_packet_count = 0;

    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RECOVERY_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_RECOVERY_COUNT);
    EXPECT_EQ(last_loaded_packet.data[0], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.data[1], DCC_RESET_BYTE);
    EXPECT_EQ(last_loaded_packet.preamble_bits, DCC_PREAMBLE_BITS_SERVICE);

}

// ============================================================================
// Write: ACK spanning command into recovery still detected
// ============================================================================

TEST(DccServiceModeCommon, write_ack_spanning_command_into_recovery) {

    setup_and_begin_write();
    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    /* Start ACK pulse during last command evaluation */
    uint16_t sample_index;
    for (sample_index = 0; sample_index < 50; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Transition to RECOVERY */
    DccServiceModeCommon_run(&test_context);
    DccServiceModeCommon_run(&test_context);

    /* Continue ACK pulse in recovery — total > 87 */
    for (sample_index = 0; sample_index < 50; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

    /* Falling edge: enough lows to pass the dropout filter. */
    for (sample_index = 0; sample_index < TEST_ACK_END_LOWS; sample_index++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

    DccServiceModeCommon_run(&test_context);  /* detect ACK → RESET_POST */
    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// Verify: no recovery phase (skips straight to retry)
// ============================================================================

TEST(DccServiceModeCommon, verify_no_recovery_phase) {

    setup_and_begin();
    drive_to_command_phase();
    load_packet_count = 0;

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)DCC_SERVICE_MODE_COMMAND_REPEAT);

    /* Next run: should go to retry (RESET_PRE), NOT recovery */
    uint32_t packets_before = load_packet_count;
    DccServiceModeCommon_run(&test_context);

    /* Should be at RESET_PRE — next run sends a reset packet */
    DccServiceModeCommon_run(&test_context);
    EXPECT_EQ(load_packet_count, packets_before + 1);
    EXPECT_EQ(last_loaded_packet.data[0], DCC_RESET_BYTE);

}

// ============================================================================
// Write: custom recovery count (10 for address-only mode)
// ============================================================================

TEST(DccServiceModeCommon, write_custom_recovery_count) {

    reset_mocks();
    reset_step_callback();
    _test_interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &_test_interface);
    DccServiceModeCommon_enter(&test_context);

    dcc_packet_t packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &packet, mock_step_callback,
                                         true, DCC_SERVICE_MODE_COMMAND_REPEAT, 10);

    drive_to_command_phase();

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_COMMAND_REPEAT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* → RECOVERY */
    load_packet_count = 0;

    for (packet_index = 0; packet_index < 10; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    EXPECT_EQ(load_packet_count, (uint32_t)10);

    feed_ack_samples(104);
    DccServiceModeCommon_run(&test_context);

    drive_post_reset_to_idle();

    EXPECT_EQ(step_callback_count, (uint32_t)1);
    EXPECT_EQ(step_result, DCC_SERVICE_MODE_SUCCESS);

}

// ============================================================================
// ACK pulse-width window boundary tests (S-9.2.3 §D: 6 ms +/- 1 ms)
//
// The width counter accepts a high run as an ACK only when, on the falling
// edge, the run length is in [ACK_MIN_SAMPLES, ACK_MAX_SAMPLES] (inclusive) and
// not flagged as over-current. These constants mirror the formulas in
// dcc_service_mode_common.c so the tests track the configured window.
// ============================================================================

static const uint16_t ACK_MIN_SAMPLES =
    (uint16_t)((USER_DEFINED_DCC_ACK_MIN_DURATION_US / DCC_ONE_BIT_HALF_PERIOD_US) - 1);
static const uint16_t ACK_MAX_SAMPLES =
    (uint16_t)(USER_DEFINED_DCC_ACK_MAX_DURATION_US / DCC_ONE_BIT_HALF_PERIOD_US);

static interface_dcc_service_mode_common_t _bm_interface;
static dcc_packet_t _bm_packet;

// Drive the state machine to COMMAND state with the first command packet sent,
// so subsequent ack_sample() calls are evaluated. ACK counters are reset to 0
// on the RESET_PRE -> COMMAND transition.
static void reach_command_first_packet(void) {

    reset_mocks();
    reset_step_callback();
    _bm_interface = make_interface();
    DccServiceModeCommon_initialize(&test_context, &_bm_interface);
    DccServiceModeCommon_enter(&test_context);

    _bm_packet = make_command_packet();
    DccServiceModeCommon_begin_operation(&test_context, &_bm_packet, mock_step_callback, false, DCC_SERVICE_MODE_COMMAND_REPEAT, 0);

    uint8_t packet_index;
    for (packet_index = 0; packet_index < DCC_SERVICE_MODE_RESET_PRE_COUNT; packet_index++) {

        DccServiceModeCommon_run(&test_context);

    }

    DccServiceModeCommon_run(&test_context);  /* RESET_PRE -> COMMAND */
    DccServiceModeCommon_run(&test_context);  /* first command packet */

    /* These tests inject the ACK directly via ack_sample() to unit-test the
     * width/dropout detection in isolation; force the scan window open (the
     * blanking/window-opening logic is covered by the packet-position tests). */
    test_context.ack_window_open = true;

}

// Feed n consecutive above-threshold samples (no falling edge).
static void feed_high(uint16_t n) {

    uint16_t i;
    for (i = 0; i < n; i++) {

        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);

    }

}

// Close a run: feed enough below-threshold samples to pass the dropout filter
// (a single dip is bridged, so a real falling edge needs > DROPOUT_SAMPLES lows).
static void feed_low(void) {

    uint16_t i;
    for (i = 0; i < TEST_ACK_END_LOWS; i++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

}

// Feed a single sub-threshold dip (bridged by the dropout filter, does NOT end
// the run unless it pushes the consecutive-low count past DROPOUT_SAMPLES).
static void feed_dip(uint16_t n) {

    uint16_t i;
    for (i = 0; i < n; i++) {

        DccServiceModeCommon_ack_sample(&test_context, 0);

    }

}

TEST(DccServiceModeCommon, ack_window_min_minus_one_not_detected) {

    reach_command_first_packet();
    feed_high((uint16_t)(ACK_MIN_SAMPLES - 1));
    feed_low();

    EXPECT_FALSE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_exactly_min_detected) {

    reach_command_first_packet();
    feed_high(ACK_MIN_SAMPLES);
    feed_low();

    EXPECT_TRUE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_exactly_max_detected) {

    reach_command_first_packet();
    feed_high(ACK_MAX_SAMPLES);
    feed_low();

    EXPECT_TRUE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_max_plus_one_overrun_not_detected) {

    reach_command_first_packet();
    feed_high((uint16_t)(ACK_MAX_SAMPLES + 1));
    feed_low();

    EXPECT_FALSE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_sustained_overcurrent_not_detected) {

    reach_command_first_packet();
    feed_high((uint16_t)(ACK_MAX_SAMPLES * 2));
    feed_low();

    EXPECT_FALSE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_high_run_without_falling_edge_not_detected) {

    reach_command_first_packet();
    feed_high((uint16_t)(ACK_MIN_SAMPLES + 5));
    /* No falling edge — detection only happens when the run ends. */

    EXPECT_FALSE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_multiple_short_blips_not_detected) {

    reach_command_first_packet();

    uint8_t blip;
    for (blip = 0; blip < 3; blip++) {

        feed_high((uint16_t)(ACK_MIN_SAMPLES - 20));
        feed_low();

    }

    EXPECT_FALSE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_short_blip_then_valid_run_detected) {

    reach_command_first_packet();

    /* A too-short blip must reset the run counter so a following valid run
     * is still recognized. */
    feed_high((uint16_t)(ACK_MIN_SAMPLES - 20));
    feed_low();
    feed_high(ACK_MIN_SAMPLES);
    feed_low();

    EXPECT_TRUE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_detection_is_idempotent) {

    reach_command_first_packet();

    feed_high(ACK_MIN_SAMPLES);
    feed_low();
    EXPECT_TRUE(test_context.ack_detected);

    /* Once latched, further samples (even an over-current run) are ignored and
     * the result stays detected. */
    feed_high((uint16_t)(ACK_MAX_SAMPLES * 2));
    feed_low();
    EXPECT_TRUE(test_context.ack_detected);

}

// ============================================================================
// Dropout filter: a brief sub-threshold dip is bridged (the noisy-motor case);
// a dip longer than the tolerance is a real falling edge that splits the run.
// ============================================================================

TEST(DccServiceModeCommon, ack_window_brief_dropout_is_bridged) {

    reach_command_first_packet();

    /* 50 high, a tolerated dip, 50 high -> one ~100-sample span (in [MIN,MAX]). */
    feed_high(50);
    feed_dip((uint16_t)(TEST_ACK_END_LOWS - 1));   /* DROPOUT_SAMPLES lows: bridged */
    feed_high(50);
    feed_low();

    EXPECT_TRUE(test_context.ack_detected);

}

TEST(DccServiceModeCommon, ack_window_deep_dropout_splits_run) {

    reach_command_first_packet();

    /* Same samples but the dip exceeds the tolerance, so it ends the first run
     * (50 < MIN) and the second 50-run is also too short -> not detected. */
    feed_high(50);
    feed_dip(TEST_ACK_END_LOWS);                   /* > DROPOUT_SAMPLES: real falling edge */
    feed_high(50);
    feed_low();

    EXPECT_FALSE(test_context.ack_detected);

}

// ============================================================================
// ACK scan-window blanking (S-9.2.3 line 55): a full valid ACK injected during
// the first BLANK_PACKETS command packets is ignored; only from the next packet
// on is it detected. Uses the REAL packet-count gating (no forced window).
// ============================================================================

TEST(DccServiceModeCommon, ack_blanked_until_window_opens) {

    setup_and_begin();
    drive_to_command_phase();

    uint16_t i;
    uint8_t pkt;

    /* During each blanked command packet, a full valid ACK must be ignored. */
    for (pkt = 0; pkt < DCC_SERVICE_MODE_ACK_BLANK_PACKETS; pkt++) {

        DccServiceModeCommon_run(&test_context);   /* load command packet (1..BLANK) */

        for (i = 0; i < 100; i++) {
            DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);
        }
        for (i = 0; i < TEST_ACK_END_LOWS; i++) {
            DccServiceModeCommon_ack_sample(&test_context, 0);
        }

        EXPECT_FALSE(test_context.ack_detected);   /* blanked */

    }

    /* Next command packet opens the window -> the same ACK is now detected. */
    DccServiceModeCommon_run(&test_context);
    for (i = 0; i < 100; i++) {
        DccServiceModeCommon_ack_sample(&test_context, USER_DEFINED_DCC_ACK_THRESHOLD_MA + 10);
    }
    for (i = 0; i < TEST_ACK_END_LOWS; i++) {
        DccServiceModeCommon_ack_sample(&test_context, 0);
    }

    EXPECT_TRUE(test_context.ack_detected);

}
