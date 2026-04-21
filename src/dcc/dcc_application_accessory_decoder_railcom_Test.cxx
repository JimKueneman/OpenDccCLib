/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file dcc_application_accessory_decoder_railcom_Test.cxx
 * @brief GoogleTest suite for dcc_application_accessory_decoder_railcom.c
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_accessory_decoder_railcom.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_ACCESSORY_DECODER

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint32_t send_ch1_count = 0;
static uint8_t last_ch1_id = 0;
static uint8_t last_ch1_data = 0;

static uint32_t send_ch2_count = 0;
static dcc_railcom_response_t last_ch2_response;

static void mock_send_ch1(uint8_t datagram_id, uint8_t data) {

    send_ch1_count++;
    last_ch1_id = datagram_id;
    last_ch1_data = data;

}

static void mock_send_ch2(const dcc_railcom_response_t *response) {

    send_ch2_count++;
    last_ch2_response = *response;

}

static void reset_mocks(void) {

    send_ch1_count = 0;
    last_ch1_id = 0xFF;
    last_ch1_data = 0xFF;
    send_ch2_count = 0;
    memset(&last_ch2_response, 0, sizeof(last_ch2_response));

}

static const interface_dcc_application_accessory_decoder_railcom_t mock_interface = {

    .send_ch1 = mock_send_ch1,
    .send_ch2 = mock_send_ch2

};

// ============================================================================
// Test fixture
// ============================================================================

class AccDecoderRailcomTest : public ::testing::Test {

protected:

    void SetUp() override {

        reset_mocks();
        DccApplicationAccessoryDecoderRailcom_initialize(&mock_interface);

    }

};

// ============================================================================
// Initialize tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, initialize_resets_state) {

    /* Put module into a non-default state, then re-initialize */
    DccApplicationAccessoryDecoderRailcom_send_srq(0x1AB, true);
    DccApplicationAccessoryDecoderRailcom_initialize(&mock_interface);

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

TEST_F(AccDecoderRailcomTest, initialize_with_null_interface) {

    DccApplicationAccessoryDecoderRailcom_initialize(NULL);

    /* on_cutout should silently return when interface is NULL */
    DccApplicationAccessoryDecoderRailcom_send_srq(0x0055, false);
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 0u);

}

// ============================================================================
// send_srq tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_srq_sets_pending_state) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0010, false);

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_PENDING);

}

TEST_F(AccDecoderRailcomTest, send_srq_stores_address) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x00AB, false);

    /* Trigger cutout so the stored address is sent via ch1 */
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 1u);
    EXPECT_EQ(last_ch1_data, 0xAB);

}

// ============================================================================
// send_status tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_status_packs_aspect_only) {

    DccApplicationAccessoryDecoderRailcom_send_status(0x15, false, false);

    /* Drive the response out via the cutout path */
    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch2_count, 1u);
    EXPECT_EQ(last_ch2_response.datagram_id, 4);
    EXPECT_EQ(last_ch2_response.count, 1);
    EXPECT_EQ(last_ch2_response.data[0], 0x15);

}

TEST_F(AccDecoderRailcomTest, send_status_packs_command_match) {

    DccApplicationAccessoryDecoderRailcom_send_status(0x00, true, false);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.data[0], (1 << 5));

}

TEST_F(AccDecoderRailcomTest, send_status_packs_is_setpoint) {

    DccApplicationAccessoryDecoderRailcom_send_status(0x00, false, true);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.data[0], (1 << 6));

}

TEST_F(AccDecoderRailcomTest, send_status_packs_all_flags) {

    DccApplicationAccessoryDecoderRailcom_send_status(0x1F, true, true);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    /* 0x1F | (1<<5) | (1<<6) = 0x7F */
    EXPECT_EQ(last_ch2_response.data[0], 0x7F);

}

// ============================================================================
// send_status_extended tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_status_extended_packs_two_datagrams) {

    /* aspect_state = 0xAB = 10101011
     * low 5 bits = 0x0B, upper 3 bits = 0x05 */
    DccApplicationAccessoryDecoderRailcom_send_status_extended(0xAB, false, false);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch2_count, 1u);
    EXPECT_EQ(last_ch2_response.datagram_id, 4);
    EXPECT_EQ(last_ch2_response.count, 2);
    EXPECT_EQ(last_ch2_response.data[0], 0x0B);
    EXPECT_EQ(last_ch2_response.data[1], 0x05);

}

TEST_F(AccDecoderRailcomTest, send_status_extended_flags) {

    DccApplicationAccessoryDecoderRailcom_send_status_extended(0x00, true, true);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    /* data[0] should have bit5=command_match, bit6=is_setpoint */
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)((1 << 5) | (1 << 6)));
    EXPECT_EQ(last_ch2_response.data[1], 0x00);

}

