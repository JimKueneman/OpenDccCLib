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
 * @file ack_pulse_driver.c
 * @brief GPIO-based ACK pulse generator for service mode testing.
 *
 * @details Uses a one-shot hardware timer (TIMA1) to produce a precise pulse on PB12.
 * The pulse width is configurable at runtime via UART commands so the Python
 * test script can exercise NMRA S-9.2.3 ACK timing compliance.
 *
 * REQUIRES SysConfig additions to decoder.syscfg:
 *
 *   const GPIO4   = GPIO.addInstance();
 *   GPIO4.$name                          = "GPIO_ACK";
 *   GPIO4.port                           = "PORTB";
 *   GPIO4.associatedPins[0].$name        = "ACK_OUT";
 *   GPIO4.associatedPins[0].assignedPin  = "12";
 *
 *   const TIMER2  = TIMER.addInstance();
 *   TIMER2.$name              = "ACK_PULSE_TIMER";
 *   TIMER2.timerClkPrescale   = 40;          // 40 MHz / 40 = 1 MHz (1 us ticks)
 *   TIMER2.timerPeriod        = "6 ms";      // default 6 ms
 *   TIMER2.timerMode          = "ONE_SHOT";
 *   TIMER2.interrupts         = ["ZERO"];
 *   TIMER2.interruptPriority  = "2";
 *   TIMER2.peripheral.$assign = "TIMA1";
 *
 * This generates the following defines in ti_msp_dl_config.h:
 *   ACK_PULSE_TIMER_INST              (TIMA1)
 *   ACK_PULSE_TIMER_INST_IRQHandler   TIMA1_IRQHandler
 *   ACK_PULSE_TIMER_INST_INT_IRQN     (TIMA1_INT_IRQn)
 *   ACK_PULSE_TIMER_INST_LOAD_VALUE   (5999U)
 *   GPIO_ACK_PORT               (GPIOB)
 *   GPIO_ACK_ACK_OUT_PIN        (DL_GPIO_PIN_12)
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ack_pulse_driver.h"

#ifdef DCC_COMPILE_DECODER

#include "ti_msp_dl_config.h"

/* ========================================================================== */
/* Static state                                                               */
/* ========================================================================== */

    /** @brief Pulse width for fire(), clamped to 1000-20000 us. */
static uint32_t _width_us = 6000;
    /** @brief When false, start() and fire() leave the pin alone. */
static bool _enabled = true;
    /** @brief true while a fire() pulse is in progress; cleared by the timer ISR. */
static volatile bool _active = false;

/* ========================================================================== */
/* Timer ISR — clears the ACK pin when the one-shot timer expires             */
/* ========================================================================== */

    /**
     * @brief One-shot timer ISR; ends a fire() pulse by clearing the ACK pin and stopping the counter.
     */
void ACK_PULSE_TIMER_INST_IRQHandler(void) {

    switch (DL_TimerA_getPendingInterrupt(ACK_PULSE_TIMER_INST)) {

        case DL_TIMER_IIDX_ZERO:
            DL_GPIO_clearPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);
            DL_TimerA_stopCounter(ACK_PULSE_TIMER_INST);
            _active = false;
            break;

        default:
            break;
    }

}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

    /**
     * @brief Clears the ACK pin and restores the default width (6000 us), enabled and idle state.
     */
void AckPulseDriver_initialize(void) {

    DL_GPIO_clearPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);
    _width_us = 6000;
    _enabled = true;
    _active = false;

}

    /**
     * @brief Stores the pulse width after clamping it to 1000-20000 us.
     *
     * @verbatim
     * @param width_us Requested pulse duration in microseconds.
     * @endverbatim
     */
void AckPulseDriver_set_width_us(uint32_t width_us) {

    if (width_us < 1000)
        width_us = 1000;

    if (width_us > 20000)
        width_us = 20000;

    _width_us = width_us;

}

    /**
     * @brief Returns the stored pulse width.
     *
     * @return Pulse width in microseconds.
     */
uint32_t AckPulseDriver_get_width_us(void) {

    return _width_us;

}

    /**
     * @brief Enables or disables ACK generation.
     *
     * @verbatim
     * @param enabled true = start() and fire() drive the pin, false = no-op.
     * @endverbatim
     */
void AckPulseDriver_set_enabled(bool enabled) {

    _enabled = enabled;

}

    /**
     * @brief Reports whether ACK generation is enabled.
     *
     * @return Current enabled flag.
     */
bool AckPulseDriver_is_enabled(void) {

    return _enabled;

}

    /**
     * @brief Sets the ACK pin HIGH unless ACK generation is disabled; the library times the 6 ms.
     */
void AckPulseDriver_start(void) {

    if (!_enabled)
        return;

    DL_GPIO_setPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);

}

    /**
     * @brief Clears the ACK pin LOW; called by the library when the 6 ms ACK window ends.
     */
void AckPulseDriver_stop(void) {

    DL_GPIO_clearPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);

}

    /**
     * @brief Starts a self-timed pulse: pin HIGH now, cleared by the one-shot timer after the stored width.
     *
     * @details No-op when disabled or while a previous pulse is still active. The timer runs at
     * 1 MHz, so the load value is width - 1.
     */
void AckPulseDriver_fire(void) {

    if (!_enabled)
        return;

    if (_active)
        return;

    _active = true;

    /* Set the ACK pin HIGH */
    DL_GPIO_setPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);

    /* Load the timer with the configured pulse width.
     * Timer runs at 1 MHz (prescale=40 from 40 MHz), so load value = width_us - 1. */
    DL_TimerA_setLoadValue(ACK_PULSE_TIMER_INST, (uint16_t)(_width_us - 1));
    DL_TimerA_startCounter(ACK_PULSE_TIMER_INST);

}

#endif /* DCC_COMPILE_DECODER */
