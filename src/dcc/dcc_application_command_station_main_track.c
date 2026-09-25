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
 * @details Thin facade over the interface struct wired by dcc_config.c: power
 * sequencing of the H-bridge, timer and encoder, and forwarding of packet
 * inserts and removals to the main track scheduler. Every function null-guards
 * the interface pointer.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_command_station_main_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_command_station_main_track_t *_interface = (void *)0;

    /**
     * @brief Initialize the command station main track application module.
     *
     * @details Stores the interface pointer. Every other function in this
     * module is a no-op (or returns false) until this has been called.
     *
     * @verbatim
     * @param interface Pointer to populated interface_dcc_application_command_station_main_track_t (wired by dcc_config.c).
     * @endverbatim
     */
void DccApplicationCommandStationMainTrack_initialize(const interface_dcc_application_command_station_main_track_t *interface) {

    _interface = interface;

}

    /**
     * @brief Enable main track power output and start DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if the module has not been initialized
     * -# Apply track power (track_power_set(true)) first
     * -# Start the timer at DCC_ONE_BIT_HALF_PERIOD_US
     * -# Start the bit encoder
     */
void DccApplicationCommandStationMainTrack_power_on(void) {

    if (!_interface) {

        return;

    }

    _interface->track_power_set(true);
    _interface->timer_start(DCC_ONE_BIT_HALF_PERIOD_US);
    _interface->encoder_start();

}

    /**
     * @brief Disable main track power output and stop DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if the module has not been initialized
     * -# Stop the bit encoder
     * -# Stop the timer
     * -# Remove track power (track_power_set(false)) last
     */
void DccApplicationCommandStationMainTrack_power_off(void) {

    if (!_interface) {

        return;

    }

    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

    /**
     * @brief Send a one-shot packet on the main track (not auto-refreshed).
     *
     * @details Algorithm:
     * -# Return false if the module has not been initialized
     * -# Hand the packet to the scheduler with auto_refresh = false
     * -# The scheduler refuses a one-shot whose repeat_count is 0 (it would
     *    never be sent) and refuses when no slot is free
     * -# An active slot keyed on (address, tag) -- a refresh slot included --
     *    is overwritten and becomes a one-shot
     *
     * @verbatim
     * @param packet Pointer to the dcc_packet_t to schedule.
     * @param address DCC address used as the primary duplicate-combining key.
     * @param tag Sub-key for duplicate combining (dcc_tag_enum).
     * @param priority Packet priority level (dcc_priority_enum).
     * @endverbatim
     *
     * @return true if the packet was scheduled; false if the scheduler is full, the one-shot has repeat_count 0, or the module is uninitialized.
     */
bool DccApplicationCommandStationMainTrack_send_packet(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority) {

    if (!_interface) {

        return false;

    }

    return _interface->scheduler_insert(packet, address, tag, priority, false);

}

    /**
     * @brief Add a packet to the main track auto-refresh cycle.
     *
     * @details Algorithm:
     * -# Return false if the module has not been initialized
     * -# Hand the packet to the scheduler with auto_refresh = true; the
     *    scheduler refuses only when no slot is free (repeat_count is ignored
     *    for refresh slots)
     * -# An active slot keyed on (address, tag) -- a pending one-shot included
     *    -- is overwritten and becomes a refresh slot
     *
     * @verbatim
     * @param packet Pointer to the dcc_packet_t to schedule.
     * @param address DCC address used as the primary duplicate-combining key.
     * @param tag Sub-key for duplicate combining (dcc_tag_enum).
     * @param priority Packet priority level (dcc_priority_enum).
     * @endverbatim
     *
     * @return true if the packet was scheduled; false if the scheduler is full or the module is uninitialized.
     */
bool DccApplicationCommandStationMainTrack_add_to_auto_refresh(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority) {

    if (!_interface) {

        return false;

    }

    return _interface->scheduler_insert(packet, address, tag, priority, true);

}

    /**
     * @brief Remove every active scheduler slot for an address.
     *
     * @details Forwards to scheduler_remove_address, which deactivates all
     * slots keyed on the address -- one-shots that have not finished sending
     * included, not only auto-refresh slots.
     *
     * @verbatim
     * @param address The DCC address whose slots are purged.
     * @endverbatim
     */
void DccApplicationCommandStationMainTrack_remove_from_auto_refresh(dcc_address_t address) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_remove_address(address);

}

    /**
     * @brief Clear every scheduler slot, auto-refresh and pending one-shots alike.
     *
     * @details Forwards to scheduler_clear. Nothing is left for the scheduler
     * to send until a new packet is inserted.
     */
void DccApplicationCommandStationMainTrack_remove_all_auto_refresh(void) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_clear();

}

#endif /* DCC_COMPILE_COMMAND_STATION */
