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
 * @file dcc_application_accessory_decoder_railcom.c
 * @brief Application-layer implementation for accessory decoder RailCom SRQ and status.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#include "dcc_application_accessory_decoder_railcom.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_ACCESSORY_DECODER)

// =============================================================================
// RailCom datagram ID aliases (mapped to existing dcc_defines.h constants)
// =============================================================================

    /* SRQ is a 12-bit Channel-1 datagram with NO identifier (S-9.3.2
     * Section 7.1 / Table 36); see DccApplicationAccessoryDecoderRailcom_on_cutout. */

    /** @brief All-pairs status -- Ch2 datagram ID 3 (S-9.3.2) */
#define RAILCOM_ID_STATUS_4     3

    /** @brief Single-pair status -- Ch2 datagram ID 4 (S-9.3.2) */
#define RAILCOM_ID_STATUS       4

    /** @brief Time report -- Ch2 datagram ID 5 (S-9.3.2) */
#define RAILCOM_ID_TIME         5

    /** @brief Error report -- Ch2 datagram ID 6 (S-9.3.2) */
#define RAILCOM_ID_ERROR        6

// =============================================================================
// Static state
// =============================================================================

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_accessory_decoder_railcom_t *_interface = (void *)0;

    /** @brief Current SRQ state */
static dcc_acc_srq_state_enum _srq_state = DCC_ACC_SRQ_IDLE;

    /** @brief Stored accessory address for pending SRQ */
static uint16_t _srq_address = 0;

    /** @brief True if the SRQ address uses extended (11-bit) format */
static bool _srq_is_extended = false;

    /** @brief Queued Ch2 response to send during cutout */
static dcc_railcom_response_t _pending_response;

    /** @brief True if a Ch2 response is queued */
static bool _has_pending_response = false;

// =============================================================================
// Public API
// =============================================================================

/**
 * @verbatim
 * @param interface  Pointer to populated interface struct (wired by dcc_config.c).
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_initialize(const interface_dcc_application_accessory_decoder_railcom_t *interface) {

    _interface = interface;
    _srq_state = DCC_ACC_SRQ_IDLE;
    _srq_address = 0;
    _srq_is_extended = false;
    _has_pending_response = false;

}

/**
 * @verbatim
 * @param address      The accessory decoder address (9-bit basic, 11-bit extended).
 * @param is_extended  True for extended (11-bit) addressing, false for basic (9-bit).
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_srq(uint16_t address, bool is_extended) {

    _srq_address = address;
    _srq_is_extended = is_extended;
    _srq_state = DCC_ACC_SRQ_PENDING;

}

/**
 * @verbatim
 * @param aspect_state   Current aspect state (5 bits, 0-31).
 * @param command_match  True if the last command matched this decoder.
 * @param is_setpoint    True if reporting a setpoint value.
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_status(uint8_t aspect_state, bool command_match, bool is_setpoint) {

    uint8_t packed;

    packed = (uint8_t)(aspect_state & 0x1F);

    if (command_match) {

        packed |= (uint8_t)(1 << 5);

    }

    if (is_setpoint) {

        packed |= (uint8_t)(1 << 6);

    }

    _pending_response.datagram_id = RAILCOM_ID_STATUS;
    _pending_response.data[0] = packed;
    _pending_response.count = 1;
    _has_pending_response = true;

}

/**
 * @verbatim
 * @param aspect_state   Current aspect state (8 bits, 0-255).
 * @param command_match  True if the last command matched this decoder.
 * @param is_setpoint    True if reporting a setpoint value.
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_status_extended(uint8_t aspect_state, bool command_match, bool is_setpoint) {

    uint8_t packed_low;
    uint8_t packed_high;

    /* First datagram: lower 5 bits + flags */
    packed_low = (uint8_t)(aspect_state & 0x1F);

    if (command_match) {

        packed_low |= (uint8_t)(1 << 5);

    }

    if (is_setpoint) {

        packed_low |= (uint8_t)(1 << 6);

    }

    /* Second datagram: upper 3 bits */
    packed_high = (uint8_t)((aspect_state >> 5) & 0x07);

    _pending_response.datagram_id = RAILCOM_ID_STATUS;
    _pending_response.data[0] = packed_low;
    _pending_response.data[1] = packed_high;
    _pending_response.count = 2;
    _has_pending_response = true;

}

