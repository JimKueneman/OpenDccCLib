// ti_driverlib_dcc_driver.c
//
// Reference implementation of the DCC hardware drivers for MSPM0G3507.
//
// If you are porting to a different MCU, use this file as a template.
// Replace the TI DriverLib calls with your MCU's equivalents. The key
// contracts each function must fulfill are documented in the header.
//
// TIMESTAMP PATTERN:
// The microsecond timestamp is derived from the shared 58us DCC timer.
// TI_DccDriver_timestamp_tick() is called from the shared timer ISR and
// increments a software counter. get_timestamp_usec() multiplies by 58
// to return approximate microseconds. Resolution is 58us, which is
// sufficient for timeout calculations.

#include "ti_driverlib_dcc_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

/* Tick counter for software microsecond timestamp.
 * Incremented every 58us from the shared DCC timer ISR via
 * TI_DccDriver_timestamp_tick(). */
static volatile uint32_t _timestamp_ticks = 0;

void TI_DccDriver_initialize(void) {

    _timestamp_ticks = 0;
}

void TI_DccDriver_lock_shared_resources(void) {

    __disable_irq();
}

void TI_DccDriver_unlock_shared_resources(void) {

    __enable_irq();
}

uint32_t TI_DccDriver_get_timestamp_usec(void) {

    /* Each tick is 58us (DCC_ONE_BIT_HALF_PERIOD_US). Multiply to get
     * approximate microseconds. Atomic 32-bit read on Cortex-M0+. */
    return _timestamp_ticks * 58u;
}

void TI_DccDriver_timer_start(uint16_t half_bit_period_usec) {

    /* Counter counts load..0 inclusive, so the period is (load + 1) ticks.
     * At 1 MHz (1 tick = 1us) the load value must be period_us - 1. */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, half_bit_period_usec - 1u);
    DL_TimerA_enableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(DCC_BIT_TIMER_INST);
}

void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec) {

    /* Single register write — safe to call from ISR context.
     * load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, half_bit_period_usec - 1u);
}

void TI_DccDriver_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

void TI_DccDriver_track_power_set(bool enabled) {

    if (enabled) {
        DL_GPIO_setPins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
    }
}

uint16_t TI_DccDriver_current_sense_read(void) {

    /* Read the ACK sense pin (PB12).  Returns 100 (above threshold) when
     * the decoder board is asserting its ACK GPIO, 0 otherwise. */
    uint32_t pins = DL_GPIO_readPins(GPIO_ACK_SENSE_PORT,
                                      GPIO_ACK_SENSE_ACK_IN_PIN);
    return (pins & GPIO_ACK_SENSE_ACK_IN_PIN) ? 100 : 0;
}

void TI_DccDriver_shared_timer_start(uint16_t period_usec) {

    /* load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, period_usec - 1u);
    DL_TimerA_enableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(DCC_BIT_TIMER_INST);
}

void TI_DccDriver_shared_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

void TI_DccDriver_railcom_timer_start(uint16_t period_usec) {

    /* load = period_us - 1 (period is load + 1 ticks at 1 MHz).
     * All cutout state delays are non-zero, so this never underflows. */
    DL_TimerA_setLoadValue(RAILCOM_TIMER_INST, period_usec - 1u);
    DL_TimerA_enableInterrupt(RAILCOM_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(RAILCOM_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(RAILCOM_TIMER_INST);
}

void TI_DccDriver_railcom_timer_stop(void) {

    DL_TimerA_stopCounter(RAILCOM_TIMER_INST);
    DL_TimerA_disableInterrupt(RAILCOM_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(RAILCOM_TIMER_INST_INT_IRQN);
}

void TI_DccDriver_main_pin_toggle(void) {

    /* Continuous DCC: the encoder never stalls, so we always toggle PB1. The DCC
     * line is IDENTICAL whether RailCom is on or off. The cutout is a separate
     * signal (begin/end below) that real H-bridge hardware would mux on to
     * tristate the track -- blanking is hardware's job, not ours. */
    DL_GPIO_togglePins(GPIO_DCC_PORT, GPIO_DCC_DCC_SIGNAL_PIN);
}

/* Cutout-active signal (T_CS): raise PB2. In a real station this gates the
 * H-bridge into the cutout (tristate). Here it is the Saleae cutout-window marker. */
void TI_DccDriver_main_cutout_begin(void) {

    DL_GPIO_setPins(GPIO_DCC_MIRROR_PORT, GPIO_DCC_MIRROR_DCC_MIRROR_PIN);
}

/* Cutout-active signal (T_CE): drop PB2 -- the H-bridge resumes driving the track. */
void TI_DccDriver_main_cutout_end(void) {

    DL_GPIO_clearPins(GPIO_DCC_MIRROR_PORT, GPIO_DCC_MIRROR_DCC_MIRROR_PIN);
}

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

void TI_DccDriver_svc_pin_toggle(void) {

    DL_GPIO_togglePins(GPIO_SERVICE_MODE_DCC_PORT,
                       GPIO_SERVICE_MODE_DCC_SERVICE_MODE_DCC_SIGNAL_PIN);
    /* PB2 (DCC_MIRROR) is now the RailCom cutout marker, not a DCC mirror. */
}

void TI_DccDriver_timestamp_tick(void) {

    _timestamp_ticks++;
}
