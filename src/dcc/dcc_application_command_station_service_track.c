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
 * @date 13 Apr 2026
 */

#include "dcc_application_command_station_service_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_command_station_service_track_t *_interface = (void *)0;

void DccApplicationCommandStationServiceTrack_initialize(const interface_dcc_application_command_station_service_track_t *interface) {

    _interface = interface;

}

/* =========================================================================
 * Power control
 * ========================================================================= */

void DccApplicationCommandStationServiceTrack_power_on(void) {

    if (!_interface) {

        return;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

}

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

bool DccApplicationCommandStationServiceTrack_enter_service_mode(void) {

    if (!_interface) {

        return false;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

    return _interface->enter_service_mode();

}

void DccApplicationCommandStationServiceTrack_exit_service_mode(void) {

    if (!_interface) {

        return;

    }

    _interface->exit_service_mode();
    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

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

bool DccApplicationCommandStationServiceTrack_direct_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_read_cv) {

        return false;

    }

    return _interface->direct_read_cv(cv, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_direct_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_write_cv) {

        return false;

    }

    return _interface->direct_write_cv(cv, value, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_direct_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_read_bit) {

        return false;

    }

    return _interface->direct_read_bit(cv, bit, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_direct_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->direct_write_bit) {

        return false;

    }

    return _interface->direct_write_bit(cv, bit, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

/* =========================================================================
 * Paged mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED

bool DccApplicationCommandStationServiceTrack_paged_read_cv(uint16_t cv, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_read_cv) {

        return false;

    }

    return _interface->paged_read_cv(cv, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_paged_write_cv(uint16_t cv, uint8_t value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_write_cv) {

        return false;

    }

    return _interface->paged_write_cv(cv, value, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_paged_read_bit(uint16_t cv, uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_read_bit) {

        return false;

    }

    return _interface->paged_read_bit(cv, bit, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_paged_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->paged_write_bit) {

        return false;

    }

    return _interface->paged_write_bit(cv, bit, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

/* =========================================================================
 * Register mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER

bool DccApplicationCommandStationServiceTrack_register_read_cv(uint16_t cv, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_read_cv) {

        return false;

    }

    return _interface->register_read_cv(cv, decoder_type, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_register_write_cv(uint16_t cv, uint8_t value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_write_cv) {

        return false;

    }

    return _interface->register_write_cv(cv, value, decoder_type, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_register_read_bit(uint16_t cv, uint8_t bit, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_read_bit) {

        return false;

    }

    return _interface->register_read_bit(cv, bit, decoder_type, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_register_write_bit(uint16_t cv, uint8_t bit, bool bit_value, dcc_decoder_type_enum decoder_type, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->register_write_bit) {

        return false;

    }

    return _interface->register_write_bit(cv, bit, bit_value, decoder_type, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_register_factory_reset(dcc_service_mode_task_on_complete_callback_t on_complete) {

    if (!_interface || !_interface->register_factory_reset) {

        return false;

    }

    return _interface->register_factory_reset(on_complete);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

/* =========================================================================
 * Address-only mode programming (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS

bool DccApplicationCommandStationServiceTrack_address_read(dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_read) {

        return false;

    }

    return _interface->address_read(on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_address_write(uint8_t address, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_write) {

        return false;

    }

    return _interface->address_write(address, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_address_read_bit(uint8_t bit, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_read_bit) {

        return false;

    }

    return _interface->address_read_bit(bit, on_complete, on_progress);

}

bool DccApplicationCommandStationServiceTrack_address_write_bit(uint8_t bit, bool bit_value, dcc_service_mode_task_on_complete_callback_t on_complete, dcc_service_mode_task_on_progress_callback_t on_progress) {

    if (!_interface || !_interface->address_write_bit) {

        return false;

    }

    return _interface->address_write_bit(bit, bit_value, on_complete, on_progress);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

/* =========================================================================
 * Decoder mode detection (task layer)
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT

bool DccApplicationCommandStationServiceTrack_detect_mode(dcc_service_mode_task_on_detect_callback_t on_detect) {

    if (!_interface || !_interface->detect_mode) {

        return false;

    }

    return _interface->detect_mode(on_detect);

}

#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#endif /* DCC_COMPILE_COMMAND_STATION */
