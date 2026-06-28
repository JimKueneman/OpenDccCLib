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
 * @file dcc_application_decoder_railcom.h
 * @brief Application-layer API for decoder RailCom responses.
 *
 * @details Provides high-level named functions that hide datagram IDs, 4/8
 * encoding, and Ch1/Ch2 splitting from the user. Wraps the internal
 * dcc_railcom_decoder module through an interface struct wired by dcc_config.c
 * during DccConfig_initialize(). Application code includes this header instead
 * of the internal RailCom encoder header.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#ifndef __DCC_APPLICATION_DECODER_RAILCOM__
#define __DCC_APPLICATION_DECODER_RAILCOM__

#include "dcc_types.h"

#if defined(DCC_COMPILE_RAILCOM) && (defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — wired by dcc_config.c during initialization. */
typedef struct {

        /** @brief Send a Channel 1 datagram (12-bit payload). */
    void (*send_ch1)(uint8_t datagram_id, uint8_t data);

        /** @brief Send a Channel 2 datagram (up to 6 encoded bytes). */
    void (*send_ch2)(const dcc_railcom_response_t *response);

        /** @brief Send a raw special code word (ACK/NACK), bypassing 4/8 table. */
    void (*send_code_word)(uint8_t code_word);

} interface_dcc_application_decoder_railcom_t;

    /**
     * @brief Initialize the decoder RailCom application module.
     * @param interface Pointer to populated
     *        @ref interface_dcc_application_decoder_railcom_t (wired by dcc_config.c).
     */
extern void DccApplicationDecoderRailcom_initialize(const interface_dcc_application_decoder_railcom_t *interface);

// =============================================================================
// Mobile Decoder Only — address feedback, track search, CV auto-transfer
// =============================================================================

#ifdef DCC_COMPILE_DECODER

    /**
     * @brief Send Ch1 address feedback, alternating ADR1 (low) and ADR2 (high).
     * @param address The decoder address (0-10239).
     *
     * Alternates between ADR1 (low 8 bits, datagram ID 1) and ADR2 (high 6 bits,
     * datagram ID 2) on successive calls.
     */
extern void DccApplicationDecoderRailcom_send_address_feedback(uint16_t address);

    /**
     * @brief Send a Ch2 track search response.
     * @param address The decoder address.
     * @param seconds_since_powerup Seconds elapsed since decoder power-up.
     *
     * Packs ADR1, ADR2, and time into a multi-byte Ch2 datagram.
     */
extern void DccApplicationDecoderRailcom_send_track_search_response(uint16_t address, uint8_t seconds_since_powerup);

    /**
     * @brief Send a Ch2 CV auto-transfer datagram (ID 12, 36-bit payload).
     * @param indexed_cv_address The indexed CV address (up to 24 bits).
     * @param value The CV value.
     */
extern void DccApplicationDecoderRailcom_send_cv_auto_transfer(uint32_t indexed_cv_address, uint8_t value);

#endif /* DCC_COMPILE_DECODER */

// =============================================================================
// Shared — POM, dynamic data, ACK/NACK, raw (mobile + accessory decoder)
// =============================================================================

    /**
     * @brief Send a Ch2 POM (programming on main) response.
     * @param cv_address The CV address being read (used as first data byte).
     * @param value The CV value to report.
     *
     * Sends a datagram ID 0 response combining cv_address and value.
     */
extern void DccApplicationDecoderRailcom_send_pom_response(uint16_t cv_address, uint8_t value);

    /**
     * @brief Send a Ch2 dynamic data datagram (ID 7).
     * @param subid Sub-index identifying the dynamic data type.
     * @param value The dynamic data value.
     */
extern void DccApplicationDecoderRailcom_send_dynamic_data(uint8_t subid, uint8_t value);

    /**
     * @brief Send the RailCom ACK special code word.
     *
     * Transmits the raw ACK code word (@ref DCC_RAILCOM_CODE_WORD_ACK) per the
     * 2026 draft S-9.3.2, bypassing the 4/8 encode table.
     */
extern void DccApplicationDecoderRailcom_send_ack(void);

    /**
     * @brief Send the RailCom NACK special code word.
     *
     * Transmits the raw NACK code word (@ref DCC_RAILCOM_CODE_WORD_NACK) per the
     * 2026 draft S-9.3.2, bypassing the 4/8 encode table.
     */
extern void DccApplicationDecoderRailcom_send_nack(void);

    /**
     * @brief Send a raw Ch2 datagram (escape hatch).
     * @param datagram_id Datagram ID (0-15).
     * @param data Pointer to data bytes to send.
     * @param count Number of data bytes (max @ref DCC_RAILCOM_DATAGRAM_MAX_BYTES).
     *
     * Builds a @ref dcc_railcom_response_t and sends via Ch2.
     */
extern void DccApplicationDecoderRailcom_send_raw(uint8_t datagram_id, const uint8_t *data, uint8_t count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && (DCC_COMPILE_DECODER || DCC_COMPILE_ACCESSORY_DECODER) */

#endif /* __DCC_APPLICATION_DECODER_RAILCOM__ */
