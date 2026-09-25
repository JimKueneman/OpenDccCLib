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
 * rewrite. Each function below is wired into the dcc_config_t struct in
 * command_station.c. You must provide an implementation that fulfills the
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

// One-time hardware setup. Call before DccConfig_initialize().
extern void TI_DccDriver_initialize(void);

// Disable all interrupts (or acquire a mutex). The library calls this to
// protect shared data structures accessed from both ISR and main-loop context.
// Must be nestable or paired with unlock. Keep the critical section short.
extern void TI_DccDriver_lock_shared_resources(void);

// Re-enable interrupts (or release the mutex). Must match a prior lock call.
extern void TI_DccDriver_unlock_shared_resources(void);

// Return a free-running microsecond timestamp. The library uses this for
// timeout calculations. Does not need to be absolute -- only monotonic.
// Wrapping at 2^32 (~71 minutes) is fine; the library handles wrap-around.
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

// Start the DCC bit timer with the given half-bit period in microseconds.
// Typical values: 58 us for a '1' bit, 100 us for a '0' bit.
// The timer ISR must call DccConfig_timer_half_bit_isr() on each compare match.
// This function is called from main-loop context (not ISR-safe requirement).
extern void TI_DccDriver_timer_start(uint16_t half_bit_period_usec);

// Change the timer period for the next half-bit. Called FROM ISR context by
// the library's bit encoder, so this must be fast -- ideally a single
// register write. Do not disable/re-enable interrupts here.
extern void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec);

// Stop the DCC bit timer and disable its interrupt. Called from main-loop
// context when track power is turned off.
extern void TI_DccDriver_timer_stop(void);

// Enable or disable the track power output (H-bridge enable).
// enabled=true means power on, enabled=false means power off.
extern void TI_DccDriver_track_power_set(bool enabled);

// Read the ACK sense GPIO pin (PB12) as a digital current-sense substitute.
// Returns 100 when the pin is HIGH (decoder is asserting ACK), 0 when LOW.
// The library compares this against USER_DEFINED_DCC_ACK_THRESHOLD_MA (60),
// so 100 > 60 triggers ACK detection.
extern uint16_t TI_DccDriver_current_sense_read(void);

// Start the shared fixed-period DCC timer (58us). Both main track and service
// track are clocked from this single timer. The ISR must call
// DccConfig_58us_timer_isr().
extern void TI_DccDriver_shared_timer_start(uint16_t period_usec);

// Stop the shared fixed-period DCC timer.
extern void TI_DccDriver_shared_timer_stop(void);

// Start the RailCom cutout one-shot timer with the given period in
// microseconds. The ISR must call DccConfig_railcom_oneshot_timer_isr().
extern void TI_DccDriver_railcom_timer_start(uint16_t period_usec);

// Stop the RailCom cutout one-shot timer.
extern void TI_DccDriver_railcom_timer_stop(void);

// Toggle the main track DCC signal GPIO pin. Called from ISR context by the
// bit encoder's tick_isr.
extern void TI_DccDriver_main_pin_toggle(void);

// Enable or disable the service track power output (PB4).
extern void TI_DccDriver_svc_track_power_set(bool enabled);

// Toggle the service track DCC signal GPIO pin. Called from ISR context by the
// bit encoder's tick_isr.
extern void TI_DccDriver_svc_pin_toggle(void);

// Increment the software timestamp counter. Call from the shared DCC timer ISR
// (every 58us) to provide a free-running microsecond timestamp.
extern void TI_DccDriver_timestamp_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
