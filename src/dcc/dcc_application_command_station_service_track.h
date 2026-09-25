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
 * @file dcc_application_command_station_service_track.h
 * @brief Application-layer API for command station service track (programming) operations.
 *
 * @details Provides the user-facing functions for the service track DCC output:
 * power control, service mode entry/exit, and all programming operations. The
 * programming surface is the high-level TASK API — read_cv, write_cv, read_bit,
 * write_bit (per mode), plus register factory-reset and decoder mode detection.
 * Each task sequences and verifies the underlying primitive packet operations and
 * reports its result through the per-call on_complete callback; the raw primitive
 * operations are internal and are driven only by the task layer.
 *
 * Initialized by dcc_config.c during DccConfig_initialize(). Application code
 * includes this header instead of the internal module headers.
 *
 * This module owns the command-station service-track application layer.
 * It delegates timer, encoder, and programming operations through an interface
 * struct wired at initialization time by dcc_config.c. Service mode must be
 * entered before any programming operation and exited afterward; the module
 * manages power sequencing automatically on enter/exit.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_APPLICATION_COMMAND_STATION_SERVICE_TRACK__
#define __DCC_APPLICATION_COMMAND_STATION_SERVICE_TRACK__

#include "dcc_types.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — wired by dcc_config.c during initialization. */
typedef struct {

        /** @brief Enable or disable service track power (H-bridge). */
    void (*track_power_set)(bool enabled);

        /** @brief Start the DCC timer for this channel. */
    void (*timer_start)(uint16_t half_bit_period_usec);

        /** @brief Stop the DCC timer for this channel. */
    void (*timer_stop)(void);

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

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

        /** @brief Direct mode task: read a CV byte. */
    bool (*direct_read_cv)(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: write a CV byte (with verify). */
    bool (*direct_write_cv)(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: read a single CV bit. */
    bool (*direct_read_bit)(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: write a single CV bit (with verify). */
    bool (*direct_write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

        /** @brief Paged mode task: read a CV byte. */
    bool (*paged_read_cv)(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: write a CV byte (with verify). */
    bool (*paged_write_cv)(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: read a single CV bit. */
    bool (*paged_read_bit)(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: write a single CV bit (with verify). */
    bool (*paged_write_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

        /** @brief Register mode task: read a CV byte. */
    bool (*register_read_cv)(uint16_t cv_number, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: write a CV byte (with verify). */
    bool (*register_write_cv)(uint16_t cv_number, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: read a single CV bit. */
    bool (*register_read_bit)(uint16_t cv_number, uint8_t bit_position, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: write a single CV bit (with verify). */
    bool (*register_write_bit)(
                uint16_t cv_number,
                uint8_t bit_position,
                bool bit_value,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: decoder factory reset (write 8 to register 8). */
    bool (*register_factory_reset)(dcc_service_mode_task_on_complete_callback_t on_complete);

        /** @brief Register mode task: verify a single register value. */
    bool (*register_verify_value)(uint16_t cv_number, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

        /** @brief Address-only mode task: read CV#1 (short address). */
    bool (*address_read)(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: write CV#1 (short address, with verify). */
    bool (*address_write)(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: verify CV#1 against a value. */
    bool (*address_verify)(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: read a single bit of CV#1. */
    bool (*address_read_bit)(uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: write a single bit of CV#1 (with verify). */
    bool (*address_write_bit)(uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

        /** @brief Detect mode task: probe the decoder for all supported service modes. */
    bool (*detect_mode)(dcc_service_mode_task_on_detect_callback_t on_detect);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

} interface_dcc_application_command_station_service_track_t;

        /**
         * @brief Initialize the command station service track application module.
         *
         * @details Every other function in this module is a no-op (or returns
         *  false) until this has been called.
         *
         * @param interface Pointer to populated @ref interface_dcc_application_command_station_service_track_t struct (wired by dcc_config.c).
         */
    extern void DccApplicationCommandStationServiceTrack_initialize(const interface_dcc_application_command_station_service_track_t *interface);

    // =========================================================================
    // Power control
    // =========================================================================

        /**
         * @brief Enable service track power output and start DCC signal generation.
         *
         * @details Applies track power first, then starts the timer at the DCC
         *  one-bit half period and starts the bit encoder. Not needed before
         *  enter_service_mode(), which performs the same sequence itself.
         */
    extern void DccApplicationCommandStationServiceTrack_power_on(void);

        /**
         * @brief Disable service track power output and stop DCC signal generation.
         *
         * @details Stops the bit encoder and the timer, then removes track power
         *  last.
         */
    extern void DccApplicationCommandStationServiceTrack_power_off(void);

    // =========================================================================
    // Service mode entry/exit
    // =========================================================================

        /**
         * @brief Enter service mode on the service track.
         *
         * @details Applies track power, starts the timer and encoder, then enters
         *  the service-mode core; power_on() is not needed first.
         *
         * @return true if service mode was activated; false if the module is
         *  uninitialized or the core refused entry.
         */
    extern bool DccApplicationCommandStationServiceTrack_enter_service_mode(void);

        /**
         * @brief Exit service mode, stop the signal and remove track power.
         *
         * @details Leaves the service-mode core first, then stops the encoder
         *  and the timer and removes track power last.
         */
    extern void DccApplicationCommandStationServiceTrack_exit_service_mode(void);

        /**
         * @brief Check if service mode is currently active.
         *
         * @return true if service mode is active; false otherwise, including
         *  when the module has not been initialized.
         */
    extern bool DccApplicationCommandStationServiceTrack_is_service_mode_active(void);

    // =========================================================================
    // Direct mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

        /**
         * @brief Direct mode: read a CV byte (8 bit-verifies, then a confirming byte verify).
         *
         * @details Starts the task and returns at once; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the CV byte read.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each of the 9 steps; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_direct_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Direct mode: write a CV byte, then verify it.
         *
         * @details Two operations, write then verify; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param value Byte value to write.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the byte verified.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_direct_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Direct mode: read a single CV bit.
         *
         * @details Verifies the bit for 1, then for 0 if that is not acknowledged; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position within the CV byte (0-7).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit read (1 or 0).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t); not used by this task, may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_direct_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Direct mode: write a single CV bit, then verify it.
         *
         * @details Two operations, write-bit then verify-bit; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position within the CV byte (0-7).
         * @param bit_value Bit value to write (true = 1, false = 0).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit value verified (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_direct_write_bit(
                uint16_t cv_number,
                uint8_t bit_position,
                bool bit_value,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

    // =========================================================================
    // Paged mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

        /**
         * @brief Paged mode: read a CV byte by scanning verifies 0-255 until acknowledged.
         *
         * @details Starts the scan and returns at once; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the CV byte found.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_paged_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Paged mode: write a CV byte, then verify it.
         *
         * @details Two operations, write then targeted verify; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param value Byte value to write.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the byte verified.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_paged_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Paged mode: read a single CV bit.
         *
         * @details Reads the full byte by scanning and extracts the bit; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position within the CV byte (0-7).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit read (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_paged_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Paged mode: write a single CV bit, then verify it.
         *
         * @details Read-modify-write of the whole byte; the result arrives through on_complete.
         *
         * @param cv_number CV number (1-1024).
         * @param bit_position Bit position within the CV byte (0-7).
         * @param bit_value Bit value to write (true = 1, false = 0).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit value verified (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_paged_write_bit(
                uint16_t cv_number,
                uint8_t bit_position,
                bool bit_value,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

    // =========================================================================
    // Register mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

        /**
         * @brief Register mode: read a CV byte by scanning register verifies 0-255 until acknowledged.
         *
         * @details The decoder type is given per call because it selects the CV-to-register map; the result arrives through on_complete.
         *
         * @param cv_number CV number, mapped to a physical register for decoder_type.
         * @param decoder_type Mobile or accessory, per @ref dcc_decoder_type_enum; selects the CV-to-register map.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the CV byte found.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_read_cv(
                uint16_t cv_number,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Register mode: write a CV byte, then verify it.
         *
         * @details Two operations, write then verify; the result arrives through on_complete.
         *
         * @param cv_number CV number, mapped to a physical register for decoder_type.
         * @param value Byte value to write.
         * @param decoder_type Mobile or accessory, per @ref dcc_decoder_type_enum; selects the CV-to-register map.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the byte verified.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_write_cv(
                uint16_t cv_number,
                uint8_t value,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Register mode: read a single CV bit.
         *
         * @details Reads the full byte by scanning and extracts the bit; the result arrives through on_complete.
         *
         * @param cv_number CV number, mapped to a physical register for decoder_type.
         * @param bit_position Bit position within the CV byte (0-7).
         * @param decoder_type Mobile or accessory, per @ref dcc_decoder_type_enum; selects the CV-to-register map.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit read (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_read_bit(
                uint16_t cv_number,
                uint8_t bit_position,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Register mode: write a single CV bit, then verify it.
         *
         * @details Read-modify-write of the whole byte; the result arrives through on_complete.
         *
         * @param cv_number CV number, mapped to a physical register for decoder_type.
         * @param bit_position Bit position within the CV byte (0-7).
         * @param bit_value Bit value to write (true = 1, false = 0).
         * @param decoder_type Mobile or accessory, per @ref dcc_decoder_type_enum; selects the CV-to-register map.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit value verified (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_write_bit(
                uint16_t cv_number,
                uint8_t bit_position,
                bool bit_value,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Register mode: decoder factory reset (write 8 to register 8).
         *
         * @details Applies to both mobile and accessory decoders. The acknowledgement is optional per S-9.2.3, so the result may be NO_ACK.
         *
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); the acknowledgement is optional, so the result may be NO_ACK.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete);

        /**
         * @brief Register mode: verify a single register value (one verify operation).
         *
         * @details The result arrives through on_complete: SUCCESS if the value was acknowledged, VERIFY_FAIL otherwise.
         *
         * @param cv_number CV number, mapped to a physical register for decoder_type.
         * @param value Expected byte value.
         * @param decoder_type Mobile or accessory, per @ref dcc_decoder_type_enum; selects the CV-to-register map.
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); SUCCESS if acknowledged, VERIFY_FAIL otherwise.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
         */
    extern bool DccApplicationCommandStationServiceTrack_register_verify_value(
                uint16_t cv_number,
                uint8_t value,
                dcc_decoder_type_enum decoder_type,
                dcc_service_mode_task_on_complete_callback_t on_complete,
                dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

    // =========================================================================
    // Address-only mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

        /**
         * @brief Address-only mode: read CV#1 (short address) by scanning verifies 0-127 until acknowledged.
         *
         * @details Starts the scan and returns at once; the result arrives through on_complete.
         *
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the address found (0-127).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
         */
    extern bool DccApplicationCommandStationServiceTrack_address_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Address-only mode: write CV#1 (short address), then verify it.
         *
         * @details Two operations, write then verify; the result arrives through on_complete.
         *
         * @param address Short address to write (1-127).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the address verified.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or address is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_address_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Address-only mode: verify CV#1 against a value (one verify operation).
         *
         * @details The result arrives through on_complete: SUCCESS if the address was acknowledged, VERIFY_FAIL otherwise.
         *
         * @param address Expected short address (1-127).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); SUCCESS if acknowledged, VERIFY_FAIL otherwise.
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or address is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_address_verify(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Address-only mode: read a single bit of CV#1.
         *
         * @details Reads the full byte by scanning and extracts the bit; the result arrives through on_complete.
         *
         * @param bit_position Bit position within CV#1 (0-6; bit 7 of a short address is always 0).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit read (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or bit_position is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_address_read_bit(uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /**
         * @brief Address-only mode: write a single bit of CV#1, then verify it.
         *
         * @details Read-modify-write of the whole byte; the result arrives through on_complete.
         *
         * @param bit_position Bit position within CV#1 (0-6; bit 7 of a short address is always 0).
         * @param bit_value Bit value to write (true = 1, false = 0).
         * @param on_complete Completion callback (@ref dcc_service_mode_task_on_complete_callback_t); value = the bit value verified (0 or 1).
         * @param on_progress Progress callback (@ref dcc_service_mode_task_on_progress_callback_t), called after each step; may be NULL.
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or bit_position is out of range.
         */
    extern bool DccApplicationCommandStationServiceTrack_address_write_bit(uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

    // =========================================================================
    // Decoder mode detection (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

        /**
         * @brief Probe the decoder for every supported service mode.
         *
         * @details Runs one detection probe per compiled-in mode and reports the set as a bitmask through on_detect.
         *
         * @param on_detect Detection callback (@ref dcc_service_mode_task_on_detect_callback_t); supported_modes = bitmask of DCC_SERVICE_MODE_SUPPORTED_* flags (0 = none detected).
         * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
         */
    extern bool DccApplicationCommandStationServiceTrack_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_COMMAND_STATION_SERVICE_TRACK__ */
