/*
 * ack_pulse_driver.h -- GPIO-based ACK pulse generator for service mode testing.
 *
 * Generates a timed HIGH pulse on the ACK output pin (PB12) when fire() is
 * called.  The pulse width is configurable via set_width_us() and defaults
 * to 6000 us (6 ms) per NMRA S-9.2.3.  A one-shot hardware timer (TIMG12)
 * clears the pin automatically so the caller (ISR context) is never blocked.
 *
 * The CS reads this pin as a digital current-sense substitute to detect ACK.
 *
 * UART commands on the decoder allow the Python test script to:
 *   ACK <width_us>  -- set pulse width
 *   ACK ON          -- enable ACK generation
 *   ACK OFF         -- disable ACK generation (forces NO_ACK on CS)
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
     */
extern uint32_t AckPulseDriver_get_width_us(void);

    /**
     * @brief Enable or disable ACK pulse generation.
     * @param enabled  true = fire() produces a pulse, false = fire() is a no-op.
     */
extern void AckPulseDriver_set_enabled(bool enabled);

    /**
     * @brief Check whether ACK generation is enabled.
     */
extern bool AckPulseDriver_is_enabled(void);

    /**
     * @brief Fire an ACK pulse.  ISR-safe.
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
