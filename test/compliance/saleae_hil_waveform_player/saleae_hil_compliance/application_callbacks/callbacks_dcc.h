// callbacks_dcc.h
//
// Application callbacks that respond to DCC library events.
//
// These functions are wired into the dcc_config_t struct in command_station.c.
// The library calls them when specific events occur (packet sent, service mode
// complete, etc.). They run in main-loop context (from DccConfig_run()), NOT
// from ISR context, so they are safe to use UART output or other slow I/O.
//
// HOW TO ADD NEW CALLBACKS:
//   1. Write your function here with a signature matching the corresponding
//      function pointer typedef in dcc_config.h.
//   2. Declare it in this header inside the DCC_COMPILE_COMMAND_STATION guard.
//   3. In command_station.c, replace the NULL for that field with your
//      function pointer.
//
// COMPILE GUARD: These callbacks are only compiled when
// DCC_COMPILE_COMMAND_STATION is defined in dcc_user_config.h.

#ifndef __CALLBACKS_DCC__
#define __CALLBACKS_DCC__

#include "dcc_lib/dcc_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DCC_COMPILE_COMMAND_STATION

// Called after every DCC packet is fully transmitted on the track.
// In this HIL-compliance firmware it drives the test-trigger GPIO (PB3): when
// armed via CallbacksDcc_arm_trigger(), the next NON-idle packet raises PB3 so
// a logic analyzer can hardware-trigger on the exact packet under test.
extern void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet);

// Arm the test trigger: clears PB3 low and arms it so the next non-idle packet
// transmitted drives a single clean rising edge on PB3. Called from the UART
// "TRIG" command just before the harness sends the command under test.
extern void CallbacksDcc_arm_trigger(void);

// Mock decoder (HIL only). Hold one CV value so the bench can exercise read-back
// and write+verify end-to-end through the real ACK path. set() makes the mock
// ACK Direct verify commands for `cv` that match `value` (and accept writes to
// it); off() disables it. Driven by the UART "SVC MOCKCV" command.
extern void CallbacksDcc_mock_decoder_set(uint16_t cv, uint8_t value);
extern void CallbacksDcc_mock_decoder_off(void);

// HIL boundary test: when early=true the width-test mock fires its pulse on the
// FIRST command packet (inside the blanking window) so a test can confirm the
// library masks it; false restores normal in-window firing.
extern void CallbacksDcc_set_mock_ack_early(bool early);

// RailCom cutout cancel (HIL only, S-9.3.2 CS-008). arm() requests a cancel of the
// next in-progress cutout; the 58us bit-ISR must call cancel_tick() each tick to
// fire it mid-cutout (restoring the H-bridge early). One-shot; disarms after firing.
extern void CallbacksDcc_arm_railcom_cancel(void);
extern void CallbacksDcc_railcom_cancel_tick(void);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef __cplusplus
}
#endif

#endif /* __CALLBACKS_DCC__ */
