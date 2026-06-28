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
 * @file dcc_railcom_decoder.h
 * @brief RailCom 4/8 encoding and datagram transmission for decoders.
 *
 * @details Encodes 6-bit data values into 8-bit DC-balanced codewords per
 * NMRA RP-9.3.2. Provides functions to send Channel 1 and Channel 2
 * datagrams via the interface's uart_write callback. Disabled at runtime if
 * uart_write is NULL.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#ifndef __DCC_RAILCOM_DECODER__
#define __DCC_RAILCOM_DECODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Transmit one 4/8-encoded byte. NULL = no RailCom responses. */
    void (*uart_write)(uint8_t byte);

        /** @brief App hook: a command addressed to this decoder was recognized before
         *  the XOR. The app fills @p out for Channel 2 and returns the reply status
         *  (DATA / ACK / BUSY / NACK / NONE). NULL = no Channel 2 (ADR only). Must be
         *  fast and non-blocking -- it runs while the XOR byte is being received. */
    dcc_railcom_reply_status_enum (*on_railcom_request)(const uint8_t *instruction,
            uint8_t instruction_count, dcc_railcom_response_t *out);

} interface_dcc_railcom_decoder_t;

    /**
     * @brief Initialize the RailCom encoder module.
     * @param interface Pointer to populated interface struct.
     */
extern void DccRailcomDecoder_initialize(const interface_dcc_railcom_decoder_t *interface);

    /**
     * @brief Set this decoder's active address, pushed by the packet decoder whenever
     *  it resolves or changes (at init and on address-CV writes).
     *
     * @details The RailCom Tx engine holds the address for its ADR (Channel 1)
     *  datagram, so it never re-reads CVs during the cutout.
     *
     * @param address The decoder's resolved @ref dcc_address_t.
     * @param type The resolved @ref dcc_address_type_enum (short / long).
     */
extern void DccRailcomDecoder_set_address(dcc_address_t address, dcc_address_type_enum type);

    /**
     * @brief Send a raw RailCom special code word (bypasses the 4/8 table).
     * @param code_word Raw 8-bit code word to transmit (e.g.
     *        @ref DCC_RAILCOM_CODE_WORD_ACK or @ref DCC_RAILCOM_CODE_WORD_NACK).
     *
     * @details Special code words (ACK/NACK) per the 2026 draft S-9.3.2 are
     * transmitted verbatim over the same UART write path as encoded bytes,
     * but are NOT run through the 4/8 encode table.
     */
extern void DccRailcomDecoder_send_code_word(uint8_t code_word);

    /**
     * @brief Send a Channel 1 datagram (2 encoded bytes, 12-bit payload).
     * @param datagram_id Datagram ID (4 bits, 0-15).
     * @param data Data byte (8 bits).
     *
     * @details Combines the 4-bit ID and 8-bit data into a 12-bit value,
     * splits into two 6-bit halves, encodes each, and sends via UART.
     */
extern void DccRailcomDecoder_send_ch1(uint8_t datagram_id, uint8_t data);

    /**
     * @brief Send a Channel 2 datagram (up to 6 encoded bytes).
     * @param response Pointer to the response datagram to encode and send.
     *
     * @details First two bytes encode the 12-bit combined ID+data[0]. Any
     * additional data bytes are encoded individually as 6-bit values.
     */
extern void DccRailcomDecoder_send_ch2(const dcc_railcom_response_t *response);

// =============================================================================
// Decoder-side recognizer (pure)
// =============================================================================

    /**
     * @brief Total DCC packet length (including the XOR byte) implied by the bytes
     *  received so far, from the self-describing multifunction-decoder instruction
     *  format (S-9.2.1).
     *
     * @details Pure -- no side effects, no module state. The decoder-side RailCom
     *  recognizer uses it to know a command is complete (the next byte is the XOR)
     *  before the cutout. Returns 0 when the length cannot yet be determined: too few
     *  bytes to classify, an extended form not yet sized (XPOM), an accessory/reserved
     *  leading byte, or an instruction this module does not size.
     *
     * @param data Packet bytes received so far (address + instruction).
     * @param count Number of bytes in @p data.
     *
     * @return Total packet length in bytes including XOR, or 0 if undeterminable.
     */
extern uint8_t DccRailcomDecoder_packet_length(const uint8_t *data, uint8_t count);

    /**
     * @brief Extract the multifunction address and its type from a packet's leading
     *  bytes.
     *
     * @details Pure -- no side effects, no module state. Decodes the broadcast, short
     *  (CV1), long (CV17/18), and idle leading bytes. The recognizer compares the
     *  result against this decoder's own address to decide "addressed to me."
     *
     * @param data Packet bytes received so far.
     * @param count Number of bytes in @p data.
     * @param address Out: decoded @ref dcc_address_t (valid only when true is returned).
     * @param type Out: decoded @ref dcc_address_type_enum (valid only when true returned).
     *
     * @return true if an address was decoded; false if too few bytes or an
     *         accessory/reserved leading byte (not a multifunction packet).
     */
extern bool DccRailcomDecoder_packet_address(const uint8_t *data, uint8_t count,
            dcc_address_t *address, dcc_address_type_enum *type);

    /**
     * @brief Feed one assembled packet byte to the RailCom Tx dispatch.
     *
     * @details Wired (in dcc_config) to the bit decoder's on_byte_received. As bytes
     *  arrive it recognizes a complete command addressed to this decoder (before the
     *  XOR), arms the ADR reply, and on the XOR byte -- if the packet validates --
     *  clocks the reply out via uart_write for the app to place in the cutout.
     *
     * @param data Packet bytes assembled so far.
     * @param count Number of bytes in @p data.
     */
extern void DccRailcomDecoder_on_byte_received(const uint8_t *data, uint8_t count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */

#endif /* __DCC_RAILCOM_DECODER__ */
