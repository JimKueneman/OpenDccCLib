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
// This demo implementation toggles a debug GPIO for oscilloscope triggering.
extern void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef __cplusplus
}
#endif

#endif /* __CALLBACKS_DCC__ */
