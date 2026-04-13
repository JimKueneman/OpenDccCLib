/*
 * ti_driverlib_dcc_driver.c -- Hardware driver implementation (MSPM0G3507).
 *
 * This file uses TI DriverLib to read a 16-bit hardware timer running at
 * 1 MHz (1 tick = 1 microsecond).  An overflow ISR counts how many times
 * the 16-bit timer has wrapped, giving a full 32-bit microsecond timestamp.
 *
 * PORTING: Replace the DL_TimerA_xxx calls with your MCU's equivalent
 * timer API.  The key requirement is a free-running counter at 1 us
 * resolution (or close to it).
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
