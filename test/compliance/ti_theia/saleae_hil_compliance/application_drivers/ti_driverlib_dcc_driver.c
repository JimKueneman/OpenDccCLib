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
        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
    }
}

uint16_t TI_DccDriver_current_sense_read(void) {

    /* Read the MOCK_ACK pin (PB9). On the HIL bench MOCK_ACK_DRIVE (PB24) is
     * jumpered to MOCK_ACK (PB9), so the firmware-generated mock pulse is read
     * back here as the "real" ACK current-sense level. Returns 100 (above the
     * ACK threshold) when high, 0 otherwise. */
    uint32_t pins = DL_GPIO_readPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_PIN);
    return (pins & GPIO_GRP_SALEAE_MOCK_ACK_PIN) ? 100 : 0;
}

/* =========================================================================
 * Mock ACK loopback (HIL only)
 *
 * Drive MOCK_ACK_DRIVE (PB24) high for a controlled number of 58 us ISR ticks,
 * then low. Wired to MOCK_ACK (PB9) by a board jumper, so the library's ACK
 * width counter (in dcc_service_mode_common) reads it back through
 * current_sense_read() and validates the 6 ms +/- 1 ms window. Lets the s9_2_3
 * suite test ACK detection electrically with no real decoder.
 *
 *   arm(width_us)  -> records the pulse width (in 58 us ticks)
 *   on_command()   -> when a service-mode command packet is sent, start the pulse
 *   tick()         -> called every 58 us ISR; drives the pulse high then low
 * ========================================================================= */

#define MOCK_ACK_TICK_US 58u   /* fixed DCC bit-timer ISR period */

static volatile uint16_t _mock_ack_width_ticks = 0;   /* armed width; 0 = not armed */
static volatile uint16_t _mock_ack_remaining   = 0;   /* pulse countdown; >0 = high */

void TI_DccDriver_mock_ack_arm(uint16_t width_us) {

    /* Arm a one-shot pulse. Fired by on_command() at the next service command. */
    _mock_ack_width_ticks = (uint16_t)(width_us / MOCK_ACK_TICK_US);
    _mock_ack_remaining = 0;
}

void TI_DccDriver_mock_ack_on_command(void) {

    /* Called when a service-mode command packet starts transmitting. If armed,
     * begin the pulse now (so it lands inside the common module's COMMAND-state
     * ACK-sample window) and disarm so it fires exactly once. */
    if (_mock_ack_width_ticks > 0) {

        _mock_ack_remaining = _mock_ack_width_ticks;
        _mock_ack_width_ticks = 0;
    }
}

void TI_DccDriver_mock_ack_fire(uint16_t width_us) {

    /* Immediately start a pulse of the given width. Used by the mock decoder to
     * ACK a verify command the instant a value match is detected (the width-test
     * arm/on_command path above is left untouched). */
    _mock_ack_remaining = (uint16_t)(width_us / MOCK_ACK_TICK_US);
}

void TI_DccDriver_mock_ack_tick(void) {

    /* Called every 58 us ISR, BEFORE the library reads current_sense_read(). */
    if (_mock_ack_remaining > 0) {

        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_DRIVE_PIN);
        _mock_ack_remaining--;

    } else {

        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_DRIVE_PIN);
    }
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
    DL_GPIO_togglePins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
}

/* Cutout-active signal (T_CS): raise PB2. In a real station this gates the
 * H-bridge into the cutout (tristate). Here it is the Saleae cutout-window marker. */
void TI_DccDriver_main_cutout_begin(void) {

    DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_CUTOUT_PIN);
}

/* Cutout-active signal (T_CE): drop PB2 -- the H-bridge resumes driving the track. */
void TI_DccDriver_main_cutout_end(void) {

    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_CUTOUT_PIN);
}

void TI_DccDriver_svc_pin_toggle(void) {

    DL_GPIO_togglePins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_SERVICE_MODE_DCC_PIN);

}

void TI_DccDriver_timestamp_tick(void) {

    _timestamp_ticks++;
}
