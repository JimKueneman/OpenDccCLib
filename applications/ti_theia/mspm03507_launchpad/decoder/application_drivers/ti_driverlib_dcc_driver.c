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
 * @file ti_driverlib_dcc_driver.c
 * @brief Hardware driver implementation (MSPM0G3507).
 *
 * @details This file uses TI DriverLib to read a 16-bit hardware timer running at
 * 1 MHz (1 tick = 1 microsecond).  An overflow ISR counts how many times
 * the 16-bit timer has wrapped, giving a full 32-bit microsecond timestamp.
 *
 * PORTING: Replace the DL_TimerA_xxx calls with your MCU's equivalent
 * timer API.  The key requirement is a free-running counter at 1 us
 * resolution (or close to it).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_driverlib_dcc_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

/* Overflow counter for 32-bit microsecond timestamp from 16-bit timer */
static volatile uint32_t _timestamp_overflows = 0;

void TI_DccDriver_initialize(void) {

    /* Start the free-running timestamp timer */
    DL_TimerA_startCounter(TIMESTAMP_TIMER_INST);

}

void TI_DccDriver_lock_shared_resources(void) {

    __disable_irq();

}

void TI_DccDriver_unlock_shared_resources(void) {

    __enable_irq();

}

/* Build a 32-bit microsecond timestamp from the 16-bit hardware timer.
 * Interrupts are briefly disabled so the overflow count and timer value
 * are read atomically (prevents a race where the timer overflows between
 * reading the overflow counter and reading the timer register). */
uint32_t TI_DccDriver_get_timestamp_usec(void) {

    uint32_t overflows;
    uint16_t count;

    __disable_irq();
    overflows = _timestamp_overflows;
    count = DL_TimerA_getTimerCount(TIMESTAMP_TIMER_INST);
    __enable_irq();

    /* Timer counts down from load value to 0 at 1 MHz (1 us per tick) */
    return (overflows * 65536u) + (65535u - count);

}

/* Timer overflow ISR -- fires every 65536 us (~65 ms).
 * Increments the overflow counter so get_timestamp_usec() can compute
 * the full 32-bit time. */
void TIMESTAMP_TIMER_INST_IRQHandler(void) {

    switch (DL_TimerA_getPendingInterrupt(TIMESTAMP_TIMER_INST)) {

        case DL_TIMER_IIDX_ZERO:

            _timestamp_overflows++;
            break;

        default:

            break;

    }

}

/* Blocking microsecond delay for the RailCom Tx bit-bang.  Uses a one-shot
 * 20 MHz hardware timer (20 ticks/us, 50 ns resolution) so the 4 us bit period
 * is accurate -- the 1 MHz timestamp timer is too coarse (1 us = 25% of a bit).
 *
 * NOT FINISHED (2026-09-25): DELAY_TIMER_INST is not defined for this demo.
 * decoder.syscfg has only TIMESTAMP_TIMER and ACK_PULSE_TIMER, so this file
 * does not compile as committed.  Add a third TIMER instance in SysConfig
 * named DELAY_TIMER with a 50.00 ns period (SysConfig places it on TIMG0,
 * one-shot, fed by the 20 MHz low-power bus clock).  The HIL decoder firmware
 * at test/compliance/mobile_decoder/saleae_hil_compliance/decoder.syscfg has
 * the working instance (TIMER3) and uses this identical driver file.
 *
 * PORTING: Replace with your MCU's equivalent one-shot timer or a cycle-accurate
 * busy-wait.  The requirement is sub-microsecond accuracy at a 4 us bit. */
void TI_DccDriver_railcom_delay_us(uint16_t us) {

    DL_TimerG_setLoadValue(DELAY_TIMER_INST, (uint16_t)(us * 20u));   /* 20 ticks/us @ 50 ns */
    DL_TimerG_startCounter(DELAY_TIMER_INST);

    while (!DL_TimerG_getRawInterruptStatus(DELAY_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT)) {

        /* spin until the counter reaches zero */

    }

    DL_TimerG_clearInterruptStatus(DELAY_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);

}
