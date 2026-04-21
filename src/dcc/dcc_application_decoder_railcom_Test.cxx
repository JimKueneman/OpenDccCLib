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
 * Test suite for DCC Application Decoder RailCom
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_decoder_railcom.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#if defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER)

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint32_t send_ch1_count = 0;
static uint8_t last_ch1_datagram_id = 0;
static uint8_t last_ch1_data = 0;

static uint32_t send_ch2_count = 0;
static dcc_railcom_response_t last_ch2_response;

static void mock_send_ch1(uint8_t datagram_id, uint8_t data) {

    send_ch1_count++;
    last_ch1_datagram_id = datagram_id;
    last_ch1_data = data;

}

static void mock_send_ch2(const dcc_railcom_response_t *response) {

    send_ch2_count++;
    last_ch2_response = *response;

}

static void reset_mocks(void) {

    send_ch1_count = 0;
    last_ch1_datagram_id = 0;
    last_ch1_data = 0;
    send_ch2_count = 0;
    memset(&last_ch2_response, 0, sizeof(last_ch2_response));

}

static interface_dcc_application_decoder_railcom_t make_interface(void) {

    interface_dcc_application_decoder_railcom_t iface;
    memset(&iface, 0, sizeof(iface));

    iface.send_ch1 = mock_send_ch1;
    iface.send_ch2 = mock_send_ch2;

    return iface;

}

// ============================================================================
// Initialization
// ============================================================================

TEST(DccApplicationDecoderRailcom, initialize_does_not_crash) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

}

// ============================================================================
// Mobile Decoder Only tests
// ============================================================================

#ifdef DCC_COMPILE_DECODER

// --- send_address_feedback ---

TEST(DccApplicationDecoderRailcom, send_address_feedback_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_address_feedback(0x1234);

    EXPECT_EQ(send_ch1_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_address_feedback_first_call_sends_adr1) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t address = 0x0A5B;
    DccApplicationDecoderRailcom_send_address_feedback(address);

    EXPECT_EQ(send_ch1_count, (uint32_t)1);
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)1);
    EXPECT_EQ(last_ch1_data, (uint8_t)(address & 0xFF));

}

TEST(DccApplicationDecoderRailcom, send_address_feedback_second_call_sends_adr2) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t address = 0x0A5B;
    DccApplicationDecoderRailcom_send_address_feedback(address);
    DccApplicationDecoderRailcom_send_address_feedback(address);

    EXPECT_EQ(send_ch1_count, (uint32_t)2);
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)2);
    EXPECT_EQ(last_ch1_data, (uint8_t)((address >> 8) & 0x3F));

}

TEST(DccApplicationDecoderRailcom, send_address_feedback_alternates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t address = 0x0A5B;

    /* 1st call: ADR1 */
    DccApplicationDecoderRailcom_send_address_feedback(address);
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)1);

    /* 2nd call: ADR2 */
    DccApplicationDecoderRailcom_send_address_feedback(address);
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)2);

    /* 3rd call: back to ADR1 */
    DccApplicationDecoderRailcom_send_address_feedback(address);
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)1);
    EXPECT_EQ(last_ch1_data, (uint8_t)(address & 0xFF));

}

TEST(DccApplicationDecoderRailcom, send_address_feedback_reinitialize_resets_alternate) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t address = 0x0A5B;

    /* Advance to ADR2 state */
    DccApplicationDecoderRailcom_send_address_feedback(address);

    /* Re-initialize should reset the alternate flag */
    DccApplicationDecoderRailcom_initialize(&iface);
    DccApplicationDecoderRailcom_send_address_feedback(address);

    /* Should be ADR1 again (id=1) after re-init */
    EXPECT_EQ(last_ch1_datagram_id, (uint8_t)1);

}

// --- send_track_search_response ---

TEST(DccApplicationDecoderRailcom, send_track_search_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_track_search_response(0x1234, 42);

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_track_search_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t address = 0x1A3C;
    uint8_t seconds = 55;
    DccApplicationDecoderRailcom_send_track_search_response(address, seconds);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)2);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)(address & 0xFF));
    EXPECT_EQ(last_ch2_response.data[1], (uint8_t)((address >> 8) & 0x3F));
    EXPECT_EQ(last_ch2_response.data[2], seconds);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)3);

}

