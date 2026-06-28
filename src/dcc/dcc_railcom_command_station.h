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
 * @brief RailCom 4/8 decoding, cutout management, and receive buffer.
 *
 * @details Decodes RailCom bytes received during cutout windows. Manages
 * a circular buffer of decoded datagrams tagged with the DCC address of the
 * packet that preceded the cutout. Disabled at runtime if railcom_uart_read
 * is NULL in the config.
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#ifndef __DCC_RAILCOM_DECODER__
#define __DCC_RAILCOM_DECODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_COMMAND_STATION)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Read one byte from RailCom UART. Returns true if byte available. */
    bool (*uart_read)(uint8_t *byte);

        /**
         * @brief User callback: RailCom datagram decoded. NULL = no notification.
         *        Fired from DccRailcomDecoder_run(), NOT ISR context.
         */
    void (*on_datagram)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram);

} interface_dcc_railcom_decoder_t;

    /** @brief Instance context for the RailCom decoder module.
     *
     *  @details Holds all per-instance state that was formerly file-scope static.
     *  Allocate one of these per DCC output channel that uses RailCom.
     */
typedef struct {

    const interface_dcc_railcom_decoder_t *interface;
    dcc_railcom_datagram_t buffer[USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH];
    uint8_t buffer_head;
    uint8_t buffer_tail;
    uint8_t buffer_count;
    dcc_address_t cutout_address;
    volatile bool cutout_pending;

} dcc_railcom_decoder_context_t;

    /**
     * @brief Initialize the RailCom decoder module.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     * @param interface Pointer to populated @ref interface_dcc_railcom_decoder_t struct.
     */
    extern void DccRailcomDecoder_initialize(dcc_railcom_decoder_context_t *context, const interface_dcc_railcom_decoder_t *interface);

    /**
     * @brief Main loop processing for the RailCom decoder.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     */
extern void DccRailcomDecoder_run(dcc_railcom_decoder_context_t *context);

    /**
     * @brief Begin a RailCom cutout window for a given address.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     * @param address The DCC address associated with this cutout.
     */
    extern void DccRailcomDecoder_begin_cutout(dcc_railcom_decoder_context_t *context, dcc_address_t address);

    /**
     * @brief End the current RailCom cutout window.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     */
extern void DccRailcomDecoder_end_cutout(dcc_railcom_decoder_context_t *context);

    /**
     * @brief Read the next decoded RailCom datagram from the buffer.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     * @param datagram Pointer to @ref dcc_railcom_datagram_t to fill with decoded data.
     * @return true if a datagram was available, false if buffer empty.
     */
    extern bool DccRailcomDecoder_read(dcc_railcom_decoder_context_t *context, dcc_railcom_datagram_t *datagram);

    /**
     * @brief Return the number of decoded datagrams available in the buffer.
     * @param context Pointer to @ref dcc_railcom_decoder_context_t instance.
     * @return Number of datagrams waiting to be read.
     */
extern uint8_t DccRailcomDecoder_available(const dcc_railcom_decoder_context_t *context);

    /**
     * @brief Decode a single RailCom 4/8 encoded byte.
     * @param encoded The 8-bit codeword received from UART.
     * @return 6-bit decoded value, or DCC_RAILCOM_DECODE_INVALID/ACK/NACK.
     */
extern uint8_t DccRailcomDecoder_decode_byte(uint8_t encoded);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_RAILCOM_DECODER__ */
