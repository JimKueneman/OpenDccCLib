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
 * @file dcc_service_mode_register.h
 * @brief Physical register mode CV programming (legacy, registers 1-8).
 *
 * @details Addresses physical registers 1-8. Per S-9.2.3 each operation is a
 * two-step sequence: a page-preset (page register -> page 1) followed by the
 * register verify/write. This is the oldest service mode, supported by all decoders.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_SERVICE_MODE_REGISTER__
#define __DCC_SERVICE_MODE_REGISTER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Start a service mode operation via the common module. */
    bool (*begin_operation)(const dcc_packet_t *packet, dcc_service_mode_step_callback_t callback, bool is_write_operation, uint8_t command_repeat, uint8_t recovery_count);

        /** @brief Check if the common module is idle. */
    bool (*is_common_idle)(void);

        /** @brief User callback: programming operation finished. NULL = no notification. */
    void (*on_complete)(dcc_service_mode_result_t result);

} interface_dcc_service_mode_register_t;

    /** @brief Instance context for the register service mode module. */
typedef struct {

    const interface_dcc_service_mode_register_t *interface;
    uint8_t register_state;    /**< register_state_enum cast to uint8_t */
    uint8_t register_number;   /**< pending register for the command step */
    uint8_t value;             /**< pending value for the command step */
    bool is_write;             /**< true = write, false = verify */

} dcc_service_mode_register_context_t;

    /**
     * @brief Initialize the register service mode module.
     * @param context Pointer to @ref dcc_service_mode_register_context_t instance.
     * @param interface Pointer to populated @ref interface_dcc_service_mode_register_t struct.
     */
    extern void DccServiceModeRegister_initialize(dcc_service_mode_register_context_t *context, const interface_dcc_service_mode_register_t *interface);

    /**
     * @brief Write a register value using register mode.
     * @param context Pointer to @ref dcc_service_mode_register_context_t instance.
     * @param register_number Register number to write (1-8).
     * @param value Byte value to write.
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeRegister_write(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value);

    /**
     * @brief Verify a register value using register mode.
     * @param context Pointer to @ref dcc_service_mode_register_context_t instance.
     * @param register_number Register number to verify (1-8).
     * @param value Expected byte value.
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeRegister_verify(dcc_service_mode_register_context_t *context, uint8_t register_number, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#endif /* __DCC_SERVICE_MODE_REGISTER__ */
