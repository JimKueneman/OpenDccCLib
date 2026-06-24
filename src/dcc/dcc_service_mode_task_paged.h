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
 * @file dcc_service_mode_task_paged.h
 * @brief Task orchestrator for Paged mode CV programming (S-9.2.3 §E).
 *
 * @details Sequences paged mode primitive operations to implement read_cv
 * (iterate paged_verify 0-255), write_cv (write + verify), read_bit
 * (full byte read, extract bit), and write_bit (read-modify-write sequence).
 * The paged primitive handles the page preset injection internally.
 * Singleton — only one service track per command station.
 *
 * @author Jim Kueneman
 * @date 23 Jun 2026
 */

#ifndef __DCC_SERVICE_MODE_TASK_PAGED__
#define __DCC_SERVICE_MODE_TASK_PAGED__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Paged primitive: verify a CV byte on the current page. */
    bool (*paged_verify)(uint16_t cv_number, uint8_t value);

        /** @brief Paged primitive: write a CV byte on the current page. */
    bool (*paged_write)(uint16_t cv_number, uint8_t value);

        /** @brief Check if the primitive module is idle and ready for a new operation. */
    bool (*is_idle)(void);

        /** @brief Signal the application to begin monitoring the ACK pin.
         *         Called at the Packet End bit of the second service mode packet (spec §D). */
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_paged_t;

    /**
     * @brief Initialize the paged task module. Call once during DccConfig_initialize().
     * @param interface Pointer to populated @ref interface_dcc_service_mode_task_paged_t (wired by dcc_config.c).
     */
    extern void DccServiceModeTaskPaged_initialize(const interface_dcc_service_mode_task_paged_t *interface);

    /**
     * @brief Read a CV byte using paged mode. Iterates paged_verify 0-255 until ACK.
     * @param cv CV number (1-1024).
     * @param on_complete Called when complete; value = CV byte found.
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or cv out of range.
     */
    extern bool DccServiceModeTaskPaged_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Write a CV byte then verify. Always 2 operations (write + targeted verify).
     * @param cv CV number (1-1024).
     * @param value Byte to write.
     * @param on_complete Called when complete; value = byte verified.
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or cv out of range.
     */
    extern bool DccServiceModeTaskPaged_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Read a single CV bit using paged mode. Reads full byte, extracts bit.
     * @param cv CV number (1-1024).
     * @param bit Bit position (0-7).
     * @param on_complete Called when complete; value = 0 or 1.
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or parameters out of range.
     */
    extern bool DccServiceModeTaskPaged_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Write a single CV bit using paged mode. Read-modify-write sequence.
     * @param cv CV number (1-1024).
     * @param bit Bit position (0-7).
     * @param bit_value Value to write.
     * @param on_complete Called when complete; value = bit value verified (0 or 1).
     * @param on_progress Called after each step (nullable).
     * @return true if started, false if busy or parameters out of range.
     */
    extern bool DccServiceModeTaskPaged_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /**
     * @brief Notify the task module that the primitive has finished its full operation
     *        (including recovery packets). Wire this into the primitive's on_complete slot
     *        via dcc_config.c. The task module advances its state machine on this event;
     *        the ACK outcome is taken from @p result (SUCCESS = ACK detected by the common
     *        module's pulse-width measurement, anything else = no ACK).
     * @param result Result of the primitive operation (passed through from primitive callback).
     */
    extern void DccServiceModeTaskPaged_on_primitive_complete(dcc_service_mode_result_t result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#endif /* __DCC_SERVICE_MODE_TASK_PAGED__ */
