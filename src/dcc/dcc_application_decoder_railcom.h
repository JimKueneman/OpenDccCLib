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
 * @brief Channel 2 reply builders for the decoder's RailCom callback.
 *
 * @details Fills a dcc_railcom_response_t with the datagram id and byte layout
 * of the common Channel 2 replies, for use inside on_railcom_request(): the
 * application calls one builder on the callback's out response and returns
 * DCC_RAILCOM_REPLY_DATA; the dcc_railcom_decoder engine encodes and transmits
 * it in the cutout. Channel 1 (the address broadcast) and the ACK, NACK and
 * BUSY code words are produced by the engine from the reply status, so they
 * have no builders here. Stateless: nothing to initialize or wire.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_APPLICATION_DECODER_RAILCOM__
#define __DCC_APPLICATION_DECODER_RAILCOM__

#include "dcc_types.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

        /**
         * @brief Fill a POM reply (ID 0): the CV address low byte and the CV value.
         *
         * @details Answers a POM read or write. Sets the datagram id and a two-byte
         * payload; the engine 4/8-encodes it into Channel 2 when the callback returns
         * DCC_RAILCOM_REPLY_DATA.
         *
         * @param response Pointer to the @ref dcc_railcom_response_t to fill (the callback's out).
         * @param cv_address CV address (1-based); the low 8 bits are sent.
         * @param value CV value read or written.
         * @return true if filled, false if response is NULL.
         */
    extern bool DccApplicationDecoderRailcom_pom_response(dcc_railcom_response_t *response, uint16_t cv_address, uint8_t value);

        /**
         * @brief Fill a dynamic-data reply (ID 7): a DV sub-index and its value.
         *
         * @details Reports one dynamic variable (speed, load, fuel, ...). Sets the
         * datagram id and a two-byte payload: sub-index then value.
         *
         * @param response Pointer to the @ref dcc_railcom_response_t to fill.
         * @param subid Dynamic variable sub-index (0-63).
         * @param value Variable value.
         * @return true if filled, false if response is NULL.
         */
    extern bool DccApplicationDecoderRailcom_dynamic_data(dcc_railcom_response_t *response, uint8_t subid, uint8_t value);

        /**
         * @brief Fill a CV automatic-transfer reply (ID 12): 24-bit indexed CV address and value.
         *
         * @details Sets the datagram id and a four-byte payload: the three address bytes
         * low byte first, then the value.
         *
         * @param response Pointer to the @ref dcc_railcom_response_t to fill.
         * @param indexed_cv_address Indexed CV address (page index and offset), low byte first.
         * @param value CV value.
         * @return true if filled, false if response is NULL.
         */
    extern bool DccApplicationDecoderRailcom_cv_auto_transfer(dcc_railcom_response_t *response, uint32_t indexed_cv_address, uint8_t value);

        /**
         * @brief Fill an arbitrary reply: escape hatch for XPOM and other datagram ids.
         *
         * @details Copies the bytes as given; the id is masked to its low 4 bits. A zero
         * count produces an id-only datagram. On any argument failure the response is
         * left untouched.
         *
         * @param response Pointer to the @ref dcc_railcom_response_t to fill.
         * @param datagram_id 4-bit datagram id (DCC_RAILCOM_ID_*).
         * @param data Data bytes to copy; may be NULL when count is 0.
         * @param count Number of data bytes, at most DCC_RAILCOM_DATAGRAM_MAX_BYTES.
         * @return true if filled, false if response is NULL, data is NULL with a
         *  non-zero count, or count exceeds the maximum (response is left untouched).
         */
    extern bool DccApplicationDecoderRailcom_raw(dcc_railcom_response_t *response, uint8_t datagram_id, const uint8_t *data, uint8_t count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */

#endif /* __DCC_APPLICATION_DECODER_RAILCOM__ */
