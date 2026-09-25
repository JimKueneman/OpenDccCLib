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
 * @file ack_pulse_driver.h
 * @brief GPIO-based ACK pulse generator for service mode testing.
 *
 * @details Drives the ACK output pin (PB12).  In normal operation the DCC library
 * calls start() and, 6 ms later, stop() through dcc_config_t.  For bench testing,
 * fire() generates a timed HIGH pulse on its own: the width is configurable via
 * set_width_us(), defaults to 6000 us (6 ms) per NMRA S-9.2.3, and a one-shot
 * hardware timer (TIMA1, ACK_PULSE_TIMER) clears the pin automatically so the
 * caller is never blocked.
 *
 * The CS reads this pin as a digital current-sense substitute to detect ACK.
 *
 * UART commands on the decoder allow the Python test script to:
 *   ACK <width_us>  -- set pulse width
 *   ACK ON          -- enable ACK generation
 *   ACK OFF         -- disable ACK generation (forces NO_ACK on CS)
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __ACK_PULSE_DRIVER__
#define __ACK_PULSE_DRIVER__

#include "dcc_lib/dcc_types.h"

#ifdef DCC_COMPILE_DECODER

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Initialize the ACK pulse driver.
     *
     * @details Sets the ACK pin LOW, stores default width (6000 us), and
     * marks the driver as enabled.  Call once at startup after SYSCFG_DL_init().
     */
extern void AckPulseDriver_initialize(void);

    /**
     * @brief Set the ACK pulse width in microseconds.
     * @param width_us Pulse duration.  Clamped to 1000-20000 us.
     */
extern void AckPulseDriver_set_width_us(uint32_t width_us);

    /**
     * @brief Get the current ACK pulse width in microseconds.
     *
     * @return Pulse width used by fire(), 1000-20000 us.
     */
extern uint32_t AckPulseDriver_get_width_us(void);

    /**
     * @brief Enable or disable ACK pulse generation.
     * @param enabled  true = fire() produces a pulse, false = fire() is a no-op.
     */
extern void AckPulseDriver_set_enabled(bool enabled);

    /**
     * @brief Check whether ACK generation is enabled.
     *
     * @return true when start() and fire() drive the pin, false when they are no-ops.
     */
extern bool AckPulseDriver_is_enabled(void);

    /**
     * @brief Start ACK pulse — sets the ACK pin HIGH.
     *
     * @details The DCC library handles 6ms timing and calls stop() automatically.
     * No-op if ACK generation is disabled.
     */
extern void AckPulseDriver_start(void);

    /**
     * @brief Stop ACK pulse — clears the ACK pin LOW.
     *
     * @details Called by the DCC library after 6ms has elapsed.
     */
extern void AckPulseDriver_stop(void);

    /**
     * @brief Fire a self-timed ACK pulse using the hardware timer (used by the ACK TEST command).
     *
     * @details Sets the ACK pin HIGH and starts the one-shot timer.  The timer
     * ISR clears the pin when it expires.
     *
     * No-op if:
     *   - ACK generation is disabled (set_enabled(false))
     *   - A pulse is already in progress (timer still running)
     */
extern void AckPulseDriver_fire(void);

#ifdef __cplusplus
}
#endif

#endif /* DCC_COMPILE_DECODER */

#endif /* __ACK_PULSE_DRIVER__ */
