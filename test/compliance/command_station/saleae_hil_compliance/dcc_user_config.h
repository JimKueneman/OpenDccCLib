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
 * @brief This is the FIRST file to customize when porting OpenDccCLib to a new project. It controls which library features are compiled and sets buffer/pool sizes.
 *
 * @details Every project that uses OpenDccCLib must provide this file. The library headers
 * include it via "dcc_user_config.h", so it must be on your compiler's include path.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection
//
// Enable exactly one role (or both if your device is a combined station+decoder).
// DCC_COMPILE_COMMAND_STATION  -- builds the packet scheduler, bit encoder,
//                                 service mode, and RailCom support.
// DCC_COMPILE_DECODER          -- builds the bit decoder, CV handling, and
//                                 packet parser for receiving DCC commands.
// =============================================================================

    /** @brief Build the command-station role: packet scheduler, bit encoder, service mode, RailCom cutout. */
#define DCC_COMPILE_COMMAND_STATION
// #define DCC_COMPILE_DECODER
    /** @brief Build the RailCom cutout and receive path (comment out to strip). */
#define DCC_COMPILE_RAILCOM           // RailCom cutout + Rx (comment out to strip)

// =============================================================================
// Service Mode Selection (requires DCC_COMPILE_COMMAND_STATION)
//
// Each flag compiles in one NMRA service-mode programming method. Disable any
// you do not need to save flash. All four are independent of each other.
//   DIRECT   -- CV direct byte/bit read/write  (most common, recommended)
//   PAGED    -- paged mode for older decoders
//   REGISTER -- register mode for very old decoders
//   ADDRESS  -- short-address-only programming (legacy)
// =============================================================================

    /** @brief Direct-mode CV byte/bit read/write (S-9.2.3; most common, recommended). */
#define DCC_COMPILE_SERVICE_MODE_DIRECT
    /** @brief Paged mode for older decoders. */
#define DCC_COMPILE_SERVICE_MODE_PAGED
    /** @brief Register mode for very old decoders. */
#define DCC_COMPILE_SERVICE_MODE_REGISTER
    /** @brief Short-address-only programming (legacy). */
#define DCC_COMPILE_SERVICE_MODE_ADDRESS

// =============================================================================
// Service Mode Task Selection (requires DCC_COMPILE_COMMAND_STATION)
// =============================================================================

    /** @brief Application-level Direct-mode tasks (read/write CV and bit with retries). */
#define DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    /** @brief Application-level paged-mode tasks. */
#define DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    /** @brief Application-level register-mode tasks. */
#define DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    /** @brief Application-level address-mode tasks. */
#define DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    /** @brief Application-level task that probes which service modes a decoder supports. */
#define DCC_COMPILE_SERVICE_MODE_TASK_DETECT

// =============================================================================
// Command Station Buffer & Pool Sizes
//
// Tune these for your MCU's available RAM. Larger values support more locos
// and more concurrent operations but use more memory.
// =============================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

    /**
     * @brief Max concurrent packets the scheduler can hold. Each slot is ~20 bytes.
     *
     * @details 24 handles 10 locos with speed+function refresh plus a few one-shot
     * packets. Increase if you see scheduler-full errors; decrease to save RAM.
     */
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT    24

    /**
     * @brief Ops-mode preamble bits the command station transmits.
     *
     * @details RailCom build: >= 16 required (S-9.3.2 sec 2.4), 18 recommended for
     * post-cutout relock margin.
     */
#define USER_DEFINED_DCC_PREAMBLE_BITS_OPS       18

    /**
     * @brief Max locos with automatic speed/function refresh.
     *
     * @details Each loco uses one scheduler slot permanently. Must be <=
     * USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT minus headroom for one-shot packets
     * (accessory, CV, etc.).
     */
#define USER_DEFINED_DCC_MAX_LOCOS               10

    /** @brief Depth of the RailCom receive ring buffer. Only matters when uart_read is wired in dcc_railcom_hw_t; 4 is usually enough. */
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH     4

    /** @brief Number of retries for service mode read/write/verify before giving up. */
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES     3

    /**
     * @brief Service-mode ACK current threshold in milliamps.
     *
     * @details The decoder pulls at least this much for between ACK_MIN and ACK_MAX
     * microseconds to signal success. NMRA S-9.2.3 recommends 60 mA, 5-7 ms. Adjust
     * if your current-sense circuit has different scaling. On this bench the mock ACK
     * pin reads as 100 when high, so it clears the threshold.
     */
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA        60
    /** @brief Shortest pulse accepted as an ACK, in microseconds (S-9.2.3: 6 ms - 1 ms). */
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US   5000
    /** @brief Longest pulse accepted as an ACK, in microseconds (S-9.2.3: 6 ms + 1 ms). */
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US   7000

    /**
     * @brief Bridge brief current-sense dropouts during an ACK, in microseconds.
     *
     * @details Covers noisy motor loads and comparator chatter. 0 = strict
     * consecutive-high. Keep it a small fraction of the 6 ms ACK (single-digit
     * samples at the 58 us sample period) or it defeats the width discrimination.
     * ~116 us = 2 samples.
     */
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_USER_CONFIG__ */
