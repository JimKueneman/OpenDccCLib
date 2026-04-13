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
 * @file dcc_service_mode_direct.h
 * @brief Direct mode CV programming (byte write/verify, bit write/verify).
 *
 * @details The modern preferred service mode. Supports direct byte write,
 * direct byte verify, direct bit write, and direct bit verify operations.
 * Each operation is single-step (one call to the common module).
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_SERVICE_MODE_DIRECT__
#define __DCC_SERVICE_MODE_DIRECT__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Start a service mode operation via the common module. */
    bool (*begin_operation)(const dcc_packet_t *packet, dcc_service_mode_step_callback_t callback, bool is_write_operation, uint8_t recovery_count);

        /** @brief Check if the common module is idle. */
    bool (*is_common_idle)(void);

        /** @brief User callback: programming operation finished. NULL = no notification. */
    void (*on_complete)(dcc_service_mode_result_t result);

} interface_dcc_service_mode_direct_t;

    /** @brief Instance context for the direct service mode module. */
typedef struct {

    const interface_dcc_service_mode_direct_t *interface;

} dcc_service_mode_direct_context_t;

    /**
     * @brief Initialize the direct service mode module.
     * @param context Pointer to @ref dcc_service_mode_direct_context_t instance.
     * @param interface Pointer to populated @ref interface_dcc_service_mode_direct_t struct.
     */
    extern void DccServiceModeDirect_initialize(dcc_service_mode_direct_context_t *context, const interface_dcc_service_mode_direct_t *interface);

    /**
     * @brief Write a byte to a CV using direct mode.
     * @param context Pointer to @ref dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to write (1-1024).
     * @param value Byte value to write.
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeDirect_write_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value);

    /**
     * @brief Verify a CV byte value using direct mode.
     * @param context Pointer to @ref dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to verify (1-1024).
     * @param value Expected byte value.
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeDirect_verify_byte(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t value);

    /**
     * @brief Write a single bit to a CV using direct mode.
     * @param context Pointer to @ref dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to write (1-1024).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Value to write (true = 1, false = 0).
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeDirect_write_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value);

    /**
     * @brief Verify a single bit in a CV using direct mode.
     * @param context Pointer to @ref dcc_service_mode_direct_context_t instance.
     * @param cv_number CV number to verify (1-1024).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Expected bit value (true = 1, false = 0).
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeDirect_verify_bit(dcc_service_mode_direct_context_t *context, uint16_t cv_number, uint8_t bit_position, bool bit_value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#endif /* __DCC_SERVICE_MODE_DIRECT__ */
