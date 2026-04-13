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
 * @file dcc_application_service_track.h
 * @brief Application-layer API for service track (programming) operations.
 *
 * @details Provides the user-facing functions for the service track DCC output:
 * power control, service mode entry/exit, and all programming operations
 * (direct, paged, register, address-only). Initialized by dcc_config.c during
 * DccConfig_initialize(). Application code includes this header instead of the
 * internal module headers.
 *
 * @author Jim Kueneman
 * @date 08 Apr 2026
 */

#ifndef __DCC_APPLICATION_SERVICE_TRACK__
#define __DCC_APPLICATION_SERVICE_TRACK__

#include "dcc_types.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — wired by dcc_config.c during initialization. */
typedef struct {

        /** @brief Start the DCC timer for this channel. */
    void (*timer_start)(uint16_t half_bit_period_usec);

        /** @brief Stop the DCC timer for this channel. */
    void (*timer_stop)(void);

        /** @brief Enable or disable track power. */
    void (*track_power_set)(bool enabled);

        /** @brief Start the bit encoder (begin generating DCC signal). */
    void (*encoder_start)(void);

        /** @brief Stop the bit encoder. */
    void (*encoder_stop)(void);

        /** @brief Enter service mode. Returns true if entered successfully. */
    bool (*enter_service_mode)(void);

        /** @brief Exit service mode. */
    void (*exit_service_mode)(void);

        /** @brief Check if service mode is currently active. */
    bool (*is_service_mode_active)(void);

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

        /** @brief Direct mode: write a CV byte. */
    bool (*direct_write_byte)(uint16_t cv_number, uint8_t value);

        /** @brief Direct mode: verify a CV byte. */
    bool (*direct_verify_byte)(uint16_t cv_number, uint8_t value);

        /** @brief Direct mode: write a single CV bit. */
    bool (*direct_write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /** @brief Direct mode: verify a single CV bit. */
    bool (*direct_verify_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

        /** @brief Paged mode: write a CV. */
    bool (*paged_write)(uint16_t cv_number, uint8_t value);

        /** @brief Paged mode: verify a CV. */
    bool (*paged_verify)(uint16_t cv_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

        /** @brief Register mode: write a register. */
    bool (*register_write)(uint8_t register_number, uint8_t value);

        /** @brief Register mode: verify a register. */
    bool (*register_verify)(uint8_t register_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

        /** @brief Address-only mode: write the short address (CV 1). */
    bool (*address_write)(uint8_t address);

        /** @brief Address-only mode: verify the short address (CV 1). */
    bool (*address_verify)(uint8_t address);

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

} interface_dcc_application_service_track_t;

    /**
     * @brief Initialize the service track application module.
     * @param interface Pointer to populated @ref interface_dcc_application_service_track_t struct (wired by dcc_config.c).
     */
extern void DccApplicationServiceTrack_initialize(const interface_dcc_application_service_track_t *interface);

    // =========================================================================
    // Power control
    // =========================================================================

    /** @brief Enable service track power output and start DCC signal generation. */
extern void DccApplicationServiceTrack_power_on(void);

    /** @brief Disable service track power output and stop DCC signal generation. */
extern void DccApplicationServiceTrack_power_off(void);

    // =========================================================================
    // Service mode entry/exit
    // =========================================================================

    /**
     * @brief Enter service mode on the service track.
     * @return true if service mode was activated, false if busy.
     */
extern bool DccApplicationServiceTrack_enter(void);

    /** @brief Exit service mode on the service track. */
extern void DccApplicationServiceTrack_exit(void);

    /** @brief Check if service mode is currently active. */
extern bool DccApplicationServiceTrack_is_active(void);

    // =========================================================================
    // Direct mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /** @brief Direct mode: write a CV byte. */
extern bool DccApplicationServiceTrack_direct_write_byte(uint16_t cv_number, uint8_t value);

    /** @brief Direct mode: verify a CV byte. */
extern bool DccApplicationServiceTrack_direct_verify_byte(uint16_t cv_number, uint8_t value);

    /** @brief Direct mode: write a single CV bit. */
extern bool DccApplicationServiceTrack_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value);

    /** @brief Direct mode: verify a single CV bit. */
extern bool DccApplicationServiceTrack_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

    // =========================================================================
    // Paged mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /** @brief Paged mode: write a CV. */
extern bool DccApplicationServiceTrack_paged_write(uint16_t cv_number, uint8_t value);

    /** @brief Paged mode: verify a CV. */
extern bool DccApplicationServiceTrack_paged_verify(uint16_t cv_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

    // =========================================================================
    // Register mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /** @brief Register mode: write a register. */
extern bool DccApplicationServiceTrack_register_write(uint8_t register_number, uint8_t value);

    /** @brief Register mode: verify a register. */
extern bool DccApplicationServiceTrack_register_verify(uint8_t register_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

    // =========================================================================
    // Address-only mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /** @brief Address-only mode: write the short address (CV 1). */
extern bool DccApplicationServiceTrack_address_write(uint8_t address);

    /** @brief Address-only mode: verify the short address (CV 1). */
extern bool DccApplicationServiceTrack_address_verify(uint8_t address);

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_SERVICE_TRACK__ */
