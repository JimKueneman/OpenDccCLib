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
 * rewrite. Most functions below are wired into the dcc_config_t struct in
 * command_station.c. You must provide an implementation that fulfills the
 * contract described in each comment. The per-channel timer helpers
 * (timer_start / timer_set_period / timer_stop) are no longer referenced by
 * dcc_config_t, which has no per-channel timer fields; they remain only as a
 * reference for hardware with one timer per track.
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

    /**
     * @brief One-time driver setup; resets the software timestamp counter.
     *
     * @details Call after SYSCFG_DL_init() and before DccConfig_initialize().
     */
extern void TI_DccDriver_initialize(void);

    /**
     * @brief Disables all interrupts to protect library data shared between ISR and main-loop context.
     *
     * @details This implementation is a bare __disable_irq(); it is not nest-counted, so every
     * lock must be matched by exactly one unlock. Keep the critical section short.
     */
extern void TI_DccDriver_lock_shared_resources(void);

    /**
     * @brief Re-enables interrupts. Must match a prior lock call.
     */
extern void TI_DccDriver_unlock_shared_resources(void);

    /**
     * @brief Free-running microsecond timestamp used by the library for timeouts.
     *
     * @details Monotonic, not absolute. Derived from the shared 58 us timer tick count,
     * so the resolution is 58 us. Wraps at 2^32 us (~71 minutes); the library handles
     * wrap-around.
     *
     * @return Microseconds since TI_DccDriver_initialize(), in 58 us steps.
     */
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

    /**
     * @brief Legacy per-channel bit timer start (not wired into dcc_config_t).
     *
     * @details Loads DCC_BIT_TIMER_INST with the given half-bit period, enables its zero-event
     * interrupt and starts it. The library now clocks both tracks from the shared 58 us timer
     * (see TI_DccDriver_shared_timer_start), so this is kept only as a reference.
     *
     * @param half_bit_period_usec Half-bit period in microseconds (58 for a one bit, 100 for a zero bit).
     */
extern void TI_DccDriver_timer_start(uint16_t half_bit_period_usec);

    /**
     * @brief Legacy per-channel half-bit period change (not wired into dcc_config_t).
     *
     * @details A single load-register write, so it would be safe from ISR context. Kept as a
     * reference alongside TI_DccDriver_timer_start.
     *
     * @param half_bit_period_usec Half-bit period in microseconds for the next half-bit.
     */
extern void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec);

    /**
     * @brief Legacy per-channel bit timer stop (not wired into dcc_config_t).
     *
     * @details Stops DCC_BIT_TIMER_INST and disables its interrupt. Kept as a reference
     * alongside TI_DccDriver_timer_start.
     */
extern void TI_DccDriver_timer_stop(void);

    /**
     * @brief Main-track power control, wired to main_track.track_power_set.
     *
     * @details The demo has no H-bridge enable on the main track, so this drives the main
     * DCC signal pin (GPIO_DCC_DCC_SIGNAL_PIN) high for on and low for off. A real design
     * would gate the H-bridge enable here instead.
     *
     * @param enabled true = power on, false = power off.
     */
extern void TI_DccDriver_track_power_set(bool enabled);

    /**
     * @brief Digital ACK sense on PB12 standing in for an analog current measurement.
     *
     * @details Wired to service_track.current_sense_read. The library compares the result
     * against USER_DEFINED_DCC_ACK_THRESHOLD_MA (60), so a HIGH pin (100) registers as an ACK.
     *
     * @return 100 when the decoder is asserting its ACK pin, 0 otherwise.
     */
extern uint16_t TI_DccDriver_current_sense_read(void);

    /**
     * @brief Starts the shared fixed-period DCC timer that clocks both tracks.
     *
     * @details Wired to shared_timer_start. The timer ISR must call DccConfig_58us_timer_isr()
     * on every zero event; both the main and service track bit encoders run from it.
     *
     * @param period_usec Timer period in microseconds (58 for DCC).
     */
extern void TI_DccDriver_shared_timer_start(uint16_t period_usec);

    /**
     * @brief Stops the shared fixed-period DCC timer and disables its interrupt.
     */
extern void TI_DccDriver_shared_timer_stop(void);

    /**
     * @brief Starts the RailCom cutout one-shot timer.
     *
     * @details Wired to railcom_timer_start. The timer ISR must call
     * DccConfig_railcom_oneshot_timer_isr() so the cutout state machine advances.
     *
     * @param period_usec One-shot period in microseconds for the current cutout state.
     */
extern void TI_DccDriver_railcom_timer_start(uint16_t period_usec);

    /**
     * @brief Stops the RailCom cutout one-shot timer and disables its interrupt.
     */
extern void TI_DccDriver_railcom_timer_stop(void);

    /**
     * @brief Toggles the main-track DCC signal pin; called from the shared timer ISR by the bit encoder.
     *
     * @details Also toggles the DCC mirror pin while the track-select pin (PB17) is LOW, so the
     * logic analyzer mirror shows whichever track the decoder is currently listening to.
     */
extern void TI_DccDriver_main_pin_toggle(void);

    /**
     * @brief Service-track power control, wired to service_track.track_power_set.
     *
     * @details Called by the library when service mode is entered and exited. Drives the
     * service DCC signal pin and the track-select pin (PB17) high or low together, so the
     * decoder board switches its input to the service track (PB4) for the programming
     * session and back to the main track (PB1) afterwards.
     *
     * @param enabled true = service track on, false = off.
     */
extern void TI_DccDriver_svc_track_power_set(bool enabled);

    /**
     * @brief Toggles the service-track DCC signal pin; called from the shared timer ISR by the bit encoder.
     *
     * @details Also toggles the DCC mirror pin while the track-select pin (PB17) is HIGH.
     */
extern void TI_DccDriver_svc_pin_toggle(void);

    /**
     * @brief Advances the software timestamp counter by one 58 us tick.
     *
     * @details Call from the shared DCC timer ISR on every zero event; this is what gives
     * TI_DccDriver_get_timestamp_usec() its time base.
     */
extern void TI_DccDriver_timestamp_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
