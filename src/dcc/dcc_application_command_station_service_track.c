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
 * @file dcc_application_command_station_service_track.c
 * @brief Application-layer implementation for command station service track operations.
 *
 * @details The programming surface delegates to the high-level task layer through
 * the interface struct wired by dcc_config.c. The task modules sequence and verify
 * the underlying primitive packet operations; this layer only forwards the call and
 * null-guards the interface.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_command_station_service_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_command_station_service_track_t *_interface = (void *)0;

    /**
     * @brief Initialize the command station service track application module.
     *
     * @details Stores the interface pointer. Every other function in this
     * module is a no-op (or returns false) until this has been called.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_application_command_station_service_track_t struct (wired by dcc_config.c).
     * @endverbatim
     */
void DccApplicationCommandStationServiceTrack_initialize(const interface_dcc_application_command_station_service_track_t *interface) {

    _interface = interface;

}

/* =========================================================================
 * Power control
 * ========================================================================= */

    /**
     * @brief Enable service track power output and start DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if the module has not been initialized
     * -# Apply track power (track_power_set(true)) first
     * -# Start the timer at DCC_ONE_BIT_HALF_PERIOD_US
     * -# Start the bit encoder
     */
void DccApplicationCommandStationServiceTrack_power_on(void) {

    if (!_interface) {

        return;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

}

    /**
     * @brief Disable service track power output and stop DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if the module has not been initialized
     * -# Stop the bit encoder
     * -# Stop the timer
     * -# Remove track power (track_power_set(false)) last
     */
void DccApplicationCommandStationServiceTrack_power_off(void) {

    if (!_interface) {

        return;

    }

    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

/* =========================================================================
 * Service mode entry/exit
 * ========================================================================= */

    /**
     * @brief Enter service mode on the service track.
     *
     * @details Algorithm:
     * -# Return false if the module has not been initialized
     * -# Apply track power (track_power_set(true)) first
     * -# Start the timer at DCC_ONE_BIT_HALF_PERIOD_US and start the bit encoder
     * -# Enter the service-mode core and return its verdict
     *
     * @return true if service mode was activated; false if the module is uninitialized or the core refused entry.
     */
bool DccApplicationCommandStationServiceTrack_enter_service_mode(void) {

    if (!_interface) {

        return false;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

    return _interface->enter_service_mode();

}

    /**
     * @brief Exit service mode, stop the signal and remove track power.
     *
     * @details Algorithm:
     * -# Return if the module has not been initialized
     * -# Leave the service-mode core
     * -# Stop the bit encoder, then the timer
     * -# Remove track power (track_power_set(false)) last
     */
void DccApplicationCommandStationServiceTrack_exit_service_mode(void) {

    if (!_interface) {

        return;

    }

    _interface->exit_service_mode();
    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

    /**
     * @brief Check if service mode is currently active.
     *
     * @details Forwards to the service-mode core through the interface.
     *
     * @return true if service mode is active; false otherwise, including when the module has not been initialized.
     */
bool DccApplicationCommandStationServiceTrack_is_service_mode_active(void) {

    if (!_interface) {

        return false;

    }

    return _interface->is_service_mode_active();

}

/* =========================================================================
 * Direct mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT

    /**
     * @brief Direct mode: read a CV byte (8 bit-verifies, then a confirming byte verify).
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param on_complete Completion callback; value = the CV byte read.
     * @param on_progress Progress callback, called after each of the 9 steps; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
     */
bool DccApplicationCommandStationServiceTrack_direct_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_read_cv) {

        return false;

    }

    return _interface->direct_read_cv(cv_number, on_complete, on_progress);

}

    /**
     * @brief Direct mode: write a CV byte, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param value Byte value to write.
     * @param on_complete Completion callback; value = the byte verified.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
     */
bool DccApplicationCommandStationServiceTrack_direct_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_write_cv) {

        return false;

    }

    return _interface->direct_write_cv(cv_number, value, on_complete, on_progress);

}

    /**
     * @brief Direct mode: read a single CV bit.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param on_complete Completion callback; value = the bit read (1 or 0).
     * @param on_progress Progress callback; not used by this task, may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_direct_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_read_bit) {

        return false;

    }

    return _interface->direct_read_bit(cv_number, bit_position, on_complete, on_progress);

}

    /**
     * @brief Direct mode: write a single CV bit, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Bit value to write (true = 1, false = 0).
     * @param on_complete Completion callback; value = the bit value verified (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_write_bit) {

        return false;

    }

    return _interface->direct_write_bit(cv_number, bit_position, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

/* =========================================================================
 * Paged mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

    /**
     * @brief Paged mode: read a CV byte by scanning verifies 0-255 until acknowledged.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param on_complete Completion callback; value = the CV byte found.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
     */
