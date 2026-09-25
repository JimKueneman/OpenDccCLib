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
 * @file ti_driverlib_dcc_driver.h
 * @brief Hardware driver interface for the DCC library on MSPM0G3507.
 *
 * @details PORTING GUIDE: If you are bringing up a new MCU, this is the file to
 * rewrite. The functions below are wired into the dcc_config_t struct in
 * saleae_hil_compliance.c (the per-channel timer_start / timer_set_period /
 * timer_stop trio is kept for reference but is not wired: the bench config uses
 * the shared 58 us timer). You must provide an implementation that fulfills the
 * contract described in each comment.
 *
 * All functions in this file are called by the DCC library through function
 * pointers. The library does not care how you implement them, only that
 * they behave as documented.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __TI_DRIVERLIB_DCC_DRIVER__
#define __TI_DRIVERLIB_DCC_DRIVER__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /** @brief One-time driver setup (zeroes the timestamp counter). Call before DccConfig_initialize(). */
extern void TI_DccDriver_initialize(void);

    /**
     * @brief Disable all interrupts to protect shared library state.
     *
     * @details The library calls this around data structures accessed from both ISR
     * and main-loop context. Must be paired with TI_DccDriver_unlock_shared_resources().
     * Keep the critical section short.
     */
extern void TI_DccDriver_lock_shared_resources(void);

    /** @brief Re-enable interrupts. Must match a prior lock call. */
extern void TI_DccDriver_unlock_shared_resources(void);

    /**
     * @brief Free-running microsecond timestamp for library timeouts.
     *
     * @details Derived from the shared 58 us timer tick, so resolution is 58 us. Only
     * monotonic behaviour matters; wrapping at 2^32 is fine because the library
     * handles wrap-around.
     *
     * @return Approximate microseconds since TI_DccDriver_initialize().
     */
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

    /**
     * @brief Start the per-channel DCC bit timer with the given half-bit period (reference only, not wired on this bench).
     *
     * @details Typical values: 58 us for a one bit, 100 us for a zero bit. The timer
     * ISR would call the half-bit ISR on each compare match. The bench config uses
     * TI_DccDriver_shared_timer_start() instead.
     *
     * @param half_bit_period_usec  Half-bit period in microseconds.
     */
extern void TI_DccDriver_timer_start(uint16_t half_bit_period_usec);

    /**
     * @brief Change the bit-timer period for the next half-bit (reference only, not wired on this bench).
     *
     * @details Intended for ISR context, so it is a single register write.
     *
     * @param half_bit_period_usec  Next half-bit period in microseconds.
     */
extern void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec);

    /** @brief Stop the per-channel DCC bit timer and disable its interrupt (reference only, not wired on this bench). */
extern void TI_DccDriver_timer_stop(void);

    /**
     * @brief Main-track power: idle level of the main DCC pin (PB1).
     *
     * @details The bench has no H-bridge, so "power" is just the pin level the
     * encoder toggles from: high when enabled, low when off.
     *
     * @param enabled  true = pin high (power on), false = pin low (power off).
     */
extern void TI_DccDriver_track_power_set(bool enabled);

    /**
     * @brief Read the MOCK_ACK GPIO pin (PB9) as a digital current-sense substitute.
     *
     * @details The library compares the value against USER_DEFINED_DCC_ACK_THRESHOLD_MA
     * (60), so 100 > 60 registers as an ACK sample.
     *
     * @return 100 when the pin is HIGH (mock ACK asserted), 0 when LOW.
     */
extern uint16_t TI_DccDriver_current_sense_read(void);

    /**
     * @brief Arm a one-shot mock-ACK pulse of the given width (HIL only).
     *
     * @details Mock ACK loopback: MOCK_ACK_DRIVE (PB24) is jumpered to MOCK_ACK (PB9)
     * so the library reads back a controlled-width pulse. The width is rounded to
     * 58 us ticks; the pulse starts at the next TI_DccDriver_mock_ack_on_command().
     *
     * @param width_us  Pulse width in microseconds.
     */
extern void TI_DccDriver_mock_ack_arm(uint16_t width_us);
    /** @brief Start the armed mock-ACK pulse; call when a service command packet is sent. Fires once, then disarms. */
extern void TI_DccDriver_mock_ack_on_command(void);
    /** @brief Advance the mock-ACK pulse one ISR tick; call every 58 us before the library samples the ACK. */
