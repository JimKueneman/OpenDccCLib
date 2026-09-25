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
 * @file dcc_service_mode_task_direct.h
 * @brief Task orchestrator for Direct mode CV programming (S-9.2.3 §E).
 *
 * @details Sequences direct mode primitive operations to implement read_cv
 * (8x verify_bit bits 0-7, then verify_byte), write_cv (write + verify), read_bit (verify_bit
 * of 1, then of 0 if silent),
 * and write_bit (write_bit + verify_bit). Drives the state machine forward on
 * each on_ack() notification from the application hardware layer.
 * Singleton — only one service track per command station.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_SERVICE_MODE_TASK_DIRECT__
#define __DCC_SERVICE_MODE_TASK_DIRECT__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Direct primitive: verify a single CV bit. */
    bool (*verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /** @brief Direct primitive: verify a CV byte. */
    bool (*verify_byte)(uint16_t cv_number, uint8_t value);

        /** @brief Direct primitive: write a CV byte. */
    bool (*write_byte)(uint16_t cv_number, uint8_t value);

        /** @brief Direct primitive: write a single CV bit. */
    bool (*write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /** @brief Check if the primitive module is idle and ready for a new operation. */
    bool (*is_idle)(void);

        /** @brief Signal the application to begin monitoring the ACK pin.
         *         Called at the Packet End bit of the second service mode packet (spec §D). */
    void (*on_start_ack_scan)(void);

} interface_dcc_service_mode_task_direct_t;

        /**
         * @brief Initialize the direct task module. Call once during DccConfig_initialize().
         * @param interface Pointer to populated @ref interface_dcc_service_mode_task_direct_t (wired by dcc_config.c).
         */
    extern void DccServiceModeTaskDirect_initialize(const interface_dcc_service_mode_task_direct_t *interface);

        /**
         * @brief Read a CV byte using direct mode. Sequences 8 verify_bit operations (bits 0-7),
         * then one verify_byte of the assembled value.
         * @param cv_number CV number (1-1024).
         * @param on_complete @ref dcc_service_mode_task_on_complete_callback_t called when complete: SUCCESS with the CV byte if verify_byte is ACKed;
         *        NO_ACK (value 0) if nothing was ACKed at all, i.e. no decoder; VERIFY_FAIL (value =
         *        the assembled byte) if some bits were ACKed but the byte was not; BUSY (value 0) if a
         *        later primitive could not be started.
         * @param on_progress @ref dcc_service_mode_task_on_progress_callback_t called after each of the 9 steps (nullable).
         * @return true if started; false if another operation is running, the CV is out of range, or the first primitive could not start.
         */
    extern bool DccServiceModeTaskDirect_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Write a CV byte then verify. Always 2 operations (write + verify).
         * @param cv_number CV number (1-1024).
         * @param value Byte to write.
         * @param on_complete @ref dcc_service_mode_task_on_complete_callback_t called when complete: SUCCESS if the verify was ACKed,
         *        VERIFY_FAIL if not (value = the byte written either way); BUSY (value 0) if the verify could not be started.
         * @param on_progress @ref dcc_service_mode_task_on_progress_callback_t called after each step (nullable).
         * @return true if started; false if another operation is running, the CV is out of range, or the write could not start.
         */
    extern bool DccServiceModeTaskDirect_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Read a single CV bit. Issues a verify_bit for value 1; if that is not ACKed,
         * a second verify_bit for value 0 confirms the bit is really 0 rather than unanswered.
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position (0-7).
         * @param on_complete @ref dcc_service_mode_task_on_complete_callback_t called when complete: SUCCESS with value 1 or 0 when the decoder
         *        ACKed that value; NO_ACK (value 0) when neither verify was ACKed, i.e. no decoder; BUSY (value 0) if the
         *        confirming verify could not be started.
         * @param on_progress @ref dcc_service_mode_task_on_progress_callback_t; not used by this operation (nullable).
         * @return true if started; false if another operation is running, a parameter is out of range, or the first verify could not start.
         */
    extern bool DccServiceModeTaskDirect_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Write a single CV bit then verify. Always 2 operations (write_bit + verify_bit).
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position (0-7).
         * @param bit_value Value to write.
         * @param on_complete @ref dcc_service_mode_task_on_complete_callback_t called when complete: SUCCESS if the verify was ACKed,
         *        VERIFY_FAIL if not (value = the bit written, 0 or 1); BUSY (value 0) if the verify could not be started.
         * @param on_progress @ref dcc_service_mode_task_on_progress_callback_t; not used by this operation (nullable).
         * @return true if started; false if another operation is running, a parameter is out of range, or the write could not start.
         */
    extern bool DccServiceModeTaskDirect_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Notify the task module that the primitive has finished its full operation
         *        (including recovery packets). Wire this into the primitive's on_complete slot
         *        via dcc_config.c. The task module advances its state machine on this event;
         *        the ACK outcome is taken from @p result (SUCCESS = ACK detected by the common
         *        module's pulse-width measurement, anything else = no ACK).
         * @param result @ref dcc_service_mode_result_enum of the primitive operation (passed through from primitive callback).
         */
    extern void DccServiceModeTaskDirect_on_primitive_complete(dcc_service_mode_result_enum result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#endif /* __DCC_SERVICE_MODE_TASK_DIRECT__ */
