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
 * @file dcc_application_service_track.c
 * @brief Application-layer implementation for service track operations.
 *
 * @details Legacy module: still compiled and unit-tested, but not wired by
 * dcc_config.c (see dcc_application_command_station_service_track for the current API).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_service_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Interface pointer supplied to DccApplicationServiceTrack_initialize; NULL until then. */
static const interface_dcc_application_service_track_t *_interface = (void *)0;

    /**
     * @brief Initialize the service track application module.
     *
     * @details Stores the interface pointer; nothing is powered and service mode is not entered.
     *
     * @verbatim
     * @param interface Pointer to a populated interface_dcc_application_service_track_t.
     * @endverbatim
     */
void DccApplicationServiceTrack_initialize(const interface_dcc_application_service_track_t *interface) {

    _interface = interface;

}

/* =========================================================================
 * Power control
 * ========================================================================= */

    /**
     * @brief Enable service track power output and start DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if not initialized.
     * -# track_power_set(true).
     * -# timer_start(DCC_ONE_BIT_HALF_PERIOD_US).
     * -# encoder_start().
     */
void DccApplicationServiceTrack_power_on(void) {

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
     * -# Return if not initialized.
     * -# encoder_stop().
     * -# timer_stop().
     * -# track_power_set(false).
     */
void DccApplicationServiceTrack_power_off(void) {

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
     * -# Return false if not initialized.
     * -# Power the track: track_power_set(true), timer_start(DCC_ONE_BIT_HALF_PERIOD_US), encoder_start().
     * -# Return the result of the enter_service_mode hook; the track stays powered
     *    even when the hook refuses.
     *
     * @return true if service mode was activated; false if the hook refused or the module is not initialized.
     */
bool DccApplicationServiceTrack_enter(void) {

    if (!_interface) {

        return false;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

    return _interface->enter_service_mode();

}

    /**
     * @brief Exit service mode on the service track.
     *
     * @details Algorithm:
     * -# Return if not initialized.
     * -# exit_service_mode().
     * -# encoder_stop(), timer_stop(), track_power_set(false).
     */
void DccApplicationServiceTrack_exit(void) {

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
     * @details Returns false when uninitialized, otherwise forwards to the
     * is_service_mode_active hook.
     *
     * @return true if service mode is active; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_is_active(void) {

    if (!_interface) {

        return false;

    }

    return _interface->is_service_mode_active();

}

/* =========================================================================
 * Direct mode programming
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /**
     * @brief Direct mode: write a CV byte.
     *
     * @details Returns false when uninitialized, otherwise forwards to the direct_write_byte hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param value Byte value to write.
     * @endverbatim
     *
     * @return true if the hook reports success; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_direct_write_byte(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_write_byte(cv_number, value);

}

    /**
     * @brief Direct mode: verify a CV byte.
     *
     * @details Returns false when uninitialized, otherwise forwards to the direct_verify_byte hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param value Expected byte value.
     * @endverbatim
     *
     * @return true if the hook reports a match; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_direct_verify_byte(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_verify_byte(cv_number, value);

}

    /**
     * @brief Direct mode: write a single CV bit.
     *
     * @details Returns false when uninitialized, otherwise forwards to the direct_write_bit hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Bit value to write.
     * @endverbatim
     *
     * @return true if the hook reports success; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_write_bit(cv_number, bit_position, bit_value);

}

    /**
     * @brief Direct mode: verify a single CV bit.
     *
     * @details Returns false when uninitialized, otherwise forwards to the direct_verify_bit hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param bit_position Bit position within the CV (0-7).
     * @param bit_value Expected bit value.
     * @endverbatim
     *
     * @return true if the hook reports a match; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_verify_bit(cv_number, bit_position, bit_value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

/* =========================================================================
 * Paged mode programming
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /**
     * @brief Paged mode: write a CV.
     *
     * @details Returns false when uninitialized, otherwise forwards to the paged_write hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param value Byte value to write.
     * @endverbatim
     *
     * @return true if the hook reports success; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_paged_write(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->paged_write(cv_number, value);

}

    /**
     * @brief Paged mode: verify a CV.
     *
     * @details Returns false when uninitialized, otherwise forwards to the paged_verify hook.
     *
     * @verbatim
     * @param cv_number CV number (1-based).
     * @param value Expected byte value.
     * @endverbatim
     *
     * @return true if the hook reports a match; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_paged_verify(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->paged_verify(cv_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

/* =========================================================================
 * Register mode programming
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /**
     * @brief Register mode: write a register.
     *
     * @details Returns false when uninitialized, otherwise forwards to the register_write hook.
     *
     * @verbatim
     * @param register_number Register number (S-9.2.3 register mode).
     * @param value Byte value to write.
     * @endverbatim
     *
     * @return true if the hook reports success; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_register_write(uint8_t register_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->register_write(register_number, value);

}

    /**
     * @brief Register mode: verify a register.
     *
     * @details Returns false when uninitialized, otherwise forwards to the register_verify hook.
     *
     * @verbatim
     * @param register_number Register number (S-9.2.3 register mode).
     * @param value Expected byte value.
     * @endverbatim
     *
     * @return true if the hook reports a match; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_register_verify(uint8_t register_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->register_verify(register_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

/* =========================================================================
 * Address-only mode programming
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /**
     * @brief Address-only mode: write the short address (CV 1).
     *
     * @details Returns false when uninitialized, otherwise forwards to the address_write hook.
     *
     * @verbatim
     * @param address Short address to program into CV 1.
     * @endverbatim
     *
     * @return true if the hook reports success; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_address_write(uint8_t address) {

    if (!_interface) {

        return false;

    }

    return _interface->address_write(address);

}

    /**
     * @brief Address-only mode: verify the short address (CV 1).
     *
     * @details Returns false when uninitialized, otherwise forwards to the address_verify hook.
     *
     * @verbatim
     * @param address Expected short address in CV 1.
     * @endverbatim
     *
     * @return true if the hook reports a match; false otherwise or when the module is not initialized.
     */
bool DccApplicationServiceTrack_address_verify(uint8_t address) {

    if (!_interface) {

        return false;

    }

    return _interface->address_verify(address);

}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#endif /* DCC_COMPILE_COMMAND_STATION */
