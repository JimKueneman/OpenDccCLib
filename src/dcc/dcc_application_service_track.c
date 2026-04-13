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
 * @author Jim Kueneman
 * @date 08 Apr 2026
 */

#include "dcc_application_service_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_service_track_t *_interface = (void *)0;

void DccApplicationServiceTrack_initialize(const interface_dcc_application_service_track_t *interface) {

    _interface = interface;

}

/* =========================================================================
 * Power control
 * ========================================================================= */

void DccApplicationServiceTrack_power_on(void) {

    if (!_interface) {

        return;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

}

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

bool DccApplicationServiceTrack_enter(void) {

    if (!_interface) {

        return false;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

    return _interface->enter_service_mode();

}

void DccApplicationServiceTrack_exit(void) {

    if (!_interface) {

        return;

    }

    _interface->exit_service_mode();
    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

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

bool DccApplicationServiceTrack_direct_write_byte(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_write_byte(cv_number, value);

}

bool DccApplicationServiceTrack_direct_verify_byte(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_verify_byte(cv_number, value);

}

bool DccApplicationServiceTrack_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    if (!_interface) {

        return false;

    }

    return _interface->direct_write_bit(cv_number, bit_position, bit_value);

}

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

bool DccApplicationServiceTrack_paged_write(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->paged_write(cv_number, value);

}

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

bool DccApplicationServiceTrack_register_write(uint8_t register_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    return _interface->register_write(register_number, value);

}

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

bool DccApplicationServiceTrack_address_write(uint8_t address) {

    if (!_interface) {

        return false;

    }

    return _interface->address_write(address);

}

bool DccApplicationServiceTrack_address_verify(uint8_t address) {

    if (!_interface) {

        return false;

    }

    return _interface->address_verify(address);

}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#endif /* DCC_COMPILE_COMMAND_STATION */
