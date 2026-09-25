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
 * disable/enable), a microsecond timestamp and the RailCom bit-bang delay.
 * The command-station version also needs shared and RailCom timer control,
 * DCC pin toggling and track power control.
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
     * @brief Starts the free-running 1 MHz timestamp timer.
     *
     * @details Call after SYSCFG_DL_init() and before DccConfig_initialize().
     */
extern void TI_DccDriver_initialize(void);

    /**
     * @brief Disables all interrupts; the library uses this to protect shared state.
     *
     * @details A bare __disable_irq(), not nest-counted, so every lock must be matched by exactly one unlock.
     */
extern void TI_DccDriver_lock_shared_resources(void);

    /**
     * @brief Re-enables interrupts. Must match a prior lock call.
     */
extern void TI_DccDriver_unlock_shared_resources(void);

    /**
     * @brief Current time in microseconds from the free-running counter.
     *
     * @details 16-bit hardware timer plus a software overflow count. Wraps at 2^32 us (~4295 seconds).
     *
     * @return Microseconds since TI_DccDriver_initialize().
     */
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

    /**
     * @brief Blocking microsecond delay for the RailCom Tx bit-bang.
     *
     * @details Accurate at the 4 us bit period because it spins on a 20 MHz (50 ns per tick)
     * one-shot timer. Wired to dcc_config_t.railcom_delay_us. See the NOT FINISHED note on the
     * implementation: this demo's SysConfig has no DELAY_TIMER instance yet.
     *
     * @param us Delay in microseconds.
     */
extern void TI_DccDriver_railcom_delay_us(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
