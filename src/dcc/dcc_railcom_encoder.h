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
 * @file dcc_railcom_encoder.h
 * @brief RailCom 4/8 encoding and datagram transmission for decoders.
 *
 * @details Encodes 6-bit data values into 8-bit DC-balanced codewords per
 * NMRA RP-9.3.2. Provides functions to send Channel 1 and Channel 2
 * datagrams via the interface's uart_write callback. Disabled at runtime if
 * uart_write is NULL.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_RAILCOM_ENCODER__
#define __DCC_RAILCOM_ENCODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Transmit one 4/8-encoded byte. NULL = no RailCom responses. */
    void (*uart_write)(uint8_t byte);

} interface_dcc_railcom_encoder_t;

    /**
     * @brief Initialize the RailCom encoder module.
     * @param interface Pointer to populated interface struct.
     */
extern void DccRailcomEncoder_initialize(const interface_dcc_railcom_encoder_t *interface);

    /**
     * @brief Encode a single 6-bit value to an 8-bit RailCom codeword.
     * @param value 6-bit data value (0x00-0x3F).
     * @return 8-bit DC-balanced codeword, or 0x00 if value is out of range.
     */
extern uint8_t DccRailcomEncoder_encode_byte(uint8_t value);

    /**
     * @brief Send a raw RailCom special code word (bypasses the 4/8 table).
     * @param code_word Raw 8-bit code word to transmit (e.g.
     *        @ref DCC_RAILCOM_CODE_WORD_ACK or @ref DCC_RAILCOM_CODE_WORD_NACK).
     *
     * @details Special code words (ACK/NACK) per the 2026 draft S-9.3.2 are
     * transmitted verbatim over the same UART write path as encoded bytes,
     * but are NOT run through the 4/8 encode table.
     */
extern void DccRailcomEncoder_send_code_word(uint8_t code_word);

    /**
     * @brief Send a Channel 1 datagram (2 encoded bytes, 12-bit payload).
     * @param datagram_id Datagram ID (4 bits, 0-15).
     * @param data Data byte (8 bits).
     *
     * @details Combines the 4-bit ID and 8-bit data into a 12-bit value,
     * splits into two 6-bit halves, encodes each, and sends via UART.
     */
extern void DccRailcomEncoder_send_ch1(uint8_t datagram_id, uint8_t data);

    /**
     * @brief Send a Channel 2 datagram (up to 6 encoded bytes).
     * @param response Pointer to the response datagram to encode and send.
     *
     * @details First two bytes encode the 12-bit combined ID+data[0]. Any
     * additional data bytes are encoded individually as 6-bit values.
     */
extern void DccRailcomEncoder_send_ch2(const dcc_railcom_response_t *response);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_RAILCOM_ENCODER__ */
