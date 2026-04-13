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
 * @file dcc_bit_decoder.h
 * @brief Edge timestamp to bit classification, preamble detection, and byte
 * assembly for DCC decoder.
 *
 * @details Receives edge timestamps from the input-capture ISR, classifies
 * half-bit periods as one or zero using timing thresholds from S-9.1, pairs
 * half-bits into complete bits, detects preamble sequences, and assembles
 * raw packet bytes. Complete packets are forwarded via callback.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_BIT_DECODER__
#define __DCC_BIT_DECODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /**
         * @brief Complete packet received. Called when a valid packet frame
         *  (preamble + bytes + end bit) has been assembled.
         * @param data Raw packet bytes (including XOR byte).
         * @param byte_count Number of bytes in the packet.
         */
    void (*on_packet_received)(const uint8_t *data, uint8_t byte_count);

} interface_dcc_bit_decoder_t;

    /**
     * @brief Initialize the bit decoder module.
     * @param interface Pointer to populated interface struct.
     */
extern void DccBitDecoder_initialize(const interface_dcc_bit_decoder_t *interface);

    /**
     * @brief Process a signal edge from the input-capture ISR.
     * @param timestamp_usec Microsecond timestamp of the edge.
     *
     * @details Call this from the input-capture ISR on every edge (rising or
     * falling). The library classifies one/zero bits from the timing internally.
     */
extern void DccBitDecoder_edge(uint32_t timestamp_usec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_BIT_DECODER__ */