extern void TI_DccDriver_mock_ack_tick(void);

    /**
     * @brief Arm an INTERRUPTED mock-ACK pulse: high pre_us, low gap_us, high post_us (HIL only).
     *
     * @details Fired by TI_DccDriver_mock_ack_on_command() like the plain pulse. Proves
     * the library ACK width counter resets on the low gap (S-9.2.3 CS-005): two
     * sub-pulses each below the 6 ms window, so a NO-ACK verdict means the counter
     * did not accumulate across it.
     *
     * @param pre_us   First high phase in microseconds.
     * @param gap_us   Low gap in microseconds.
     * @param post_us  Second high phase in microseconds.
     */
extern void TI_DccDriver_mock_ack_arm_glitch(uint16_t pre_us, uint16_t gap_us, uint16_t post_us);

    /**
     * @brief Immediately start a mock-ACK pulse of the given width (mock decoder, HIL only).
     *
     * @details Used to ACK a verify command the moment a held-value match is detected.
     *
     * @param width_us  Pulse width in microseconds.
     */
extern void TI_DccDriver_mock_ack_fire(uint16_t width_us);

    /**
     * @brief Start the shared fixed-period DCC timer (58 us).
     *
     * @details Both main track and service track are clocked from this single timer.
     * The ISR must call DccConfig_58us_timer_isr().
     *
     * @param period_usec  Timer period in microseconds (58).
     */
extern void TI_DccDriver_shared_timer_start(uint16_t period_usec);

    /** @brief Stop the shared fixed-period DCC timer and disable its interrupt. */
extern void TI_DccDriver_shared_timer_stop(void);

    /**
     * @brief Start the RailCom cutout one-shot timer.
     *
     * @details The ISR must call DccConfig_railcom_oneshot_timer_isr().
     *
     * @param period_usec  One-shot period in microseconds.
     */
extern void TI_DccDriver_railcom_timer_start(uint16_t period_usec);

    /** @brief Stop the RailCom cutout one-shot timer and disable its interrupt. */
extern void TI_DccDriver_railcom_timer_stop(void);

    /**
     * @brief Toggle the main-track DCC signal pin (PB1).
     *
     * @details Called from ISR context by the bit encoder tick. Always toggles: the
     * DCC line runs continuously and looks identical with or without RailCom.
     */
extern void TI_DccDriver_main_pin_toggle(void);

    /**
     * @brief RailCom cutout-active signal (T_CS): raise the PB2 cutout strobe.
     *
     * @details This is the signal real H-bridge hardware muxes on to tristate the
     * track during the cutout; here it is the Saleae cutout-window marker. Wired to
     * the .railcom begin hook, called by the cutout timer ISR.
     */
extern void TI_DccDriver_main_cutout_begin(void);
    /** @brief RailCom cutout-active signal (T_CE): drop the PB2 cutout strobe. Wired to the .railcom end hook. */
extern void TI_DccDriver_main_cutout_end(void);

    /**
     * @brief RailCom channel-window marker: raise RAILCOM_RX_WINDOW (PB18) while a Ch1/Ch2 window is open.
     *
     * @details Wired to the cutout .uart_rx_enable hook so the Saleae can time the
     * interior sub-windows (S-9.3.2 CS-005/006); also opens the loopback receiver gate.
     */
extern void TI_DccDriver_railcom_window_open(void);
    /** @brief RailCom channel-window marker: drop RAILCOM_RX_WINDOW (PB18). Wired to .uart_rx_disable; also closes the loopback receiver gate. */
extern void TI_DccDriver_railcom_window_close(void);

    /** @brief Toggle the service-track DCC signal pin (PB4). Called from ISR context by the bit encoder tick. */
extern void TI_DccDriver_svc_pin_toggle(void);

    /**
     * @brief Service-track power: idle level of the service-track DCC pin (PB4).
     *
     * @details Same convention as the main track: no H-bridge on the bench, so
     * "power" is the pin level the encoder toggles from.
     *
     * @param enabled  true = pin high (power on), false = pin low (power off).
     */
extern void TI_DccDriver_svc_track_power_set(bool enabled);

    /** @brief Increment the software timestamp counter. Call from the shared DCC timer ISR every 58 us. */
extern void TI_DccDriver_timestamp_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
