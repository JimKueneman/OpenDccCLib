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
 * @file dcc_application_accessory_decoder_railcom.h
 * @brief Application-layer API for accessory decoder RailCom SRQ and status.
 *
 * @details Provides SRQ (Service Request) state management and high-level
 * functions for accessory-decoder-specific RailCom responses (status, time,
 * error reports). Ch1 SRQ datagrams and Ch2 status responses are queued and
 * sent during the cutout ISR. This module owns its own interface struct for
 * Ch1/Ch2 hardware access, wired by dcc_config.c.
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#ifndef __DCC_APPLICATION_ACCESSORY_DECODER_RAILCOM__
#define __DCC_APPLICATION_ACCESSORY_DECODER_RAILCOM__

#include "dcc_types.h"
#include "dcc_application_decoder_railcom.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_ACCESSORY_DECODER)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// SRQ state enumeration
// =============================================================================

    /** @brief Service Request (SRQ) state for accessory decoder RailCom. */
typedef enum {

    DCC_ACC_SRQ_IDLE,        /**< No SRQ pending. */
    DCC_ACC_SRQ_PENDING,     /**< SRQ queued, waiting for stop command. */
    DCC_ACC_SRQ_RESPONDING   /**< Stop command received, sending response in cutout. */

} dcc_acc_srq_state_enum;

// =============================================================================
// Interface struct
// =============================================================================

    /** @brief Interface struct -- wired by dcc_config.c during initialization. */
typedef struct {

        /** @brief Send a Channel 1 datagram (12-bit payload). */
    void (*send_ch1)(uint8_t datagram_id, uint8_t data);

        /** @brief Send a Channel 2 datagram (up to 6 encoded bytes). */
    void (*send_ch2)(const dcc_railcom_response_t *response);

} interface_dcc_application_accessory_decoder_railcom_t;

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the accessory decoder RailCom module.
     * @details Resets the SRQ state to IDLE and stores the interface pointer
     *          for Ch1/Ch2 hardware access. Must be called during
     *          DccConfig_initialize().
     *
     * @param interface Pointer to populated
     *        @ref interface_dcc_application_accessory_decoder_railcom_t
     *        (wired by dcc_config.c).
     */
extern void DccApplicationAccessoryDecoderRailcom_initialize(const interface_dcc_application_accessory_decoder_railcom_t *interface);

    /**
     * @brief Enter SRQ_PENDING state and store the accessory address.
     * @details Stores the address and extended flag for the Ch1 SRQ datagram
     *          that will be sent during the next cutout after the stop command.
     *
     * @param address   The accessory decoder address (9-bit basic, 11-bit extended).
     * @param is_extended True for extended (11-bit) addressing, false for basic (9-bit).
     */
extern void DccApplicationAccessoryDecoderRailcom_send_srq(uint16_t address, bool is_extended);

    /**
     * @brief Queue an ID4 12-bit Ch2 status response (basic format).
     * @details Packs aspect_state into bits[4:0], command_match into bit 5,
     *          and is_setpoint into bit 6 of a single 12-bit datagram.
     *
     * @param aspect_state  Current aspect state (5 bits, 0-31).
     * @param command_match True if the last command matched this decoder.
     * @param is_setpoint   True if reporting a setpoint value.
     */
extern void DccApplicationAccessoryDecoderRailcom_send_status(uint8_t aspect_state, bool command_match, bool is_setpoint);

    /**
     * @brief Queue an ID4 2x12-bit Ch2 status response (extended format).
     * @details Lower 5 bits of aspect_state in the first datagram, upper 3 bits
     *          in the second datagram. command_match and is_setpoint in the first.
     *
     * @param aspect_state  Current aspect state (8 bits, 0-255).
     * @param command_match True if the last command matched this decoder.
     * @param is_setpoint   True if reporting a setpoint value.
     */
extern void DccApplicationAccessoryDecoderRailcom_send_status_extended(uint8_t aspect_state, bool command_match, bool is_setpoint);

    /**
     * @brief Queue an ID3 12-bit Ch2 response for all-pairs state.
     * @details Reports the combined state of all four output pairs in a single
     *          datagram.
     *
     * @param all_pairs_state Combined state byte for all output pairs.
     */
extern void DccApplicationAccessoryDecoderRailcom_send_status_4(uint8_t all_pairs_state);

    /**
     * @brief Queue an ID5 12-bit Ch2 time report.
     * @details Packs time_value into bits[6:0] and resolution_1s into bit 7.
     *
     * @param time_value    Time value (7 bits, 0-127).
     * @param resolution_1s True for 1-second resolution, false for 1-minute.
     */
extern void DccApplicationAccessoryDecoderRailcom_send_time_report(uint8_t time_value, bool resolution_1s);

    /**
     * @brief Queue an ID6 12-bit Ch2 error report.
     * @details Packs error_code into bits[5:0] and additional into bit 6.
     *
     * @param error_code Error code (6 bits, 0-63).
     * @param additional True if additional error data follows.
     */
extern void DccApplicationAccessoryDecoderRailcom_send_error_report(uint8_t error_code, bool additional);

    /**
     * @brief Return the current SRQ state.
     * @details Returns one of @ref dcc_acc_srq_state_enum values.
     *
     * @return Current SRQ state.
     */
extern dcc_acc_srq_state_enum DccApplicationAccessoryDecoderRailcom_get_srq_state(void);

    /**
     * @brief Called by the packet decoder when a stop command is received.
     * @details Transitions from SRQ_PENDING to SRQ_RESPONDING. No effect
     *          if the current state is not SRQ_PENDING.
     */
extern void DccApplicationAccessoryDecoderRailcom_on_stop_command(void);

    /**
     * @brief Called during the cutout ISR to send queued RailCom responses.
     * @details If SRQ_PENDING, sends the SRQ datagram on Ch1. If SRQ_RESPONDING
     *          and a Ch2 response is queued, sends the response and returns to
     *          IDLE.
     */
extern void DccApplicationAccessoryDecoderRailcom_on_cutout(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_ACCESSORY_DECODER */

#endif /* __DCC_APPLICATION_ACCESSORY_DECODER_RAILCOM__ */
