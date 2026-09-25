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
 * @brief Reference implementation of the DCC hardware drivers for MSPM0G3507.
 *
 * @details If you are porting to a different MCU, use this file as a template.
 * Replace the TI DriverLib calls with your MCU's equivalents. The key
 * contracts each function must fulfill are documented in the header.
 *
 * TIMESTAMP PATTERN:
 * The microsecond timestamp is derived from the shared 58us DCC timer.
 * TI_DccDriver_timestamp_tick() is called from the shared timer ISR and
 * increments a software counter. get_timestamp_usec() multiplies by 58
 * to return approximate microseconds. Resolution is 58us, which is
 * sufficient for timeout calculations.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_driverlib_dcc_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

    /** @brief Software timestamp tick counter, advanced every 58 us by TI_DccDriver_timestamp_tick() from the shared timer ISR. */
static volatile uint32_t _timestamp_ticks = 0;

    /**
     * @brief Resets the software timestamp counter.
     */
void TI_DccDriver_initialize(void) {

    _timestamp_ticks = 0;
}

    /**
     * @brief Disables all interrupts (bare __disable_irq(), not nest-counted).
     */
void TI_DccDriver_lock_shared_resources(void) {

    __disable_irq();
}

    /**
     * @brief Re-enables interrupts.
     */
void TI_DccDriver_unlock_shared_resources(void) {

    __enable_irq();
}

    /**
     * @brief Converts the 58 us tick count into an approximate microsecond timestamp.
     *
     * @details The 32-bit tick read is atomic on Cortex-M0+, so no lock is needed.
     *
     * @return Ticks multiplied by 58; resolution 58 us, wraps at 2^32 us.
     */
uint32_t TI_DccDriver_get_timestamp_usec(void) {

    /* Each tick is 58us (DCC_ONE_BIT_HALF_PERIOD_US). Multiply to get
     * approximate microseconds. Atomic 32-bit read on Cortex-M0+. */
    return _timestamp_ticks * 58u;
}

    /**
     * @brief Legacy per-channel bit timer start (not wired into dcc_config_t).
     *
     * @details The counter counts load..0 inclusive, so the load value is period - 1 at 1 MHz.
     *
     * @verbatim
     * @param half_bit_period_usec Half-bit period in microseconds.
     * @endverbatim
     */
void TI_DccDriver_timer_start(uint16_t half_bit_period_usec) {

    /* Counter counts load..0 inclusive, so the period is (load + 1) ticks.
     * At 1 MHz (1 tick = 1us) the load value must be period_us - 1. */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, half_bit_period_usec - 1u);
    DL_TimerA_enableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(DCC_BIT_TIMER_INST);
}

    /**
     * @brief Legacy per-channel half-bit period change; a single load-register write.
     *
     * @verbatim
     * @param half_bit_period_usec Half-bit period in microseconds for the next half-bit.
     * @endverbatim
     */
