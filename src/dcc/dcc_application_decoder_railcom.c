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
 * @file dcc_application_decoder_railcom.c
 * @brief Channel 2 reply builders for the decoder's RailCom callback.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_decoder_railcom.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Fill a POM reply (ID 0).
     *
     * @details Sets the datagram id, data[0] = CV address low byte, data[1] = value,
     * count = 2.
     *
     * @verbatim
     * @param response Response to fill.
     * @param cv_address CV address; the low 8 bits are sent.
     * @param value CV value.
     * @endverbatim
     * @return true if filled, false if response is NULL.
     */
bool DccApplicationDecoderRailcom_pom_response(dcc_railcom_response_t *response, uint16_t cv_address, uint8_t value) {

    if (!response) {

        return false;

    }

    response->datagram_id = DCC_RAILCOM_ID_POM;
    response->data[0] = (uint8_t)(cv_address & 0xFF);
    response->data[1] = value;
    response->count = 2;

    return true;

}

    /**
     * @brief Fill a dynamic-data reply (ID 7).
     *
     * @details Sets the datagram id, data[0] = sub-index, data[1] = value, count = 2.
     *
     * @verbatim
     * @param response Response to fill.
     * @param subid Dynamic variable sub-index.
     * @param value Variable value.
     * @endverbatim
     * @return true if filled, false if response is NULL.
     */
bool DccApplicationDecoderRailcom_dynamic_data(dcc_railcom_response_t *response, uint8_t subid, uint8_t value) {

    if (!response) {

        return false;

    }

    response->datagram_id = DCC_RAILCOM_ID_DYN;
    response->data[0] = subid;
    response->data[1] = value;
    response->count = 2;

    return true;

}

    /**
     * @brief Fill a CV automatic-transfer reply (ID 12): 24-bit indexed CV address, low
     *  byte first, then the value.
     *
     * @details Sets the datagram id, data[0..2] = address bytes (low, mid, high),
     * data[3] = value, count = 4.
     *
     * @verbatim
     * @param response Response to fill.
     * @param indexed_cv_address Indexed CV address (page index and offset).
     * @param value CV value.
     * @endverbatim
     * @return true if filled, false if response is NULL.
     */
bool DccApplicationDecoderRailcom_cv_auto_transfer(dcc_railcom_response_t *response, uint32_t indexed_cv_address, uint8_t value) {

    if (!response) {

        return false;

    }

    response->datagram_id = DCC_RAILCOM_ID_CV_AUTO;
    response->data[0] = (uint8_t)(indexed_cv_address & 0xFF);
    response->data[1] = (uint8_t)((indexed_cv_address >> 8) & 0xFF);
    response->data[2] = (uint8_t)((indexed_cv_address >> 16) & 0xFF);
    response->data[3] = value;
    response->count = 4;

    return true;

}

    /**
     * @brief Fill an arbitrary reply from a datagram id and raw bytes.
     *
     * @details Validates the arguments first so a rejected call leaves the response
     * untouched, then stores the id masked to 4 bits, the count, and copies the bytes.
     *
     * @verbatim
     * @param response Response to fill.
     * @param datagram_id 4-bit datagram id.
     * @param data Data bytes to copy; may be NULL when count is 0.
     * @param count Number of data bytes.
     * @endverbatim
     * @return true if filled; false (response untouched) if response is NULL, data is
     *  NULL with a non-zero count, or count exceeds DCC_RAILCOM_DATAGRAM_MAX_BYTES.
     */
bool DccApplicationDecoderRailcom_raw(dcc_railcom_response_t *response, uint8_t datagram_id, const uint8_t *data, uint8_t count) {

    uint8_t byte_index;

    if (!response || (count > 0 && !data) || count > DCC_RAILCOM_DATAGRAM_MAX_BYTES) {

        return false;

    }

    response->datagram_id = (uint8_t)(datagram_id & 0x0F);
    response->count = count;

    for (byte_index = 0; byte_index < count; byte_index++) {

        response->data[byte_index] = data[byte_index];

    }

    return true;

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
