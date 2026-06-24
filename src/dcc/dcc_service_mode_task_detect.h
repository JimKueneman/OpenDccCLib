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
 * @file dcc_service_mode_task_detect.h
 * @brief Task orchestrator for service mode decoder detection (S-9.2.3 Appendix A/B).
 *
 * @details Probes the decoder to determine ALL supported service modes, returned
 * as a DCC_SERVICE_MODE_SUPPORTED_* bitmask. Stages run in order: Direct, Paged,
 * Register (all addressing CV#8 / register 8 = Manufacturer ID), then Address-Only
 * (CV#1). Direct uses the spec bit-verify method (§E lines 95-99); if Direct is
 * supported its full CV#8 byte is read so the Paged and Register stages reduce to a
 * single value-verify instead of a 0..255 scan. Whichever of Paged/Register scans
 * first also learns the CV#8 value for the other. Address-Only is probed separately
 * (CV#1, 0..127) since it cannot reach CV#8. Address-Only is NOT assumed to be
 * universally supported (the spec mandates it for command stations, not for every
 * decoder). If nothing acknowledges, supported_modes == 0 and result is NO_ACK.
 * Singleton — only one service track per command station.
 *
 * @author Jim Kueneman
 * @date 23 Jun 2026
 */

#ifndef __DCC_SERVICE_MODE_TASK_DETECT__
#define __DCC_SERVICE_MODE_TASK_DETECT__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Direct primitive: verify a single CV bit (used for CV#8 Direct detection). */
    bool (*direct_verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /** @brief Paged primitive: verify a CV byte (used for Paged mode detection probe). */
    bool (*paged_verify)(uint16_t cv_number, uint8_t value);

        /** @brief Register primitive: verify a register value (used for Register mode detection probe). */
    bool (*register_verify)(uint8_t register_number, uint8_t value);

        /** @brief Address-only primitive: verify the short address (CV#1, used for Address-Only detection probe). */
    bool (*address_verify)(uint8_t address);

        /** @brief Check if the primitive module is idle and ready for a new operation. */
    bool (*is_idle)(void);

        /** @brief Signal the application to begin monitoring the ACK pin.
         *         Called at the Packet End bit of the second service mode packet (spec §D). */
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_detect_t;

    /**
     * @brief Initialize the detect task module. Call once during DccConfig_initialize().
     * @param interface Pointer to populated @ref interface_dcc_service_mode_task_detect_t (wired by dcc_config.c).
     */
    extern void DccServiceModeTaskDetect_initialize(const interface_dcc_service_mode_task_detect_t *interface);

    /**
     * @brief Probe the decoder for ALL supported service modes.
     *        Runs Direct → Paged → Register → Address-Only, accumulating a
     *        DCC_SERVICE_MODE_SUPPORTED_* bitmask. If none acknowledge, the bitmask
     *        is 0 and result is DCC_SERVICE_MODE_NO_ACK; otherwise SUCCESS.
     * @param on_detect Called when detection completes; supported_modes = capability bitmask.
     * @return true if started, false if busy.
     */
    extern bool DccServiceModeTaskDetect_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect);

    /**
     * @brief Notify the task module that the primitive has finished its full operation
     *        (including recovery packets). Wire this into the primitive's on_complete slot
     *        via dcc_config.c. The task module advances its state machine on this event;
     *        the ACK outcome is taken from @p result (SUCCESS = ACK detected by the common
     *        module's pulse-width measurement, anything else = no ACK).
     * @param result Result of the primitive operation (passed through from primitive callback).
     */
    extern void DccServiceModeTaskDetect_on_primitive_complete(dcc_service_mode_result_t result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#endif /* __DCC_SERVICE_MODE_TASK_DETECT__ */
