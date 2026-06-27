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
 * Size 2+: This module owns the command-station service-track application layer.
 * It delegates timer, encoder, and programming operations through an interface
 * struct wired at initialization time by dcc_config.c. Service mode must be
 * entered before any programming operation and exited afterward; the module
 * manages power sequencing automatically on enter/exit.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
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
    bool (*direct_read_cv)(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: write a CV byte (with verify). */
    bool (*direct_write_cv)(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: read a single CV bit. */
    bool (*direct_read_bit)(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Direct mode task: write a single CV bit (with verify). */
    bool (*direct_write_bit)(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

        /** @brief Paged mode task: read a CV byte. */
    bool (*paged_read_cv)(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: write a CV byte (with verify). */
    bool (*paged_write_cv)(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: read a single CV bit. */
    bool (*paged_read_bit)(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Paged mode task: write a single CV bit (with verify). */
    bool (*paged_write_bit)(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

        /** @brief Register mode task: read a CV byte. */
    bool (*register_read_cv)(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: write a CV byte (with verify). */
    bool (*register_write_cv)(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: read a single CV bit. */
    bool (*register_read_bit)(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: write a single CV bit (with verify). */
    bool (*register_write_bit)(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Register mode task: decoder factory reset (write 8 to register 8). */
    bool (*register_factory_reset)(dcc_service_mode_task_on_complete_callback_t on_complete);

        /** @brief Register mode task: verify a single register value. */
    bool (*register_verify_value)(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

        /** @brief Address-only mode task: read CV#1 (short address). */
    bool (*address_read)(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: write CV#1 (short address, with verify). */
    bool (*address_write)(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: verify CV#1 against a value. */
    bool (*address_verify)(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: read a single bit of CV#1. */
    bool (*address_read_bit)(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

        /** @brief Address-only mode task: write a single bit of CV#1 (with verify). */
    bool (*address_write_bit)(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

        /** @brief Detect mode task: probe the decoder for all supported service modes. */
    bool (*detect_mode)(dcc_service_mode_task_on_detect_callback_t on_detect);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

} interface_dcc_application_command_station_service_track_t;

    /**
     * @brief Initialize the command station service track application module.
     * @param interface Pointer to populated @ref interface_dcc_application_command_station_service_track_t struct (wired by dcc_config.c).
     */
extern void DccApplicationCommandStationServiceTrack_initialize(const interface_dcc_application_command_station_service_track_t *interface);

    // =========================================================================
    // Power control
    // =========================================================================

    /** @brief Enable service track power output and start DCC signal generation. */
extern void DccApplicationCommandStationServiceTrack_power_on(void);

    /** @brief Disable service track power output and stop DCC signal generation. */
extern void DccApplicationCommandStationServiceTrack_power_off(void);

    // =========================================================================
    // Service mode entry/exit
    // =========================================================================

    /**
     * @brief Enter service mode on the service track.
     * @return true if service mode was activated, false if busy.
     */
extern bool DccApplicationCommandStationServiceTrack_enter_service_mode(void);

    /** @brief Exit service mode on the service track. */
extern void DccApplicationCommandStationServiceTrack_exit_service_mode(void);

    /** @brief Check if service mode is currently active. */
extern bool DccApplicationCommandStationServiceTrack_is_service_mode_active(void);

    // =========================================================================
    // Direct mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

    /** @brief Direct mode: read a CV byte (8 bit-verifies). */
extern bool DccApplicationCommandStationServiceTrack_direct_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Direct mode: write a CV byte then verify. */
extern bool DccApplicationCommandStationServiceTrack_direct_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Direct mode: read a single CV bit. */
extern bool DccApplicationCommandStationServiceTrack_direct_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Direct mode: write a single CV bit then verify. */
extern bool DccApplicationCommandStationServiceTrack_direct_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

    // =========================================================================
    // Paged mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

    /** @brief Paged mode: read a CV byte (scan). */
extern bool DccApplicationCommandStationServiceTrack_paged_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Paged mode: write a CV byte then verify. */
extern bool DccApplicationCommandStationServiceTrack_paged_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Paged mode: read a single CV bit. */
extern bool DccApplicationCommandStationServiceTrack_paged_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Paged mode: write a single CV bit then verify. */
extern bool DccApplicationCommandStationServiceTrack_paged_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

    // =========================================================================
    // Register mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

    /** @brief Register mode: read a CV byte (scan). decoder_type per call. */
extern bool DccApplicationCommandStationServiceTrack_register_read_cv(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Register mode: write a CV byte then verify. decoder_type per call. */
extern bool DccApplicationCommandStationServiceTrack_register_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Register mode: read a single CV bit. decoder_type per call. */
extern bool DccApplicationCommandStationServiceTrack_register_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Register mode: write a single CV bit then verify. decoder_type per call. */
extern bool DccApplicationCommandStationServiceTrack_register_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Register mode: decoder factory reset (write 8 to register 8). */
extern bool DccApplicationCommandStationServiceTrack_register_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete);

    /** @brief Register mode: verify a single register value. */
extern bool DccApplicationCommandStationServiceTrack_register_verify_value(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

    // =========================================================================
    // Address-only mode programming (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

    /** @brief Address-only mode: read CV#1 (short address). */
extern bool DccApplicationCommandStationServiceTrack_address_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Address-only mode: write CV#1 (short address) then verify. */
extern bool DccApplicationCommandStationServiceTrack_address_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Address-only mode: verify CV#1 against a value. */
extern bool DccApplicationCommandStationServiceTrack_address_verify(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Address-only mode: read a single bit of CV#1. */
extern bool DccApplicationCommandStationServiceTrack_address_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

    /** @brief Address-only mode: write a single bit of CV#1 then verify. */
extern bool DccApplicationCommandStationServiceTrack_address_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

    // =========================================================================
    // Decoder mode detection (task layer)
    // =========================================================================

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

    /** @brief Probe the decoder for all supported service modes (bitmask via on_detect). */
extern bool DccApplicationCommandStationServiceTrack_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect);

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_COMMAND_STATION_SERVICE_TRACK__ */
