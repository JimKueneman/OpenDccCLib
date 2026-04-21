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
 * Test suite for DCC Application Decoder CV
 */

#include "test/main_Test.hxx"

#include "dcc/dcc_application_decoder_cv.h"
#include "dcc/dcc_types.h"
#include "dcc/dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

// ============================================================================
// Mock / tracking state
// ============================================================================

static uint32_t cv_read_count = 0;
static uint32_t cv_write_count = 0;
static uint32_t is_locked_count = 0;

static uint16_t last_cv_number = 0;
static uint8_t last_value = 0;

static bool cv_read_return = false;
static bool cv_write_return = false;
static bool is_locked_return = false;

static uint8_t read_out_value = 0;

// ============================================================================
// Mock functions matching interface signatures
// ============================================================================

static bool mock_cv_read(uint16_t cv_number, uint8_t *value) {

    cv_read_count++;
    last_cv_number = cv_number;
    *value = read_out_value;
    return cv_read_return;

}

static bool mock_cv_write(uint16_t cv_number, uint8_t value) {

    cv_write_count++;
    last_cv_number = cv_number;
    last_value = value;
    return cv_write_return;

}

static bool mock_is_locked(void) {

    is_locked_count++;
    return is_locked_return;

}

// ============================================================================
// Helpers
// ============================================================================

static void reset_mocks(void) {

    cv_read_count = 0;
    cv_write_count = 0;
    is_locked_count = 0;

    last_cv_number = 0;
    last_value = 0;

    cv_read_return = false;
    cv_write_return = false;
    is_locked_return = false;

    read_out_value = 0;

}

static interface_dcc_application_decoder_cv_t make_interface(void) {

    interface_dcc_application_decoder_cv_t iface;
    memset(&iface, 0, sizeof(iface));
    iface.cv_read = mock_cv_read;
    iface.cv_write = mock_cv_write;
    iface.is_locked = mock_is_locked;
    return iface;

}

// ============================================================================
// Tests — initialize
// ============================================================================

TEST(DccApplicationDecoderCv, initialize_stores_interface) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    cv_read_return = true;
    read_out_value = 42;

    DccApplicationDecoderCv_initialize(&iface);

    uint8_t val = 0;
    bool result = DccApplicationDecoderCv_read(1, &val);

    EXPECT_TRUE(result);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(cv_read_count, 1u);

}

TEST(DccApplicationDecoderCv, initialize_with_null) {

    reset_mocks();

    DccApplicationDecoderCv_initialize(NULL);

    uint8_t val = 0;
    bool result = DccApplicationDecoderCv_read(1, &val);

    EXPECT_FALSE(result);
    EXPECT_EQ(cv_read_count, 0u);

}

// ============================================================================
// Tests — read
// ============================================================================

TEST(DccApplicationDecoderCv, read_null_guard) {

    reset_mocks();
    DccApplicationDecoderCv_initialize(NULL);

    uint8_t val = 0;
    bool result = DccApplicationDecoderCv_read(100, &val);

    EXPECT_FALSE(result);
    EXPECT_EQ(cv_read_count, 0u);

}

TEST(DccApplicationDecoderCv, read_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    cv_read_return = true;
    read_out_value = 0xAB;
    DccApplicationDecoderCv_initialize(&iface);

    uint8_t val = 0;
    bool result = DccApplicationDecoderCv_read(29, &val);

    EXPECT_TRUE(result);
    EXPECT_EQ(val, 0xAB);
    EXPECT_EQ(last_cv_number, 29);
    EXPECT_EQ(cv_read_count, 1u);

}

TEST(DccApplicationDecoderCv, read_returns_false_from_backend) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    cv_read_return = false;
    DccApplicationDecoderCv_initialize(&iface);

    uint8_t val = 0;
    bool result = DccApplicationDecoderCv_read(1, &val);

    EXPECT_FALSE(result);
    EXPECT_EQ(cv_read_count, 1u);

}

// ============================================================================
// Tests — write
// ============================================================================

TEST(DccApplicationDecoderCv, write_null_guard) {

    reset_mocks();
    DccApplicationDecoderCv_initialize(NULL);

    bool result = DccApplicationDecoderCv_write(1, 55);

    EXPECT_FALSE(result);
    EXPECT_EQ(cv_write_count, 0u);

}

TEST(DccApplicationDecoderCv, write_locked_returns_false) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    is_locked_return = true;
    DccApplicationDecoderCv_initialize(&iface);

    bool result = DccApplicationDecoderCv_write(29, 100);

    EXPECT_FALSE(result);
    EXPECT_EQ(is_locked_count, 1u);
    EXPECT_EQ(cv_write_count, 0u);

}

TEST(DccApplicationDecoderCv, write_unlocked_delegates) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    is_locked_return = false;
    cv_write_return = true;
    DccApplicationDecoderCv_initialize(&iface);

    bool result = DccApplicationDecoderCv_write(29, 100);

    EXPECT_TRUE(result);
    EXPECT_EQ(is_locked_count, 1u);
    EXPECT_EQ(cv_write_count, 1u);
    EXPECT_EQ(last_cv_number, 29);
    EXPECT_EQ(last_value, 100);

}

TEST(DccApplicationDecoderCv, write_unlocked_returns_false_from_backend) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    is_locked_return = false;
    cv_write_return = false;
    DccApplicationDecoderCv_initialize(&iface);

    bool result = DccApplicationDecoderCv_write(29, 100);

    EXPECT_FALSE(result);
    EXPECT_EQ(is_locked_count, 1u);
    EXPECT_EQ(cv_write_count, 1u);

}

// ============================================================================
// Tests — is_locked
// ============================================================================

TEST(DccApplicationDecoderCv, is_locked_null_guard) {

    reset_mocks();
    DccApplicationDecoderCv_initialize(NULL);

    bool result = DccApplicationDecoderCv_is_locked();

    EXPECT_FALSE(result);
    EXPECT_EQ(is_locked_count, 0u);

}

TEST(DccApplicationDecoderCv, is_locked_delegates_true) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    is_locked_return = true;
    DccApplicationDecoderCv_initialize(&iface);

    bool result = DccApplicationDecoderCv_is_locked();

    EXPECT_TRUE(result);
    EXPECT_EQ(is_locked_count, 1u);

}

TEST(DccApplicationDecoderCv, is_locked_delegates_false) {

    reset_mocks();
    interface_dcc_application_decoder_cv_t iface = make_interface();
    is_locked_return = false;
    DccApplicationDecoderCv_initialize(&iface);

    bool result = DccApplicationDecoderCv_is_locked();

    EXPECT_FALSE(result);
    EXPECT_EQ(is_locked_count, 1u);

}

#endif /* DCC_COMPILE_DECODER */
