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
 * @brief Application-layer implementation for decoder RailCom responses.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

/*
 * NOTE: RailCom datagram IDs and special code words in this module follow the
 * 2026 draft S-9.3.2.  Address framing (Table 19): ADR1/ID1 carries the HIGH
 * bits, ADR2/ID2 carries the LOW bits.  ACK/NACK are raw 4/8 special code words
 * (transmitted verbatim, NOT run through the 4/8 encode table).
 */

#include "dcc_application_decoder_railcom.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER)

// =============================================================================
// RailCom datagram ID aliases (2026 draft S-9.3.2, mapped to dcc_defines.h)
// =============================================================================

    /** @brief ADR1 (HIGH bits of address) -- datagram ID 1 (Table 19) */
#define RAILCOM_ID_ADR1_HIGH    DCC_RAILCOM_ID_ADR1_HIGH

    /** @brief ADR2 (LOW bits of address) -- datagram ID 2 (Table 19) */
#define RAILCOM_ID_ADR2_LOW     DCC_RAILCOM_ID_ADR2_LOW

    /** @brief POM response -- Ch2 datagram ID 0 */
#define RAILCOM_ID_POM          DCC_RAILCOM_ID_POM

    /** @brief Dynamic data -- Ch2 datagram ID 7 */
#define RAILCOM_ID_DYN          DCC_RAILCOM_ID_DYN

    /** @brief CV auto-transfer -- Ch2 datagram ID 12 */
#define RAILCOM_ID_CV_AUTO      DCC_RAILCOM_ID_CV_AUTO

    /** @brief Time -- Ch2 datagram ID 14 */
#define RAILCOM_ID_TIME         DCC_RAILCOM_ID_TIME

// =============================================================================
// Static state
// =============================================================================

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_decoder_railcom_t *_interface = (void *)0;

    /** @brief Alternates between ADR1 and ADR2 on successive address calls */
static bool _adr_alternate = false;

// =============================================================================
// Public API
// =============================================================================

/**
 * @verbatim
 * @param interface  Pointer to populated interface struct (wired by dcc_config.c).
 * @endverbatim
 */
void DccApplicationDecoderRailcom_initialize(const interface_dcc_application_decoder_railcom_t *interface) {

    _interface = interface;
    _adr_alternate = false;

}

// =============================================================================
// Mobile Decoder Only
// =============================================================================

#ifdef DCC_COMPILE_DECODER

/**
 * @verbatim
 * @param address  The decoder address (0-10239).
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_address_feedback(uint16_t address) {

    if (!_interface) {

        return;

    }

    if (!_adr_alternate) {

        /* ADR1 (ID 1): HIGH bits of address (Table 19) */
        _interface->send_ch1(RAILCOM_ID_ADR1_HIGH, (uint8_t)((address >> 8) & 0x3F));

    } else {

        /* ADR2 (ID 2): LOW bits of address (Table 19) */
        _interface->send_ch1(RAILCOM_ID_ADR2_LOW, (uint8_t)(address & 0xFF));

    }

    _adr_alternate = !_adr_alternate;

}

/**
 * @verbatim
 * @param address               The decoder address.
 * @param seconds_since_powerup Seconds elapsed since decoder power-up.
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_track_search_response(uint16_t address, uint8_t seconds_since_powerup) {

    dcc_railcom_response_t response;

    if (!_interface) {

        return;

    }

    /* Track-search response = three datagrams sent together (2026 draft
     * S-9.3.2): ID1 (ADR1, HIGH bits), ID2 (ADR2, LOW bits), ID14 (Time). */

    /* Datagram 1: ID1 (ADR1) — HIGH bits of address */
    response.datagram_id = RAILCOM_ID_ADR1_HIGH;
    response.data[0] = (uint8_t)((address >> 8) & 0x3F);
    response.count = 1;
    _interface->send_ch2(&response);

    /* Datagram 2: ID2 (ADR2) — LOW bits of address */
    response.datagram_id = RAILCOM_ID_ADR2_LOW;
    response.data[0] = (uint8_t)(address & 0xFF);
    response.count = 1;
    _interface->send_ch2(&response);

    /* Datagram 3: ID14 (Time) */
    response.datagram_id = RAILCOM_ID_TIME;
    response.data[0] = seconds_since_powerup;
    response.count = 1;
    _interface->send_ch2(&response);

}