// ============================================================================
// send_status_4 test
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_status_4_delegates) {

    DccApplicationAccessoryDecoderRailcom_send_status_4(0xCD);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.datagram_id, 3);
    EXPECT_EQ(last_ch2_response.data[0], 0xCD);
    EXPECT_EQ(last_ch2_response.count, 1);

}

// ============================================================================
// send_time_report tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_time_report_no_resolution) {

    DccApplicationAccessoryDecoderRailcom_send_time_report(42, false);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.datagram_id, 5);
    EXPECT_EQ(last_ch2_response.data[0], 42);
    EXPECT_EQ(last_ch2_response.count, 1);

}

TEST_F(AccDecoderRailcomTest, send_time_report_with_resolution) {

    DccApplicationAccessoryDecoderRailcom_send_time_report(42, true);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)(42 | (1 << 7)));

}

// ============================================================================
// send_error_report tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, send_error_report_no_additional) {

    DccApplicationAccessoryDecoderRailcom_send_error_report(7, false);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.datagram_id, 6);
    EXPECT_EQ(last_ch2_response.data[0], 7);
    EXPECT_EQ(last_ch2_response.count, 1);

}

TEST_F(AccDecoderRailcomTest, send_error_report_with_additional) {

    DccApplicationAccessoryDecoderRailcom_send_error_report(7, true);

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)(7 | (1 << 6)));

}

// ============================================================================
// get_srq_state tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, get_srq_state_returns_idle_after_init) {

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

TEST_F(AccDecoderRailcomTest, get_srq_state_returns_pending_after_srq) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0010, false);

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_PENDING);

}

// ============================================================================
// on_stop_command tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, on_stop_command_transitions_pending_to_responding) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0010, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_RESPONDING);

}

TEST_F(AccDecoderRailcomTest, on_stop_command_no_effect_when_idle) {

    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

TEST_F(AccDecoderRailcomTest, on_stop_command_no_effect_when_responding) {

    /* Get to RESPONDING state */
    DccApplicationAccessoryDecoderRailcom_send_srq(0x0010, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_RESPONDING);

    /* Second on_stop_command should not change state */
    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_RESPONDING);

}

// ============================================================================
// on_cutout tests
// ============================================================================

TEST_F(AccDecoderRailcomTest, on_cutout_null_interface_guard) {

    DccApplicationAccessoryDecoderRailcom_initialize(NULL);
    DccApplicationAccessoryDecoderRailcom_send_srq(0x0055, false);

    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 0u);
    EXPECT_EQ(send_ch2_count, 0u);

}

TEST_F(AccDecoderRailcomTest, on_cutout_pending_sends_ch1_basic) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x00AB, false);

    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 1u);
    EXPECT_EQ(last_ch1_id, DCC_RAILCOM_ID_MOBILITY_0);
    EXPECT_EQ(last_ch1_data, 0xAB);

}

TEST_F(AccDecoderRailcomTest, on_cutout_pending_sends_ch1_extended) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x03FF, true);

    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 1u);
    EXPECT_EQ(last_ch1_id, DCC_RAILCOM_ID_MOBILITY_0);
    EXPECT_EQ(last_ch1_data, 0xFF);

}

TEST_F(AccDecoderRailcomTest, on_cutout_responding_with_pending_sends_ch2_and_goes_idle) {

    DccApplicationAccessoryDecoderRailcom_send_status(0x0A, false, false);
    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch2_count, 1u);
    EXPECT_EQ(last_ch2_response.datagram_id, 4);
    EXPECT_EQ(last_ch2_response.data[0], 0x0A);
    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

TEST_F(AccDecoderRailcomTest, on_cutout_responding_without_pending_goes_idle) {

    DccApplicationAccessoryDecoderRailcom_send_srq(0x0001, false);
    DccApplicationAccessoryDecoderRailcom_on_stop_command();

    /* No send_status / send_time_report / etc. queued */
    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch2_count, 0u);
    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

TEST_F(AccDecoderRailcomTest, on_cutout_idle_does_nothing) {

    DccApplicationAccessoryDecoderRailcom_on_cutout();

    EXPECT_EQ(send_ch1_count, 0u);
    EXPECT_EQ(send_ch2_count, 0u);
    EXPECT_EQ(DccApplicationAccessoryDecoderRailcom_get_srq_state(), DCC_ACC_SRQ_IDLE);

}

#endif /* DCC_COMPILE_ACCESSORY_DECODER */
