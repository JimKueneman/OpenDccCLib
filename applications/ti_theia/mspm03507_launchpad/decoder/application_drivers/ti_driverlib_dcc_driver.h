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
 * @brief Hardware driver for DCC on MSPM0G3507.
 *
 * @details PORTING GUIDE: To run on a different MCU, create a new driver file that
 * implements the same functions below using your MCU's HAL.  Then
 * update decoder.c to #include your file and wire the function pointers
 * in the dcc_config_t struct.
 *
 * This is the decoder-only version: it provides lock/unlock (interrupt
 * disable/enable) and a microsecond timestamp.  The command-station
 * version also needs timer start/stop/set_period and track power control.
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

/* Start the free-running timestamp timer. */
extern void TI_DccDriver_initialize(void);

/* Disable all interrupts (used by the library to protect shared state). */
extern void TI_DccDriver_lock_shared_resources(void);

/* Re-enable interrupts. */
extern void TI_DccDriver_unlock_shared_resources(void);

/* Return the current time in microseconds from a free-running counter.
 * The counter wraps at ~4295 seconds (32-bit overflow). */
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

/* Blocking microsecond delay for the RailCom Tx bit-bang, accurate at the 4 us
 * bit period via a 20 MHz (50 ns/tick) one-shot timer. */
extern void TI_DccDriver_railcom_delay_us(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
