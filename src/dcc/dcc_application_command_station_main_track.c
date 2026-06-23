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
 * @file dcc_application_command_station_main_track.c
 * @brief Application-layer implementation for command station main track operations.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "dcc_application_command_station_main_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_command_station_main_track_t *_interface = (void *)0;

void DccApplicationCommandStationMainTrack_initialize(const interface_dcc_application_command_station_main_track_t *interface) {

    _interface = interface;

}

void DccApplicationCommandStationMainTrack_power_on(void) {

    if (!_interface) {

        return;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

}

void DccApplicationCommandStationMainTrack_power_off(void) {

    if (!_interface) {

        return;

    }

    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

void DccApplicationCommandStationMainTrack_set_railcom_enabled(bool enabled) {

    if (!_interface) {

        return;

    }

    _interface->set_railcom_enabled(enabled);

}

bool DccApplicationCommandStationMainTrack_is_railcom_enabled(void) {

    if (!_interface) {

        return false;

    }

    return _interface->is_railcom_enabled();

}

bool DccApplicationCommandStationMainTrack_send_packet(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority) {

    if (!_interface) {

        return false;

    }

    return _interface->scheduler_insert(packet, address, tag, priority, false);

}

bool DccApplicationCommandStationMainTrack_add_to_auto_refresh(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority) {

    if (!_interface) {

        return false;

    }

    return _interface->scheduler_insert(packet, address, tag, priority, true);

}

void DccApplicationCommandStationMainTrack_remove_from_auto_refresh(dcc_address_t address) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_remove_address(address);

}

void DccApplicationCommandStationMainTrack_remove_all_auto_refresh(void) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_clear();

}

#endif /* DCC_COMPILE_COMMAND_STATION */
