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
 * @file dcc_failsafe.c
 * @brief Packet-timeout fail-safe (S-9.2.4 §4, CV11).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_failsafe.h"

#ifdef DCC_COMPILE_DECODER

// =============================================================================
// Static state
// =============================================================================

    /** @brief Injected clock, CV-read and notification hooks, set by DccFailsafe_initialize. */
static const interface_dcc_failsafe_t *_interface;

    /** @brief Timestamp (us) of the last command packet addressed to us. */
static uint32_t _last_packet_usec;

    /** @brief True while timed out (stopped) and not yet recovered. */
static bool _active;

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the fail-safe module.
     *
     * @details Algorithm:
     * -# Store the interface pointer and clear the tripped state.
     * -# Stamp the last-packet clock with "now" (0 without a clock hook) so a
     *    fresh decoder does not trip before its first packet.
     *
     * @verbatim
     * @param interface Pointer to a populated interface_dcc_failsafe_t.
     * @endverbatim
     */
void DccFailsafe_initialize(const interface_dcc_failsafe_t *interface) {

    _interface = interface;
    _active = false;

    /* Stamp "seen now" so a fresh decoder does not trip before the first
     * packet arrives. */
    _last_packet_usec = (interface && interface->get_timestamp_usec) ? interface->get_timestamp_usec() : 0;

}

    /**
     * @brief Note that a command packet addressed to this decoder was received.
     *
     * @details Algorithm:
     * -# Ignore the call if the module is not initialized.
     * -# Re-stamp the last-packet clock.
     * -# If currently tripped, clear the state and fire on_failsafe_exited once.
     */
void DccFailsafe_note_valid_packet(void) {

    if (!_interface) {

        return;

    }

    if (_interface->get_timestamp_usec) {

        _last_packet_usec = _interface->get_timestamp_usec();

    }

    /* A valid addressed packet ends any fail-safe condition. */
    if (_active) {

        _active = false;

        if (_interface->on_failsafe_exited) {

            _interface->on_failsafe_exited();

        }

    }

}

    /**
     * @brief Periodic poll. Call from the main loop (DccConfig_run).
     *
     * @details Algorithm:
     * -# Return if uninitialized or the cv_read / clock hooks are absent.
     * -# Return while already tripped: recovery happens only in
     *    DccFailsafe_note_valid_packet.
     * -# Read CV11; return if unreadable or 0 (time-out disabled).
     * -# Trip and fire on_failsafe_entered once when the time since the last
     *    addressed packet reaches CV11 * DCC_FAILSAFE_CV11_UNIT_US (unsigned
     *    arithmetic tolerates clock wrap).
     */
void DccFailsafe_run(void) {

    uint8_t cv11;
    uint32_t timeout_usec;
    uint32_t elapsed_usec;

    if (!_interface || !_interface->cv_read || !_interface->get_timestamp_usec) {

        return;

    }

    /* Already tripped: stay stopped until a packet recovers us (handled in
     * DccFailsafe_note_valid_packet). Nothing further to do. */
    if (_active) {

        return;

    }

    if (!_interface->cv_read(DCC_CV_PACKET_TIMEOUT, &cv11)) {

        return;

    }

    /* CV11 = 0 disables the time-out (S-9.2.4 §4). */
    if (cv11 == 0) {

        return;

    }

    timeout_usec = (uint32_t)cv11 * DCC_FAILSAFE_CV11_UNIT_US;

    /* Unsigned subtraction tolerates the microsecond clock wrapping. */
    elapsed_usec = _interface->get_timestamp_usec() - _last_packet_usec;

    if (elapsed_usec >= timeout_usec) {

        _active = true;

        if (_interface->on_failsafe_entered) {

            _interface->on_failsafe_entered();

        }

    }

}

    /**
     * @brief Query whether the decoder is currently in fail-safe.
     * @return true if tripped (timed out and not yet recovered).
     */
bool DccFailsafe_is_active(void) {

    return _active;

}

#endif /* DCC_COMPILE_DECODER */