/**
 * @verbatim
 * @param all_pairs_state  Combined state byte for all output pairs.
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_status_4(uint8_t all_pairs_state) {

    _pending_response.datagram_id = RAILCOM_ID_STATUS_4;
    _pending_response.data[0] = all_pairs_state;
    _pending_response.count = 1;
    _has_pending_response = true;

}

/**
 * @verbatim
 * @param time_value     Time value (7 bits, 0-127).
 * @param resolution_1s  True for 1-second resolution, false for 1-minute.
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_time_report(uint8_t time_value, bool resolution_1s) {

    uint8_t packed;

    packed = (uint8_t)(time_value & 0x7F);

    if (resolution_1s) {

        packed |= (uint8_t)(1 << 7);

    }

    _pending_response.datagram_id = RAILCOM_ID_TIME;
    _pending_response.data[0] = packed;
    _pending_response.count = 1;
    _has_pending_response = true;

}

/**
 * @verbatim
 * @param error_code  Error code (6 bits, 0-63).
 * @param additional  True if additional error data follows.
 * @endverbatim
 */
void DccApplicationAccessoryDecoderRailcom_send_error_report(uint8_t error_code, bool additional) {

    uint8_t packed;

    packed = (uint8_t)(error_code & 0x3F);

    if (additional) {

        packed |= (uint8_t)(1 << 6);

    }

    _pending_response.datagram_id = RAILCOM_ID_ERROR;
    _pending_response.data[0] = packed;
    _pending_response.count = 1;
    _has_pending_response = true;

}

dcc_acc_srq_state_enum DccApplicationAccessoryDecoderRailcom_get_srq_state(void) {

    return _srq_state;

}

void DccApplicationAccessoryDecoderRailcom_on_stop_command(void) {

    if (_srq_state == DCC_ACC_SRQ_PENDING) {

        _srq_state = DCC_ACC_SRQ_RESPONDING;

    }

}

void DccApplicationAccessoryDecoderRailcom_on_cutout(void) {

    if (!_interface) {

        return;

    }

    if (_srq_state == DCC_ACC_SRQ_PENDING) {

        /* Build and send the Ch1 SRQ datagram from the stored address.
         *
         * Per S-9.3.2 Section 7.1 / Table 36 (page 45-46), the SRQ is a
         * 12-bit Channel-1 datagram with NO identifier:
         *
         *     basic    : 0AAA AAAA-AAAA  (MSB = 0, 11-bit address)
         *     extended : 1AAA AAAA-AAAA  (MSB = 1, 11-bit address)
         *
         * DccRailcomDecoder_send_ch1() packs its two arguments into one
         * 12-bit value as ((datagram_id & 0x0F) << 8) | data, then splits
         * that into two 6-bit RailCom code words.  There is no SRQ
         * identifier, so the full 12 bits must carry the format bit and the
         * 11-bit address:
         *
         *     bit 11      = format flag (0 basic, 1 extended)
         *     bits 10..0  = accessory decoder address
         *
         * Mapped onto send_ch1(datagram_id, data):
         *     datagram_id (bits 11..8) = (format << 3) | ((address >> 8) & 0x07)
         *     data        (bits  7..0) = address & 0xFF
         */
        uint16_t datagram;
        uint8_t high_field;
        uint8_t low_field;

        datagram = (uint16_t)(_srq_address & 0x07FF);

        if (_srq_is_extended) {

            datagram |= (uint16_t)(1u << 11);

        }

        high_field = (uint8_t)((datagram >> 8) & 0x0F);
        low_field = (uint8_t)(datagram & 0xFF);

        _interface->send_ch1(high_field, low_field);

    } else if (_srq_state == DCC_ACC_SRQ_RESPONDING) {

        if (_has_pending_response) {

            _interface->send_ch2(&_pending_response);
            _has_pending_response = false;

        }

        _srq_state = DCC_ACC_SRQ_IDLE;

    }

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_ACCESSORY_DECODER */
