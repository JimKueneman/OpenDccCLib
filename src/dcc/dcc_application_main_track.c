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
 * @file dcc_application_main_track.c
 * @brief Application-layer implementation for main track operations.
 *
 * @details Legacy module: still compiled and unit-tested, but not wired by
 * dcc_config.c (see dcc_application_command_station_main_track for the current API).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_main_track.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Interface pointer supplied to DccApplicationMainTrack_initialize; NULL until then. */
static const interface_dcc_application_main_track_t *_interface = (void *)0;

    /**
     * @brief Initialize the main track application module.
     *
     * @details Stores the interface pointer; nothing is powered or scheduled.
     *
     * @verbatim
     * @param interface Pointer to a populated interface_dcc_application_main_track_t.
     * @endverbatim
     */
void DccApplicationMainTrack_initialize(const interface_dcc_application_main_track_t *interface) {

    _interface = interface;

}

    /**
     * @brief Enable main track power output and start DCC signal generation.
     *
     * @details Algorithm:
     * -# Return if not initialized.
     * -# track_power_set(true).
     * -# timer_start(DCC_ONE_BIT_HALF_PERIOD_US).
     * -# encoder_start().
     */
void DccApplicationMainTrack_power_on(void) {

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
     * -# Return if not initialized.
     * -# encoder_stop().
     * -# timer_stop().
     * -# track_power_set(false).
     */
void DccApplicationMainTrack_power_off(void) {

    if (!_interface) {

        return;

    }

    _interface->encoder_stop();
    _interface->timer_stop();
    _interface->track_power_set(false);

}

    /**
     * @brief Insert a packet into the main track scheduler.
     *
     * @details Returns false when uninitialized, otherwise forwards every argument
     * to the scheduler_insert hook.
     *
     * @verbatim
     * @param packet       The packet to schedule.
     * @param address      Address used as the duplicate-combining key.
     * @param tag          Sub-key for duplicate combining.
     * @param priority     Packet priority level.
     * @param auto_refresh true = keep in refresh cycle indefinitely.
     * @endverbatim
     *
     * @return true if the packet was scheduled; false if no free slot or the module is not initialized.
     */
bool DccApplicationMainTrack_insert(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {

    if (!_interface) {

        return false;

    }

    return _interface->scheduler_insert(packet, address, tag, priority, auto_refresh);

}

    /**
     * @brief Remove all scheduler slots for a given address.
     *
     * @details Returns when uninitialized, otherwise forwards to the
     * scheduler_remove_address hook.
     *
     * @verbatim
     * @param address The address to purge.
     * @endverbatim
     */
void DccApplicationMainTrack_remove_address(dcc_address_t address) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_remove_address(address);

}

    /**
     * @brief Clear all active scheduler slots.
     *
     * @details Returns when uninitialized, otherwise forwards to the scheduler_clear hook.
     */
void DccApplicationMainTrack_clear(void) {

    if (!_interface) {

        return;

    }

    _interface->scheduler_clear();

}

#endif /* DCC_COMPILE_COMMAND_STATION */
