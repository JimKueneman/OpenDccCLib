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
 * @file dcc_failsafe.h
 * @brief Packet-timeout fail-safe (S-9.2.4 §4, CV11).
 *
 * @details Watches the elapsed time since the last command packet addressed to
 * this decoder. If that time exceeds the CV11 (DCC_CV_PACKET_TIMEOUT) value the
 * decoder must bring all controlled devices to a stop, preventing runaway when
 * the command stream is lost. CV11 = 0 disables the time-out.
 *
 * The mapping of the CV11 byte to real time is DCC_FAILSAFE_CV11_UNIT_US per
 * LSB (S-9.2.4 leaves the unit manufacturer-defined; it only requires the
 * configurable maximum to reach at least 20 seconds).
 *
 * The module is purely a timer/edge detector: it never touches motor outputs
 * itself. It signals the application through on_failsafe_entered /
 * on_failsafe_exited, which the application uses to stop and resume its
 * controlled devices.
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#ifndef __DCC_FAILSAFE__
#define __DCC_FAILSAFE__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Read a CV value. REQUIRED. Used to fetch CV11. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Free-running microsecond clock. REQUIRED. */
    uint32_t (*get_timestamp_usec)(void);

        /** @brief Decoder entered fail-safe (packet time-out). NULL = none. */
    void (*on_failsafe_entered)(void);

        /** @brief Decoder exited fail-safe (addressed packet seen). NULL = none. */
    void (*on_failsafe_exited)(void);

} interface_dcc_failsafe_t;

    /**
     * @brief Initialize the fail-safe module.
     * @param interface Pointer to populated interface struct.
     *
     * @details Clears the tripped state and stamps "last packet seen = now" so a
     * freshly initialized decoder does not immediately time out.
     */
extern void DccFailsafe_initialize(const interface_dcc_failsafe_t *interface);

    /**
     * @brief Note that a command packet addressed to this decoder was received.
     *
     * @details Re-arms the time-out by re-stamping the last-packet clock. If the
     * decoder is currently in fail-safe, this also clears the state and fires
     * on_failsafe_exited exactly once. Call from the packet-decode path whenever
     * a packet matches our address (or broadcast) in Operations Mode.
     */
extern void DccFailsafe_note_valid_packet(void);

    /**
     * @brief Periodic poll. Call from the main loop (DccConfig_run).
     *
     * @details Reads CV11; when CV11 = 0 the time-out is disabled. Otherwise, if
     * the elapsed time since the last addressed packet has reached
     * CV11 * DCC_FAILSAFE_CV11_UNIT_US and the decoder is not already tripped,
     * fires on_failsafe_entered exactly once.
     */
extern void DccFailsafe_run(void);

    /**
     * @brief Query whether the decoder is currently in fail-safe.
     * @return true if tripped (timed out and not yet recovered).
     */
extern bool DccFailsafe_is_active(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_FAILSAFE__ */
