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
 * @file dcc_application_command_station_main_track.h
 * @brief Application-layer API for command station main track operations.
 *
 * @details Provides the user-facing functions for the main track DCC output:
 * power control, one-shot packet sending, and auto-refresh slot management.
 * Initialized by dcc_config.c during DccConfig_initialize(). Application code
 * includes this header instead of the internal module headers.
 *
 * Size 2+: This module owns the command-station main-track application layer.
 * It delegates timer, encoder, and scheduler operations through an interface
 * struct wired at initialization time by dcc_config.c. Two distinct insertion
 * paths are provided: send_packet (fire-and-forget) and add_to_auto_refresh
 * (kept in the refresh cycle until explicitly removed).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_APPLICATION_COMMAND_STATION_MAIN_TRACK__
#define __DCC_APPLICATION_COMMAND_STATION_MAIN_TRACK__

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

} interface_dcc_application_command_station_main_track_t;

        /**
         * @brief Initialize the command station main track application module.
         *
         * @details Every other function in this module is a no-op (or returns
         *  false) until this has been called.
         *
         * @param interface Pointer to populated @ref interface_dcc_application_command_station_main_track_t (wired by dcc_config.c).
         */
    extern void DccApplicationCommandStationMainTrack_initialize(const interface_dcc_application_command_station_main_track_t *interface);

        /**
         * @brief Enable main track power output and start DCC signal generation.
         *
         * @details Applies track power first, then starts the timer at the DCC
         *  one-bit half period and starts the bit encoder.
         */
    extern void DccApplicationCommandStationMainTrack_power_on(void);

        /**
         * @brief Disable main track power output and stop DCC signal generation.
         *
         * @details Stops the bit encoder and the timer, then removes track power
         *  last.
         */
    extern void DccApplicationCommandStationMainTrack_power_off(void);

        /**
         * @brief Send a one-shot packet on the main track (not auto-refreshed).
         *
         * @details The packet is transmitted repeat_count times and its slot is
         *  then released. If a slot keyed on the same address and tag is already
         *  active -- an auto-refresh slot included -- its packet data is replaced
         *  and that slot becomes a one-shot.
         *
         * @param packet Pointer to the @ref dcc_packet_t to schedule.
         * @param address @ref dcc_address_t used as the primary duplicate-combining key.
         * @param tag Sub-key for duplicate combining, per @ref dcc_tag_enum (e.g. function group).
         * @param priority Packet priority level, per @ref dcc_priority_enum.
         * @return true if the packet was scheduled; false if the scheduler has no
         *  free slot, if the packet's repeat_count is 0 (it would never be sent),
         *  or if the module has not been initialized.
         */
    extern bool DccApplicationCommandStationMainTrack_send_packet(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority);

        /**
         * @brief Add a packet to the main track auto-refresh cycle.
         *
         * @details The packet is resent in the refresh cycle until it is replaced
         *  or removed; repeat_count is ignored for refresh slots. If a slot keyed
         *  on the same address and tag is already active -- a pending one-shot
         *  included -- its packet data is replaced and that slot becomes a
         *  refresh slot.
         *
         * @param packet Pointer to the @ref dcc_packet_t to schedule.
         * @param address @ref dcc_address_t used as the primary duplicate-combining key.
         * @param tag Sub-key for duplicate combining, per @ref dcc_tag_enum (e.g. function group).
         * @param priority Packet priority level, per @ref dcc_priority_enum.
         * @return true if the packet was scheduled; false if the scheduler has no
         *  free slot or the module has not been initialized.
         */
    extern bool DccApplicationCommandStationMainTrack_add_to_auto_refresh(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority);

        /**
         * @brief Remove every active scheduler slot for an address.
         *
         * @details Deactivates all slots keyed on the address -- one-shots that
         *  have not finished sending included, not only auto-refresh slots.
         *
         * @param address The @ref dcc_address_t whose slots are purged.
         */
    extern void DccApplicationCommandStationMainTrack_remove_from_auto_refresh(dcc_address_t address);

        /**
         * @brief Clear every scheduler slot, auto-refresh and pending one-shots alike.
         *
         * @details Nothing is left for the scheduler to send until a new packet
         *  is inserted.
         */
    extern void DccApplicationCommandStationMainTrack_remove_all_auto_refresh(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_COMMAND_STATION_MAIN_TRACK__ */
