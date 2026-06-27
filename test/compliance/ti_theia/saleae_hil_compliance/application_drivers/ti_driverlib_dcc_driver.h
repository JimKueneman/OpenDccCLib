// ti_driverlib_dcc_driver.h
//
// Hardware driver interface for the DCC library on MSPM0G3507.
//
// PORTING GUIDE: If you are bringing up a new MCU, this is the file to
// rewrite. Each function below is wired into the dcc_config_t struct in
// command_station.c. You must provide an implementation that fulfills the
// contract described in each comment.
//
// All functions in this file are called by the DCC library through function
// pointers. The library does not care how you implement them, only that
// they behave as documented.

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

// Read the MOCK_ACK GPIO pin (PB9) as a digital current-sense substitute.
// Returns 100 when the pin is HIGH (mock ACK asserted), 0 when LOW.
// The library compares this against USER_DEFINED_DCC_ACK_THRESHOLD_MA (60),
// so 100 > 60 triggers ACK detection.
extern uint16_t TI_DccDriver_current_sense_read(void);

// Mock ACK loopback (HIL only): drive MOCK_ACK_DRIVE (PB24), jumpered to
// MOCK_ACK (PB9), to inject a controlled-width ACK pulse the library reads back.
//   arm(width_us)  : arm a one-shot pulse of the given width (rounded to 58us ticks)
//   on_command()   : start the armed pulse (call when a service command packet sends)
//   tick()         : advance the pulse one ISR tick (call every 58us, before ack sample)
extern void TI_DccDriver_mock_ack_arm(uint16_t width_us);
extern void TI_DccDriver_mock_ack_on_command(void);
extern void TI_DccDriver_mock_ack_tick(void);

// Mock decoder (HIL only): immediately start a mock-ACK pulse of the given width.
// Used to ACK a verify command the moment a held-value match is detected.
extern void TI_DccDriver_mock_ack_fire(uint16_t width_us);

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

// Toggle the main track DCC signal GPIO pin. Called from ISR context by the bit
// encoder's tick_isr. Always toggles -- the DCC line runs continuously and looks
// identical with or without RailCom.
extern void TI_DccDriver_main_pin_toggle(void);

// RailCom cutout-active signal: raise (T_CS) / drop (T_CE) the PB2 (DCC_MIRROR)
// pin. This is the signal that real H-bridge hardware muxes on to tristate the
// track during the cutout; here it is the Saleae cutout-window marker. Wired to
// the .railcom begin/end hooks, called by the cutout timer.
extern void TI_DccDriver_main_cutout_begin(void);
extern void TI_DccDriver_main_cutout_end(void);

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
