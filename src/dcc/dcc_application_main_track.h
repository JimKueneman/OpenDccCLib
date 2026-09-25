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
 * @file dcc_application_main_track.h
 * @brief Application-layer API for main track operations.
 *
 * @details Legacy application layer for the main track DCC output: power control,
 * packet scheduling, and slot management, forwarded through an interface struct
 * that the application (or test) populates. This module is still compiled and
 * unit-tested but is NOT wired by dcc_config.c; the current command-station API
 * is dcc_application_command_station_main_track. Every call is a no-op (or
 * returns false) until DccApplicationMainTrack_initialize has been called.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_APPLICATION_MAIN_TRACK__
#define __DCC_APPLICATION_MAIN_TRACK__

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

        /** @brief Insert a packet into the scheduler. */
    bool (*scheduler_insert)(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh);

        /** @brief Remove all scheduler slots for a given address. */
    void (*scheduler_remove_address)(dcc_address_t address);

        /** @brief Clear all scheduler slots. */
    void (*scheduler_clear)(void);

} interface_dcc_application_main_track_t;

        /**
         * @brief Initialize the main track application module.
         *
         * @details Stores the interface pointer; nothing is powered or scheduled.
         *
         * @param interface Pointer to a populated @ref interface_dcc_application_main_track_t; must remain valid while the module is used.
         */
    extern void DccApplicationMainTrack_initialize(const interface_dcc_application_main_track_t *interface);

        /**
         * @brief Enable main track power output and start DCC signal generation.
         *
         * @details Calls track_power_set(true), timer_start with the one-bit half period
         * (DCC_ONE_BIT_HALF_PERIOD_US) and encoder_start, in that order. No-op before
         * initialization.
         */
    extern void DccApplicationMainTrack_power_on(void);

        /**
         * @brief Disable main track power output and stop DCC signal generation.
         *
         * @details Calls encoder_stop, timer_stop and track_power_set(false), in that
         * order (the reverse of power-on). No-op before initialization.
         */
    extern void DccApplicationMainTrack_power_off(void);

        /**
         * @brief Insert a packet into the main track scheduler.
         *
         * @details Forwards to the scheduler_insert hook. The address and tag together
         * form the scheduler's duplicate-combining key.
         *
         * @param packet The @ref dcc_packet_t to schedule.
         * @param address @ref dcc_address_t used as the duplicate-combining key.
         * @param tag @ref dcc_tag_enum sub-key for duplicate combining (e.g., function group).
         * @param priority @ref dcc_priority_enum packet priority level.
         * @param auto_refresh true = keep in refresh cycle indefinitely.
         *
         * @return true if the packet was scheduled; false if no free slot or the module is not initialized.
         */
    extern bool DccApplicationMainTrack_insert(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh);

        /**
         * @brief Remove all scheduler slots for a given address.
         *
         * @details Forwards to the scheduler_remove_address hook. No-op before initialization.
         *
         * @param address The @ref dcc_address_t to purge.
         */
    extern void DccApplicationMainTrack_remove_address(dcc_address_t address);

        /**
         * @brief Clear all active scheduler slots.
         *
         * @details Forwards to the scheduler_clear hook. No-op before initialization.
         */
    extern void DccApplicationMainTrack_clear(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_MAIN_TRACK__ */