void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec) {

    /* Single register write — safe to call from ISR context.
     * load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, half_bit_period_usec - 1u);
}

    /**
     * @brief Legacy per-channel bit timer stop; halts the counter and disables its interrupt.
     */
void TI_DccDriver_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Drives the main DCC signal pin high (on) or low (off) as the demo's main-track power control.
     *
     * @verbatim
     * @param enabled true = power on, false = power off.
     * @endverbatim
     */
void TI_DccDriver_track_power_set(bool enabled) {

    if (enabled) {
        DL_GPIO_setPins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
    }
}

    /**
     * @brief Reads the ACK sense pin (PB12) and maps it onto the library's milliamp scale.
     *
     * @return 100 when the pin is HIGH (decoder asserting ACK), 0 when LOW.
     */
uint16_t TI_DccDriver_current_sense_read(void) {

    /* Read the ACK sense pin (PB12).  Returns 100 (above threshold) when
     * the decoder board is asserting its ACK GPIO, 0 otherwise. */
    uint32_t pins = DL_GPIO_readPins(GPIO_ACK_SENSE_PORT,
                                      GPIO_ACK_SENSE_ACK_IN_PIN);
    return (pins & GPIO_ACK_SENSE_ACK_IN_PIN) ? 100 : 0;
}

    /**
     * @brief Loads DCC_BIT_TIMER_INST with the shared period, enables its interrupt and starts it.
     *
     * @verbatim
     * @param period_usec Timer period in microseconds (58 for DCC).
     * @endverbatim
     */
void TI_DccDriver_shared_timer_start(uint16_t period_usec) {

    /* load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, period_usec - 1u);
    DL_TimerA_enableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(DCC_BIT_TIMER_INST);
}

    /**
     * @brief Stops DCC_BIT_TIMER_INST and disables its interrupt.
     */
void TI_DccDriver_shared_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Loads RAILCOM_TIMER_INST with the one-shot period, enables its interrupt and starts it.
     *
     * @details All cutout state delays are non-zero, so the period - 1 load value never underflows.
     *
     * @verbatim
     * @param period_usec One-shot period in microseconds for the current cutout state.
     * @endverbatim
     */
void TI_DccDriver_railcom_timer_start(uint16_t period_usec) {

    /* load = period_us - 1 (period is load + 1 ticks at 1 MHz).
     * All cutout state delays are non-zero, so this never underflows. */
    DL_TimerA_setLoadValue(RAILCOM_TIMER_INST, period_usec - 1u);
    DL_TimerA_enableInterrupt(RAILCOM_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(RAILCOM_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(RAILCOM_TIMER_INST);
}

    /**
     * @brief Stops RAILCOM_TIMER_INST and disables its interrupt.
     */
void TI_DccDriver_railcom_timer_stop(void) {

    DL_TimerA_stopCounter(RAILCOM_TIMER_INST);
    DL_TimerA_disableInterrupt(RAILCOM_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(RAILCOM_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Toggles the main DCC signal pin, and the mirror pin while track-select (PB17) is LOW.
     */
void TI_DccDriver_main_pin_toggle(void) {

    DL_GPIO_togglePins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
    if (!(DL_GPIO_readPins(GPIO_TRACK_SELECT_PORT, GPIO_TRACK_SELECT_TRACK_SEL_PIN) & GPIO_TRACK_SELECT_TRACK_SEL_PIN)) {
        DL_GPIO_togglePins(GPIO_DCC_MIRROR_PORT, GPIO_DCC_MIRROR_DCC_MIRROR_PIN);
    }
}

    /**
     * @brief Drives the service DCC signal pin and the track-select pin (PB17) together.
     *
     * @details Track-select HIGH tells the decoder board to listen on its service input (PB4);
     * LOW returns it to the main input (PB1).
     *
     * @verbatim
     * @param enabled true = service track on, false = off.
     * @endverbatim
     */
void TI_DccDriver_svc_track_power_set(bool enabled) {

    if (enabled) {
        DL_GPIO_setPins(GPIO_SERVICE_MODE_DCC_PORT,
                        GPIO_SERVICE_MODE_DCC_SERVICE_MODE_DCC_SIGNAL_PIN);
        /* Tell decoder to listen on service track input (PB4) */
        DL_GPIO_setPins(GPIO_TRACK_SELECT_PORT,
                        GPIO_TRACK_SELECT_TRACK_SEL_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_SERVICE_MODE_DCC_PORT,
                          GPIO_SERVICE_MODE_DCC_SERVICE_MODE_DCC_SIGNAL_PIN);
        /* Tell decoder to listen on main track input (PB1) */
        DL_GPIO_clearPins(GPIO_TRACK_SELECT_PORT,
                          GPIO_TRACK_SELECT_TRACK_SEL_PIN);
    }
}

    /**
     * @brief Toggles the service DCC signal pin, and the mirror pin while track-select (PB17) is HIGH.
     */
void TI_DccDriver_svc_pin_toggle(void) {

    DL_GPIO_togglePins(GPIO_SERVICE_MODE_DCC_PORT,
                       GPIO_SERVICE_MODE_DCC_SERVICE_MODE_DCC_SIGNAL_PIN);
    if (DL_GPIO_readPins(GPIO_TRACK_SELECT_PORT, GPIO_TRACK_SELECT_TRACK_SEL_PIN) & GPIO_TRACK_SELECT_TRACK_SEL_PIN) {
        DL_GPIO_togglePins(GPIO_DCC_MIRROR_PORT, GPIO_DCC_MIRROR_DCC_MIRROR_PIN);
    }
}

    /**
     * @brief Increments the software timestamp tick counter.
     */
void TI_DccDriver_timestamp_tick(void) {

    _timestamp_ticks++;
}
