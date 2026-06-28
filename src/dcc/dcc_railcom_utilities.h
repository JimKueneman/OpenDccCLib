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
 * @file dcc_railcom_utilities.h
 * @brief Pure RailCom helper functions (4/8 codec) shared across roles.
 *
 * @details Stateless, dependency-free helpers. Unlike stateful modules these need
 * no interface-struct injection -- they are directly includable and standalone
 * testable. Each codec direction is role-gated so a build compiles only the
 * direction its role uses: decoder/accessory encode for transmit, command station
 * decode for receive.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#ifndef __DCC_RAILCOM_UTILITIES__
#define __DCC_RAILCOM_UTILITIES__

#include "dcc_types.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(DCC_COMPILE_DECODER) || defined(DCC_COMPILE_ACCESSORY_DECODER)

        /**
         * @brief Encode a 6-bit value to its 8-bit DC-balanced RailCom codeword.
         *
         * @param value 6-bit data value (0x00-0x3F).
         *
         * @return 8-bit codeword, or 0x00 if @p value is out of range.
         */
    extern uint8_t DccRailcomUtilities_encode_byte(uint8_t value);

#endif /* DCC_COMPILE_DECODER || DCC_COMPILE_ACCESSORY_DECODER */

#if defined(DCC_COMPILE_COMMAND_STATION)

        /**
         * @brief Decode an 8-bit RailCom codeword to its 6-bit value or special token.
         *
         * @param encoded Received 8-bit codeword.
         *
         * @return 6-bit data value (0x00-0x3F), or a DCC_RAILCOM_DECODE_* token for
         *         invalid / ACK / NACK code words.
         */
    extern uint8_t DccRailcomUtilities_decode_byte(uint8_t encoded);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM */

#endif /* __DCC_RAILCOM_UTILITIES__ */
