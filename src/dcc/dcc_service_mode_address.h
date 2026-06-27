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
 * @file dcc_service_mode_address.h
 * @brief Address-only mode CV programming (writes CV 1 only).
 *
 * @details Minimal service mode that only writes the primary short address
 * (CV 1). Uses register mode write to register 1. Single-step operation.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_SERVICE_MODE_ADDRESS__
#define __DCC_SERVICE_MODE_ADDRESS__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

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

} interface_dcc_service_mode_address_t;

    /** @brief Instance context for the address-only service mode module. */
typedef struct {

    const interface_dcc_service_mode_address_t *interface;
    uint8_t address_state;     /**< address_state_enum cast to uint8_t */
    uint8_t address;           /**< pending address for the command step */
    bool is_write;             /**< true = write, false = verify */

} dcc_service_mode_address_context_t;

    /**
     * @brief Initialize the address-only service mode module.
     * @param context Pointer to @ref dcc_service_mode_address_context_t instance.
     * @param interface Pointer to populated @ref interface_dcc_service_mode_address_t struct.
     */
    extern void DccServiceModeAddress_initialize(dcc_service_mode_address_context_t *context, const interface_dcc_service_mode_address_t *interface);

    /**
     * @brief Write the short address (CV 1) using address-only mode.
     * @param context Pointer to @ref dcc_service_mode_address_context_t instance.
     * @param address The short address to write (1-127).
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeAddress_write(dcc_service_mode_address_context_t *context, uint8_t address);

    /**
     * @brief Verify the short address (CV 1) using address-only mode.
     * @param context Pointer to @ref dcc_service_mode_address_context_t instance.
     * @param address The short address to verify (1-127).
     * @return true if operation started, false if busy.
     */
    extern bool DccServiceModeAddress_verify(dcc_service_mode_address_context_t *context, uint8_t address);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#endif /* __DCC_SERVICE_MODE_ADDRESS__ */
