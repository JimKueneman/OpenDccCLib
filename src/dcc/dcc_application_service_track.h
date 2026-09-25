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
 * @details Legacy application layer for the service track (programming) output:
 * power control, service mode entry/exit, and the direct, paged, register and
 * address-only programming operations, forwarded through an interface struct
 * that the application (or test) populates. This module is still compiled and
 * unit-tested but is NOT wired by dcc_config.c; the current command-station API
 * is dcc_application_command_station_service_track. Every call is a no-op (or
 * returns false) until DccApplicationServiceTrack_initialize has been called.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_APPLICATION_SERVICE_TRACK__
#define __DCC_APPLICATION_SERVICE_TRACK__

#include "dcc_types.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- populated by the caller; dcc_config.c does not wire this legacy module. */
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
         *
         * @details Stores the interface pointer; nothing is powered and service mode is not entered.
         *
         * @param interface Pointer to a populated @ref interface_dcc_application_service_track_t; must remain valid while the module is used.
         */
    extern void DccApplicationServiceTrack_initialize(const interface_dcc_application_service_track_t *interface);

    // =========================================================================
    // Power control
    // =========================================================================

        /**
         * @brief Enable service track power output and start DCC signal generation.
         *
         * @details Calls track_power_set(true), timer_start with the one-bit half period
         * (DCC_ONE_BIT_HALF_PERIOD_US) and encoder_start, in that order. No-op before
         * initialization.
         */
    extern void DccApplicationServiceTrack_power_on(void);

        /**
         * @brief Disable service track power output and stop DCC signal generation.
         *
         * @details Calls encoder_stop, timer_stop and track_power_set(false), in that
         * order (the reverse of power-on). No-op before initialization.
         */
    extern void DccApplicationServiceTrack_power_off(void);

    // =========================================================================
    // Service mode entry/exit
    // =========================================================================

        /**
         * @brief Enter service mode on the service track.
         *
         * @details Powers the track (track_power_set, timer_start, encoder_start) and then
         * calls the enter_service_mode hook. The track stays powered even when the hook
         * refuses.
         *
         * @return true if service mode was activated; false if the hook refused (busy) or the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_enter(void);

        /**
         * @brief Exit service mode on the service track.
         *
         * @details Calls the exit_service_mode hook, then stops the encoder and timer and
         * removes track power. No-op before initialization.
         */
    extern void DccApplicationServiceTrack_exit(void);

        /**
         * @brief Check if service mode is currently active.
         *
         * @details Forwards to the is_service_mode_active hook.
         *
         * @return true if service mode is active; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_is_active(void);

    // =========================================================================
    // Direct mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

        /**
         * @brief Direct mode: write a CV byte.
         *
         * @details Forwards to the direct_write_byte hook (S-9.2.3 direct mode).
         *
         * @param cv_number CV number (1-based).
         * @param value Byte value to write.
         *
         * @return true if the hook reports success; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_direct_write_byte(uint16_t cv_number, uint8_t value);

        /**
         * @brief Direct mode: verify a CV byte.
         *
         * @details Forwards to the direct_verify_byte hook (S-9.2.3 direct mode).
         *
         * @param cv_number CV number (1-based).
         * @param value Expected byte value.
         *
         * @return true if the hook reports a match; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_direct_verify_byte(uint16_t cv_number, uint8_t value);

        /**
         * @brief Direct mode: write a single CV bit.
         *
         * @details Forwards to the direct_write_bit hook (S-9.2.3 direct mode, bit manipulation).
         *
         * @param cv_number CV number (1-based).
         * @param bit_position Bit position within the CV (0-7).
         * @param bit_value Bit value to write.
         *
         * @return true if the hook reports success; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /**
         * @brief Direct mode: verify a single CV bit.
         *
         * @details Forwards to the direct_verify_bit hook (S-9.2.3 direct mode, bit manipulation).
         *
         * @param cv_number CV number (1-based).
         * @param bit_position Bit position within the CV (0-7).
         * @param bit_value Expected bit value.
         *
         * @return true if the hook reports a match; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

    // =========================================================================
    // Paged mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

        /**
         * @brief Paged mode: write a CV.
         *
         * @details Forwards to the paged_write hook (S-9.2.3 paged mode).
         *
         * @param cv_number CV number (1-based).
         * @param value Byte value to write.
         *
         * @return true if the hook reports success; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_paged_write(uint16_t cv_number, uint8_t value);

        /**
         * @brief Paged mode: verify a CV.
         *
         * @details Forwards to the paged_verify hook (S-9.2.3 paged mode).
         *
         * @param cv_number CV number (1-based).
         * @param value Expected byte value.
         *
         * @return true if the hook reports a match; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_paged_verify(uint16_t cv_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

    // =========================================================================
    // Register mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

        /**
         * @brief Register mode: write a register.
         *
         * @details Forwards to the register_write hook (S-9.2.3 register mode).
         *
         * @param register_number Register number as defined by S-9.2.3 register mode.
         * @param value Byte value to write.
         *
         * @return true if the hook reports success; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_register_write(uint8_t register_number, uint8_t value);

        /**
         * @brief Register mode: verify a register.
         *
         * @details Forwards to the register_verify hook (S-9.2.3 register mode).
         *
         * @param register_number Register number as defined by S-9.2.3 register mode.
         * @param value Expected byte value.
         *
         * @return true if the hook reports a match; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_register_verify(uint8_t register_number, uint8_t value);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

    // =========================================================================
    // Address-only mode programming
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

        /**
         * @brief Address-only mode: write the short address (CV 1).
         *
         * @details Forwards to the address_write hook (S-9.2.3 address-only mode).
         *
         * @param address Short address to program into CV 1.
         *
         * @return true if the hook reports success; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_address_write(uint8_t address);

        /**
         * @brief Address-only mode: verify the short address (CV 1).
         *
         * @details Forwards to the address_verify hook (S-9.2.3 address-only mode).
         *
         * @param address Expected short address in CV 1.
         *
         * @return true if the hook reports a match; false otherwise or when the module is not initialized.
         */
    extern bool DccApplicationServiceTrack_address_verify(uint8_t address);

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_SERVICE_TRACK__ */
