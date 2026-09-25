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
 * Test suite for the decoder RailCom Channel 2 reply builders
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_decoder_railcom.h"
#include "dcc/dcc_railcom_utilities.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

static dcc_railcom_response_t make_response(void) {
    dcc_railcom_response_t r;
    memset(&r, 0xAA, sizeof(r));     /* poison so untouched fields are visible */
    return r;
}

TEST(DccApplicationDecoderRailcom, pom_response_layout) {
    dcc_railcom_response_t r = make_response();
    EXPECT_TRUE(DccApplicationDecoderRailcom_pom_response(&r, 0x0123, 0x5A));
    EXPECT_EQ(r.datagram_id, (uint8_t)DCC_RAILCOM_ID_POM);
    EXPECT_EQ(r.data[0], (uint8_t)0x23);     /* CV address low byte */
    EXPECT_EQ(r.data[1], (uint8_t)0x5A);
    EXPECT_EQ(r.count, (uint8_t)2);
}

TEST(DccApplicationDecoderRailcom, dynamic_data_layout) {
    dcc_railcom_response_t r = make_response();
    EXPECT_TRUE(DccApplicationDecoderRailcom_dynamic_data(&r, 26, 0x40));
    EXPECT_EQ(r.datagram_id, (uint8_t)DCC_RAILCOM_ID_DYN);
    EXPECT_EQ(r.data[0], (uint8_t)26);
    EXPECT_EQ(r.data[1], (uint8_t)0x40);
    EXPECT_EQ(r.count, (uint8_t)2);
}

TEST(DccApplicationDecoderRailcom, cv_auto_transfer_layout) {
    dcc_railcom_response_t r = make_response();
    EXPECT_TRUE(DccApplicationDecoderRailcom_cv_auto_transfer(&r, 0x00ABCDEF, 0x77));
    EXPECT_EQ(r.datagram_id, (uint8_t)DCC_RAILCOM_ID_CV_AUTO);
    EXPECT_EQ(r.data[0], (uint8_t)0xEF);     /* low byte first */
    EXPECT_EQ(r.data[1], (uint8_t)0xCD);
    EXPECT_EQ(r.data[2], (uint8_t)0xAB);
    EXPECT_EQ(r.data[3], (uint8_t)0x77);
    EXPECT_EQ(r.count, (uint8_t)4);
}

TEST(DccApplicationDecoderRailcom, raw_copies_bytes_and_masks_id) {
    dcc_railcom_response_t r = make_response();
    const uint8_t bytes[] = {0x11, 0x22, 0x33};
    EXPECT_TRUE(DccApplicationDecoderRailcom_raw(&r, 0xF8, bytes, 3));   /* id 8 = XPOM */
    EXPECT_EQ(r.datagram_id, (uint8_t)0x08);
    EXPECT_EQ(r.data[0], (uint8_t)0x11);
    EXPECT_EQ(r.data[1], (uint8_t)0x22);
    EXPECT_EQ(r.data[2], (uint8_t)0x33);
    EXPECT_EQ(r.count, (uint8_t)3);
}

TEST(DccApplicationDecoderRailcom, raw_with_zero_count_and_null_data_is_fine) {
    dcc_railcom_response_t r = make_response();
    EXPECT_TRUE(DccApplicationDecoderRailcom_raw(&r, 3, NULL, 0));
    EXPECT_EQ(r.datagram_id, (uint8_t)3);
    EXPECT_EQ(r.count, (uint8_t)0);
}

TEST(DccApplicationDecoderRailcom, raw_refuses_oversize_and_null_data) {
    dcc_railcom_response_t r = make_response();
    const uint8_t bytes[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1] = {0};
    EXPECT_FALSE(DccApplicationDecoderRailcom_raw(&r, 3, bytes, DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1));
    EXPECT_EQ(r.count, (uint8_t)0xAA);        /* untouched */
    EXPECT_FALSE(DccApplicationDecoderRailcom_raw(&r, 3, NULL, 2));
    EXPECT_EQ(r.count, (uint8_t)0xAA);
}

TEST(DccApplicationDecoderRailcom, null_response_is_refused_by_every_builder) {
    EXPECT_FALSE(DccApplicationDecoderRailcom_pom_response(NULL, 1, 1));
    EXPECT_FALSE(DccApplicationDecoderRailcom_dynamic_data(NULL, 1, 1));
    EXPECT_FALSE(DccApplicationDecoderRailcom_cv_auto_transfer(NULL, 1, 1));
    const uint8_t b = 0;
    EXPECT_FALSE(DccApplicationDecoderRailcom_raw(NULL, 1, &b, 1));
}

TEST(DccApplicationDecoderRailcom, builder_output_encodes_for_the_engine) {
    /* The engine turns a DATA reply into count + 1 code words (id + data[0] share two) */
    dcc_railcom_response_t r = make_response();
    uint8_t encoded[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1];
    ASSERT_TRUE(DccApplicationDecoderRailcom_pom_response(&r, 5, 0x3C));
    EXPECT_EQ(DccRailcomUtilities_encode_ch2(&r, encoded), (uint8_t)3);
    ASSERT_TRUE(DccApplicationDecoderRailcom_cv_auto_transfer(&r, 0x010203, 4));
    EXPECT_EQ(DccRailcomUtilities_encode_ch2(&r, encoded), (uint8_t)5);
}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
