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
 * @file dcc_service_mode_task_address.h
 * @brief Task orchestrator for Address-Only mode CV programming (S-9.2.3 §E).
 *
 * @details Sequences address-only mode primitive operations to implement read
 * (iterate address_verify 0-127) and write (write + verify) of CV#1.
 * The decoder automatically clears CV#29 bit 5 and CV#19 when CV#1 is written —
 * the command station does not need to handle these side effects explicitly.
 * Singleton — only one service track per command station.
 *
 * @author Jim Kueneman
 * @date 23 Jun 2026
 */

#ifndef __DCC_SERVICE_MODE_TASK_ADDRESS__
#define __DCC_SERVICE_MODE_TASK_ADDRESS__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Address-only primitive: verify the short address (CV#1, 0-127). */
    bool (*address_verify)(uint8_t address);

        /** @brief Address-only primitive: write the short address (CV#1, 1-127). */
    bool (*address_write)(uint8_t address);

        /** @brief Check if the primitive module is idle and ready for a new operation. */
    bool (*is_idle)(void);

        /** @brief Signal the application to begin monitoring the ACK pin.
         *         Called at the Packet End bit of the second service mode packet (spec §D). */
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_address_t;

    /**
     * @brief Initialize the address task module. Call once during DccConfig_initialize().
     * @param interface Pointer to populated @ref interface_dcc_service_mode_task_address_t (wired by dcc_config.c).
     */
    extern void DccServiceModeTaskAddress_initialize(const interface_dcc_service_mode_task_address_t *interface);

    /**
     * @brief Read CV#1 (short address) using address-only mode. Iterates address_verify 0-127 until ACK.
     * @param on_complete Called when complete; value = address found (0-127).
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy.
     */
    extern bool DccServiceModeTaskAddress_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Write CV#1 (short address) then verify. Always 2 operations (write + verify).
     *        Decoder automatically clears CV#29 bit 5 and CV#19 on success.
     * @param address Address to write (1-127).
     * @param on_complete Called when complete; value = address verified.
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or address out of range.
     */
    extern bool DccServiceModeTaskAddress_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Read a single bit from CV#1. Reads full byte via iteration, extracts bit.
     * @param bit Bit position (0-6; bit 7 is always 0 in 7-bit address).
     * @param on_complete Called when complete; value = 0 or 1.
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or bit out of range.
     */
    extern bool DccServiceModeTaskAddress_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Write a single bit to CV#1 using read-modify-write. Reads byte first, then writes modified byte.
     * @param bit Bit position (0-6).
     * @param bit_value Value to write.
     * @param on_complete Called when complete; value = bit value verified (0 or 1).
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or bit out of range.
     */
    extern bool DccServiceModeTaskAddress_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Notify the task module that the primitive has finished its full operation
     *        (including recovery packets). Wire this into the primitive's on_complete slot
     *        via dcc_config.c. The task module advances its state machine on this event;
     *        the ACK outcome is taken from @p result (SUCCESS = ACK detected by the common
     *        module's pulse-width measurement, anything else = no ACK).
     * @param result Result of the primitive operation (passed through from primitive callback).
     */
    extern void DccServiceModeTaskAddress_on_primitive_complete(dcc_service_mode_result_t result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

#endif /* __DCC_SERVICE_MODE_TASK_ADDRESS__ */