bool DccApplicationCommandStationServiceTrack_paged_read_cv(uint16_t cv_number, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_read_cv) {

        return false;

    }

    return _interface->paged_read_cv(cv_number, on_complete, on_progress);

}

    /**
     * @brief Paged mode: write a CV byte, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param value Byte value to write.
     * @param on_complete Completion callback; value = the byte verified.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or cv_number is out of range.
     */
bool DccApplicationCommandStationServiceTrack_paged_write_cv(uint16_t cv_number, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_write_cv) {

        return false;

    }

    return _interface->paged_write_cv(cv_number, value, on_complete, on_progress);

}

    /**
     * @brief Paged mode: read a single CV bit.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param on_complete Completion callback; value = the bit read (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_paged_read_bit(uint16_t cv_number, uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_read_bit) {

        return false;

    }

    return _interface->paged_read_bit(cv_number, bit_position, on_complete, on_progress);

}

    /**
     * @brief Paged mode: write a single CV bit, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number (1-1024).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Bit value to write (true = 1, false = 0).
     * @param on_complete Completion callback; value = the bit value verified (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_paged_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_write_bit) {

        return false;

    }

    return _interface->paged_write_bit(cv_number, bit_position, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

/* =========================================================================
 * Register mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

    /**
     * @brief Register mode: read a CV byte by scanning register verifies 0-255 until acknowledged.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number, mapped to a physical register for decoder_type.
     * @param decoder_type Mobile or accessory (dcc_decoder_type_enum); selects the CV-to-register map.
     * @param on_complete Completion callback; value = the CV byte found.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
     */
bool DccApplicationCommandStationServiceTrack_register_read_cv(uint16_t cv_number, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_read_cv) {

        return false;

    }

    return _interface->register_read_cv(cv_number, decoder_type, on_complete, on_progress);

}

    /**
     * @brief Register mode: write a CV byte, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number, mapped to a physical register for decoder_type.
     * @param value Byte value to write.
     * @param decoder_type Mobile or accessory (dcc_decoder_type_enum); selects the CV-to-register map.
     * @param on_complete Completion callback; value = the byte verified.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
     */
bool DccApplicationCommandStationServiceTrack_register_write_cv(
            uint16_t cv_number,
            uint8_t value,
            dcc_decoder_type_enum decoder_type,
            dcc_service_mode_task_on_complete_callback_t on_complete,
            dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_write_cv) {

        return false;

    }

    return _interface->register_write_cv(cv_number, value, decoder_type, on_complete, on_progress);

}

    /**
     * @brief Register mode: read a single CV bit.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number, mapped to a physical register for decoder_type.
     * @param bit_position Bit position within the CV byte (0-7).
     * @param decoder_type Mobile or accessory (dcc_decoder_type_enum); selects the CV-to-register map.
     * @param on_complete Completion callback; value = the bit read (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_register_read_bit(
            uint16_t cv_number,
            uint8_t bit_position,
            dcc_decoder_type_enum decoder_type,
            dcc_service_mode_task_on_complete_callback_t on_complete,
            dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_read_bit) {

        return false;

    }

    return _interface->register_read_bit(cv_number, bit_position, decoder_type, on_complete, on_progress);

}

    /**
     * @brief Register mode: write a single CV bit, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number, mapped to a physical register for decoder_type.
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Bit value to write (true = 1, false = 0).
     * @param decoder_type Mobile or accessory (dcc_decoder_type_enum); selects the CV-to-register map.
     * @param on_complete Completion callback; value = the bit value verified (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or a parameter is out of range.
     */
bool DccApplicationCommandStationServiceTrack_register_write_bit(
            uint16_t cv_number,
            uint8_t bit_position,
            bool bit_value,
            dcc_decoder_type_enum decoder_type,
            dcc_service_mode_task_on_complete_callback_t on_complete,
            dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_write_bit) {

        return false;

    }

    return _interface->register_write_bit(cv_number, bit_position, bit_value, decoder_type, on_complete, on_progress);

}

    /**
     * @brief Register mode: decoder factory reset (write 8 to register 8).
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param on_complete Completion callback; the acknowledgement is optional, so the result may be NO_ACK.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
     */
