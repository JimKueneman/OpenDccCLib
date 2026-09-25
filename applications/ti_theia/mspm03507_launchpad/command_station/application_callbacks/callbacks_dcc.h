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
 * @file callbacks_dcc.h
 * @brief Application callbacks that respond to DCC library events.
 *
 * @details These functions are wired into the dcc_config_t struct in command_station.c.
 * The library calls them when specific events occur (packet sent, service mode
 * complete, etc.). They run in main-loop context (from DccConfig_run()), NOT
 * from ISR context, so they are safe to use UART output or other slow I/O.
 *
 * HOW TO ADD NEW CALLBACKS:
 *   1. Write your function here with a signature matching the corresponding
 *      function pointer typedef in dcc_config.h.
 *   2. Declare it in this header inside the DCC_COMPILE_COMMAND_STATION guard.
 *   3. In command_station.c, replace the NULL for that field with your
 *      function pointer.
 *
 * COMPILE GUARD: These callbacks are only compiled when
 * DCC_COMPILE_COMMAND_STATION is defined in dcc_user_config.h.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
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
