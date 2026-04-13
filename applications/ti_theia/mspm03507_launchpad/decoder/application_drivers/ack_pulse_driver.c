/*
 * ack_pulse_driver.c -- GPIO-based ACK pulse generator for service mode testing.
 *
 * Uses a one-shot hardware timer (TIMA1) to produce a precise pulse on PB12.
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
 */

#include "ack_pulse_driver.h"

#ifdef DCC_COMPILE_DECODER

#include "ti_msp_dl_config.h"

/* ========================================================================== */
/* Static state                                                               */
/* ========================================================================== */

static uint32_t _width_us = 6000;
static bool _enabled = true;
static volatile bool _active = false;

/* ========================================================================== */
/* Timer ISR — clears the ACK pin when the one-shot timer expires             */
/* ========================================================================== */

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

void AckPulseDriver_initialize(void) {

    DL_GPIO_clearPins(GPIO_ACK_PORT, GPIO_ACK_ACK_OUT_PIN);
    _width_us = 6000;
    _enabled = true;
    _active = false;

}

void AckPulseDriver_set_width_us(uint32_t width_us) {

    if (width_us < 1000)
        width_us = 1000;

    if (width_us > 20000)
        width_us = 20000;

    _width_us = width_us;

}

uint32_t AckPulseDriver_get_width_us(void) {

    return _width_us;

}

void AckPulseDriver_set_enabled(bool enabled) {

    _enabled = enabled;

}

bool AckPulseDriver_is_enabled(void) {

    return _enabled;

}

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