// --- send_cv_auto_transfer ---

TEST(DccApplicationDecoderRailcom, send_cv_auto_transfer_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_cv_auto_transfer(0x123456, 0xAB);

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_cv_auto_transfer_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint32_t indexed_cv = 0x1A2B3C;
    uint8_t value = 0xDD;
    DccApplicationDecoderRailcom_send_cv_auto_transfer(indexed_cv, value);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)12);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)(indexed_cv & 0xFF));
    EXPECT_EQ(last_ch2_response.data[1], (uint8_t)((indexed_cv >> 8) & 0xFF));
    EXPECT_EQ(last_ch2_response.data[2], (uint8_t)((indexed_cv >> 16) & 0xFF));
    EXPECT_EQ(last_ch2_response.data[3], value);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)4);

}

#endif /* DCC_COMPILE_DECODER */

// ============================================================================
// Shared tests (mobile + accessory decoder)
// ============================================================================

// --- send_pom_response ---

TEST(DccApplicationDecoderRailcom, send_pom_response_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_pom_response(0x00FF, 0x42);

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_pom_response_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint16_t cv_addr = 0x01FF;
    uint8_t value = 0xAB;
    DccApplicationDecoderRailcom_send_pom_response(cv_addr, value);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)0);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)(cv_addr & 0xFF));
    EXPECT_EQ(last_ch2_response.data[1], value);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)2);

}

// --- send_dynamic_data ---

TEST(DccApplicationDecoderRailcom, send_dynamic_data_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_dynamic_data(3, 100);

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_dynamic_data_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint8_t subid = 5;
    uint8_t value = 200;
    DccApplicationDecoderRailcom_send_dynamic_data(subid, value);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)7);
    EXPECT_EQ(last_ch2_response.data[0], subid);
    EXPECT_EQ(last_ch2_response.data[1], value);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)2);

}

// --- send_ack ---

TEST(DccApplicationDecoderRailcom, send_ack_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_ack();

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_ack_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    DccApplicationDecoderRailcom_send_ack();

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)15);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)0x00);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)1);

}

// --- send_nack ---

TEST(DccApplicationDecoderRailcom, send_nack_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    DccApplicationDecoderRailcom_send_nack();

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_nack_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    DccApplicationDecoderRailcom_send_nack();

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)15);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)0x01);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)1);

}

// --- send_raw ---

TEST(DccApplicationDecoderRailcom, send_raw_null_guard) {

    reset_mocks();
    DccApplicationDecoderRailcom_initialize(NULL);

    uint8_t data[] = {0x11, 0x22};
    DccApplicationDecoderRailcom_send_raw(5, data, 2);

    EXPECT_EQ(send_ch2_count, (uint32_t)0);

}

TEST(DccApplicationDecoderRailcom, send_raw_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    DccApplicationDecoderRailcom_send_raw(9, data, 3);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.datagram_id, (uint8_t)9);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)0xAA);
    EXPECT_EQ(last_ch2_response.data[1], (uint8_t)0xBB);
    EXPECT_EQ(last_ch2_response.data[2], (uint8_t)0xCC);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)3);

}

TEST(DccApplicationDecoderRailcom, send_raw_clamps_count) {

    reset_mocks();
    interface_dcc_application_decoder_railcom_t iface = make_interface();
    DccApplicationDecoderRailcom_initialize(&iface);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    DccApplicationDecoderRailcom_send_raw(3, data, 10);

    EXPECT_EQ(send_ch2_count, (uint32_t)1);
    EXPECT_EQ(last_ch2_response.count, (uint8_t)DCC_RAILCOM_DATAGRAM_MAX_BYTES);
    EXPECT_EQ(last_ch2_response.data[0], (uint8_t)0x01);
    EXPECT_EQ(last_ch2_response.data[5], (uint8_t)0x06);

}

#endif /* DCC_COMPILE_DECODER || DCC_COMPILE_ACCESSORY_DECODER */
