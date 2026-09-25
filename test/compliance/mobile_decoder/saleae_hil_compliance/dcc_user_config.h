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
 * @file dcc_user_config.h
 * @brief Project-level feature flags for the DCC library.
 *
 * @details The DCC library is compiled for EITHER a decoder OR a command station
 * (or both, though that is unusual).  Uncomment the role you need below.
 *
 * This file is included by the library's internal headers, so every .c
 * file in the library sees these defines automatically.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection
//
// Enable exactly ONE of these.  The library uses #ifdef to include or
// exclude large blocks of decoder-only or command-station-only code.
// =============================================================================

// #define DCC_COMPILE_COMMAND_STATION   /* not used in this demo */
    /** @brief Build the decoder role: bit decoder, packet parser, CV handling. */
#define DCC_COMPILE_DECODER
    /** @brief Build the RailCom transmit path (comment out to strip). */
#define DCC_COMPILE_RAILCOM           // RailCom Tx (comment out to strip)

// =============================================================================
// Decoder Configuration
// =============================================================================

#ifdef DCC_COMPILE_DECODER

    /**
     * @brief How many function outputs the decoder tracks (F0 through F28 = 29).
     *
     * @details Increase this if your hardware has more outputs; the library
     * allocates a bool array of this size.
     */
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS  29

    /**
     * @brief Decoder received-packet FIFO depth (>= 2; one slot reserved).
     *
     * @details The end-bit path enqueues; DccConfig_run() drains and dispatches.
     */
#define USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH      8

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_USER_CONFIG__ */