/**
 * @verbatim
 * @param indexed_cv_address  The indexed CV address (up to 24 bits).
 * @param value               The CV value.
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_cv_auto_transfer(uint32_t indexed_cv_address, uint8_t value) {

    dcc_railcom_response_t response;

    if (!_interface) {

        return;

    }

    /* ID 12: 36-bit payload = 24-bit indexed CV address + 8-bit value */
    response.datagram_id = RAILCOM_ID_CV_AUTO;
    response.data[0] = (uint8_t)(indexed_cv_address & 0xFF);
    response.data[1] = (uint8_t)((indexed_cv_address >> 8) & 0xFF);
    response.data[2] = (uint8_t)((indexed_cv_address >> 16) & 0xFF);
    response.data[3] = value;
    response.count = 4;

    _interface->send_ch2(&response);

}

#endif /* DCC_COMPILE_DECODER */

// =============================================================================
// Shared — POM, dynamic data, ACK/NACK, raw (mobile + accessory decoder)
// =============================================================================

/**
 * @verbatim
 * @param cv_address  The CV address being read.
 * @param value       The CV value to report.
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_pom_response(uint16_t cv_address, uint8_t value) {

    dcc_railcom_response_t response;

    if (!_interface) {

        return;

    }

    response.datagram_id = RAILCOM_ID_POM;
    response.data[0] = (uint8_t)(cv_address & 0xFF);
    response.data[1] = value;
    response.count = 2;

    _interface->send_ch2(&response);

}

/**
 * @verbatim
 * @param subid  Sub-index identifying the dynamic data type.
 * @param value  The dynamic data value.
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_dynamic_data(uint8_t subid, uint8_t value) {

    dcc_railcom_response_t response;

    if (!_interface) {

        return;

    }

    response.datagram_id = RAILCOM_ID_DYN;
    response.data[0] = subid;
    response.data[1] = value;
    response.count = 2;

    _interface->send_ch2(&response);

}

void DccApplicationDecoderRailcom_send_ack(void) {

    if (!_interface || !_interface->send_code_word) {

        return;

    }

    /* ACK is a raw special code word (2026 draft S-9.3.2), not a datagram. */
    _interface->send_code_word(DCC_RAILCOM_CODE_WORD_ACK);

}

void DccApplicationDecoderRailcom_send_nack(void) {

    if (!_interface || !_interface->send_code_word) {

        return;

    }

    /* NACK is a raw special code word (2026 draft S-9.3.2), not a datagram. */
    _interface->send_code_word(DCC_RAILCOM_CODE_WORD_NACK);

}

/**
 * @verbatim
 * @param datagram_id  Datagram ID (0-15).
 * @param data         Pointer to data bytes to send.
 * @param count        Number of data bytes (max DCC_RAILCOM_DATAGRAM_MAX_BYTES).
 * @endverbatim
 */
void DccApplicationDecoderRailcom_send_raw(uint8_t datagram_id, const uint8_t *data, uint8_t count) {

    dcc_railcom_response_t response;
    uint8_t i;

    if (!_interface) {

        return;

    }

    if (count > DCC_RAILCOM_DATAGRAM_MAX_BYTES) {

        count = DCC_RAILCOM_DATAGRAM_MAX_BYTES;

    }

    response.datagram_id = datagram_id;
    response.count = count;

    for (i = 0; i < count; i++) {

        response.data[i] = data[i];

    }

    _interface->send_ch2(&response);

}

#endif /* DCC_COMPILE_DECODER || DCC_COMPILE_ACCESSORY_DECODER */