bool DccApplicationCommandStationServiceTrack_register_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete) {

    if (!_interface || !_interface->register_factory_reset) {

        return false;

    }

    return _interface->register_factory_reset(on_complete);

}

    /**
     * @brief Register mode: verify a single register value (one verify operation).
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param cv_number CV number, mapped to a physical register for decoder_type.
     * @param value Expected byte value.
     * @param decoder_type Mobile or accessory (dcc_decoder_type_enum); selects the CV-to-register map.
     * @param on_complete Completion callback; SUCCESS if acknowledged, VERIFY_FAIL otherwise.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or the CV is not accessible in register mode.
     */
bool DccApplicationCommandStationServiceTrack_register_verify_value(
            uint16_t cv_number,
            uint8_t value,
            dcc_decoder_type_enum decoder_type,
            dcc_service_mode_task_on_complete_callback_t on_complete,
            dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_verify_value) {

        return false;

    }

    return _interface->register_verify_value(cv_number, value, decoder_type, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

/* =========================================================================
 * Address-only mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

    /**
     * @brief Address-only mode: read CV#1 (short address) by scanning verifies 0-127 until acknowledged.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param on_complete Completion callback; value = the address found (0-127).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
     */
bool DccApplicationCommandStationServiceTrack_address_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_read) {

        return false;

    }

    return _interface->address_read(on_complete, on_progress);

}

    /**
     * @brief Address-only mode: write CV#1 (short address), then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param address Short address to write (1-127).
     * @param on_complete Completion callback; value = the address verified.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or address is out of range.
     */
bool DccApplicationCommandStationServiceTrack_address_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_write) {

        return false;

    }

    return _interface->address_write(address, on_complete, on_progress);

}

    /**
     * @brief Address-only mode: verify CV#1 against a value (one verify operation).
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param address Expected short address (1-127).
     * @param on_complete Completion callback; SUCCESS if acknowledged, VERIFY_FAIL otherwise.
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or address is out of range.
     */
bool DccApplicationCommandStationServiceTrack_address_verify(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_verify) {

        return false;

    }

    return _interface->address_verify(address, on_complete, on_progress);

}

    /**
     * @brief Address-only mode: read a single bit of CV#1.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param bit_position Bit position within CV#1 (0-6; bit 7 of a short address is always 0).
     * @param on_complete Completion callback; value = the bit read (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or bit_position is out of range.
     */
bool DccApplicationCommandStationServiceTrack_address_read_bit(uint8_t bit_position, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_read_bit) {

        return false;

    }

    return _interface->address_read_bit(bit_position, on_complete, on_progress);

}

    /**
     * @brief Address-only mode: write a single bit of CV#1, then verify it.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param bit_position Bit position within CV#1 (0-6; bit 7 of a short address is always 0).
     * @param bit_value Bit value to write (true = 1, false = 0).
     * @param on_complete Completion callback; value = the bit value verified (0 or 1).
     * @param on_progress Progress callback, called after each step; may be NULL.
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, the task layer is busy, or bit_position is out of range.
     */
bool DccApplicationCommandStationServiceTrack_address_write_bit(uint8_t bit_position, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_write_bit) {

        return false;

    }

    return _interface->address_write_bit(bit_position, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

/* =========================================================================
 * Decoder mode detection (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

    /**
     * @brief Probe the decoder for every supported service mode.
     *
     * @details Forwards to the task layer through the interface after null-guarding the
     * interface pointer and the task hook; the task sequences the primitive operations
     * and reports through the callbacks.
     *
     * @verbatim
     * @param on_detect Detection callback; supported_modes = bitmask of DCC_SERVICE_MODE_SUPPORTED_* flags (0 = none detected).
     * @endverbatim
     *
     * @return true if the task was started; false if the module is uninitialized, the task hook is not wired, or the task layer is busy.
     */
bool DccApplicationCommandStationServiceTrack_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect) {

    if (!_interface || !_interface->detect_mode) {

        return false;

    }

    return _interface->detect_mode(on_detect);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#endif /* DCC_COMPILE_COMMAND_STATION */
