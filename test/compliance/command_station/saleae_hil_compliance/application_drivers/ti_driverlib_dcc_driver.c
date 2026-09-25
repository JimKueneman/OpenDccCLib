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
#include "ti_driverlib_railcom_loopback.h"   /* HIL RailCom loopback hooks */
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

    /** @brief Tick counter for the software microsecond timestamp; incremented every 58 us by TI_DccDriver_timestamp_tick(). */
static volatile uint32_t _timestamp_ticks = 0;

    /** @brief One-time driver setup: zero the timestamp counter. */
void TI_DccDriver_initialize(void) {

    _timestamp_ticks = 0;
}

    /** @brief Disable all interrupts (library critical-section enter). */
void TI_DccDriver_lock_shared_resources(void) {

    __disable_irq();
}

    /** @brief Re-enable interrupts (library critical-section exit). */
void TI_DccDriver_unlock_shared_resources(void) {

    __enable_irq();
}

    /**
     * @brief Free-running microsecond timestamp at 58 us resolution.
     *
     * @return Tick count times 58 (an atomic 32-bit read on Cortex-M0+).
     */
uint32_t TI_DccDriver_get_timestamp_usec(void) {

    /* Each tick is 58us (DCC_ONE_BIT_HALF_PERIOD_US). Multiply to get
     * approximate microseconds. Atomic 32-bit read on Cortex-M0+. */
    return _timestamp_ticks * 58u;
}

    /**
     * @brief Start the per-channel bit timer (reference only, not wired on this bench).
     *
     * @details The counter counts load..0 inclusive, so at 1 MHz the load value is
     * period - 1. Enables the ZERO interrupt and the NVIC line, then starts counting.
     *
     * @verbatim
     * @param half_bit_period_usec  Half-bit period in microseconds.
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
     * @brief Change the bit-timer period: a single load-register write, ISR-safe (reference only).
     *
     * @verbatim
     * @param half_bit_period_usec  Next half-bit period in microseconds.
     * @endverbatim
     */
void TI_DccDriver_timer_set_period(uint16_t half_bit_period_usec) {

    /* Single register write — safe to call from ISR context.
     * load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, half_bit_period_usec - 1u);
}

    /** @brief Stop the per-channel bit timer and disable its interrupt (reference only). */
void TI_DccDriver_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Main-track power: set the idle level of the main DCC pin (PB1).
     *
     * @verbatim
     * @param enabled  true = pin high, false = pin low.
     * @endverbatim
     */
void TI_DccDriver_track_power_set(bool enabled) {

    if (enabled) {
        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
    }
}

    /**
     * @brief Read MOCK_ACK (PB9) as the service-track current-sense level.
     *
     * @details On the HIL bench MOCK_ACK_DRIVE (PB24) is jumpered to MOCK_ACK (PB9), so
     * the firmware-generated mock pulse is read back here as the "real" ACK level.
     *
     * @return 100 (above the ACK threshold) when the pin is high, 0 otherwise.
     */
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

    /** @brief Fixed DCC bit-timer ISR period in microseconds; mock pulse widths are quantised to it. */
#define MOCK_ACK_TICK_US 58u   /* fixed DCC bit-timer ISR period */

    /** @brief Armed pulse width in ticks; 0 = not armed. */
static volatile uint16_t _mock_ack_width_ticks = 0;   /* armed width; 0 = not armed */
    /** @brief High-phase countdown in ticks; the drive pin is high while > 0. */
static volatile uint16_t _mock_ack_remaining   = 0;   /* high-phase countdown; >0 = high */

/* Optional GLITCH (interrupted-pulse) extension: after the first high phase, drop
 * low for _gap, then go high again for _post. Proves the library's ACK width
 * counter RESETS on the interruption (S-9.2.3 CS-005) -- two sub-pulses each
 * shorter than the 6 ms window, whose sum is in-window only if the counter
 * (wrongly) accumulated across the gap. _ticks = armed (latched by on_command);
 * _run = live countdown in tick(). All zero = a plain single pulse (unchanged). */
    /** @brief Armed GLITCH low-gap length in ticks (latched by on_command). */
static volatile uint16_t _mock_ack_gap_ticks   = 0;
    /** @brief Armed GLITCH second high phase in ticks (latched by on_command). */
static volatile uint16_t _mock_ack_post_ticks  = 0;
    /** @brief Live GLITCH low-gap countdown driven by tick(). */
static volatile uint16_t _mock_ack_gap_run     = 0;
    /** @brief Live GLITCH second-high-phase length, started when the gap countdown ends. */
static volatile uint16_t _mock_ack_post_run    = 0;

    /**
     * @brief Arm a one-shot single mock-ACK pulse; fired by on_command() at the next service command.
     *
     * @verbatim
     * @param width_us  Pulse width in microseconds (rounded down to 58 us ticks).
     * @endverbatim
     */
void TI_DccDriver_mock_ack_arm(uint16_t width_us) {

    /* Arm a one-shot single pulse. Fired by on_command() at the next service command. */
    _mock_ack_width_ticks = (uint16_t)(width_us / MOCK_ACK_TICK_US);
    _mock_ack_remaining = 0;
    _mock_ack_gap_ticks = 0;
    _mock_ack_post_ticks = 0;
}

    /**
     * @brief Arm an INTERRUPTED mock-ACK pulse: high pre_us, low gap_us, high post_us.
     *
     * @details Fired by on_command() like the plain pulse; tick() inserts the low gap
     * between the two high phases.
     *
     * @verbatim
     * @param pre_us   First high phase in microseconds.
     * @param gap_us   Low gap in microseconds.
     * @param post_us  Second high phase in microseconds.
     * @endverbatim
     */
void TI_DccDriver_mock_ack_arm_glitch(uint16_t pre_us, uint16_t gap_us, uint16_t post_us) {

    /* Arm an INTERRUPTED pulse: high pre_us, low gap_us, high post_us. Fired by
     * on_command() like the plain pulse; tick() inserts the low gap between them. */
    _mock_ack_width_ticks = (uint16_t)(pre_us / MOCK_ACK_TICK_US);
    _mock_ack_gap_ticks   = (uint16_t)(gap_us / MOCK_ACK_TICK_US);
    _mock_ack_post_ticks  = (uint16_t)(post_us / MOCK_ACK_TICK_US);
    _mock_ack_remaining = 0;
    _mock_ack_gap_run = 0;
    _mock_ack_post_run = 0;
}

    /**
     * @brief Start the armed pulse when a service-mode command packet is dispatched.
     *
     * @details If armed, begin the pulse now (so it lands inside the library COMMAND-state
     * ACK-sample window), latch any armed glitch gap/post into the live countdowns, and
     * disarm so it fires exactly once.
     */
void TI_DccDriver_mock_ack_on_command(void) {

    /* Called when a service-mode command packet starts transmitting. If armed,
     * begin the pulse now (so it lands inside the common module's COMMAND-state
     * ACK-sample window) and disarm so it fires exactly once. Latches any armed
     * glitch (gap/post) into the live countdowns too. */
    if (_mock_ack_width_ticks > 0) {

        _mock_ack_remaining = _mock_ack_width_ticks;
        _mock_ack_gap_run   = _mock_ack_gap_ticks;
        _mock_ack_post_run  = _mock_ack_post_ticks;
        _mock_ack_width_ticks = 0;
        _mock_ack_gap_ticks = 0;
        _mock_ack_post_ticks = 0;
    }
}

    /**
     * @brief Immediately start a single mock-ACK pulse of the given width.
     *
     * @details Used by the mock decoder to ACK a verify command the instant a value
     * match is detected; the width-test arm/on_command path is left untouched.
     *
     * @verbatim
     * @param width_us  Pulse width in microseconds (rounded down to 58 us ticks).
     * @endverbatim
     */
void TI_DccDriver_mock_ack_fire(uint16_t width_us) {

    /* Immediately start a single pulse of the given width. Used by the mock decoder
     * to ACK a verify command the instant a value match is detected (the width-test
     * arm/on_command path above is left untouched). */
    _mock_ack_remaining = (uint16_t)(width_us / MOCK_ACK_TICK_US);
    _mock_ack_gap_run = 0;
    _mock_ack_post_run = 0;
}

    /**
     * @brief 58 us ISR tick for the mock-ACK pulse; call BEFORE the library reads current_sense_read().
     *
     * @details Phase order: high (remaining) -> low gap (gap_run) -> high (post_run) -> low.
     * When the gap countdown reaches zero the second high phase is loaded from post_run.
     */
void TI_DccDriver_mock_ack_tick(void) {

    /* Called every 58 us ISR, BEFORE the library reads current_sense_read().
     * Phase order: high (remaining) -> low gap (gap_run) -> high (post_run) -> low. */
    if (_mock_ack_remaining > 0) {

        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_DRIVE_PIN);
        _mock_ack_remaining--;

    } else if (_mock_ack_gap_run > 0) {

        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_DRIVE_PIN);
        _mock_ack_gap_run--;
        if (_mock_ack_gap_run == 0 && _mock_ack_post_run > 0) {
            _mock_ack_remaining = _mock_ack_post_run;   /* begin the 2nd high phase */
            _mock_ack_post_run = 0;
        }

    } else {

        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MOCK_ACK_DRIVE_PIN);
    }
}

    /**
     * @brief Start the shared 58 us DCC timer: load = period - 1 at 1 MHz, enable ZERO IRQ, start.
     *
     * @verbatim
     * @param period_usec  Timer period in microseconds.
     * @endverbatim
     */
void TI_DccDriver_shared_timer_start(uint16_t period_usec) {

    /* load = period_us - 1 (period is load + 1 ticks at 1 MHz). */
    DL_TimerA_setLoadValue(DCC_BIT_TIMER_INST, period_usec - 1u);
    DL_TimerA_enableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_EnableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
    DL_TimerA_startCounter(DCC_BIT_TIMER_INST);
}

    /** @brief Stop the shared DCC timer and disable its interrupt. */
void TI_DccDriver_shared_timer_stop(void) {

    DL_TimerA_stopCounter(DCC_BIT_TIMER_INST);
    DL_TimerA_disableInterrupt(DCC_BIT_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(DCC_BIT_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Start the RailCom cutout one-shot timer: load = period - 1 at 1 MHz, enable ZERO IRQ, start.
     *
     * @details All cutout state delays are non-zero, so the subtraction never underflows.
     *
     * @verbatim
     * @param period_usec  One-shot period in microseconds.
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

    /** @brief Stop the RailCom cutout one-shot timer and disable its interrupt. */
void TI_DccDriver_railcom_timer_stop(void) {

    DL_TimerA_stopCounter(RAILCOM_TIMER_INST);
    DL_TimerA_disableInterrupt(RAILCOM_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    NVIC_DisableIRQ(RAILCOM_TIMER_INST_INT_IRQN);
}

    /**
     * @brief Toggle the main-track DCC pin (PB1).
     *
     * @details Continuous DCC: the encoder never stalls, so this always toggles. The
     * DCC line is IDENTICAL whether RailCom is on or off; the cutout is a separate
     * signal (begin/end below) that real H-bridge hardware would mux on to tristate
     * the track. Blanking is the hardware job, not the library one.
     */
void TI_DccDriver_main_pin_toggle(void) {

    /* Continuous DCC: the encoder never stalls, so we always toggle PB1. The DCC
     * line is IDENTICAL whether RailCom is on or off. The cutout is a separate
     * signal (begin/end below) that real H-bridge hardware would mux on to
     * tristate the track -- blanking is hardware's job, not ours. */
    DL_GPIO_togglePins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_MAIN_DCC_PIN);
}

    /**
     * @brief Cutout-active signal (T_CS): raise the PB2 cutout strobe and notify the loopback.
     *
     * @details In a real station this gates the H-bridge into the cutout (tristate);
     * here it is the Saleae cutout-window marker. Also flushes the loopback receive
     * ring and arms its window count.
     */
void TI_DccDriver_main_cutout_begin(void) {

    DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_CUTOUT_PIN);
    TI_RailcomLoopback_on_cutout_begin();    /* flush the receive ring, arm the window count */
}

    /**
     * @brief Cutout-active signal (T_CE): drop the PB2 cutout strobe and notify the loopback.
     *
     * @details The loopback closes its receiver gate here; a LATE-mode mock reply
     * transmits at this point.
     */
void TI_DccDriver_main_cutout_end(void) {

    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_CUTOUT_PIN);
    TI_RailcomLoopback_on_cutout_end();      /* gate closed; LATE-mode mock fires here */
}

    /**
     * @brief Channel-window marker: raise RAILCOM_RX_WINDOW (PB18) and open the loopback receiver gate.
     *
     * @details The cutout state machine calls uart_rx_enable when a RailCom channel
     * window OPENS (T_TS1 Ch1, T_TS2 Ch2) and uart_rx_disable when it CLOSES (T_TC1
     * Ch1, T_CE Ch2). Mirroring those to a probe pin makes the interior sub-windows
     * externally visible: combined with the PB2 cutout strobe (T_CS / T_CE) the Saleae
     * times all five states (S-9.3.2 CS-005/006). The pin write is compiled only when
     * SysConfig defines GPIO_GRP_SALEAE_RAILCOM_RX_WINDOW_PIN; the loopback gate opens
     * either way, and a WINDOW-mode mock reply starts transmitting here.
     */
void TI_DccDriver_railcom_window_open(void) {

#ifdef GPIO_GRP_SALEAE_RAILCOM_RX_WINDOW_PIN
    DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_RX_WINDOW_PIN);
#endif
    TI_RailcomLoopback_on_window_open();     /* receiver gate open; WINDOW-mode mock transmits */
}

    /**
     * @brief Channel-window marker: drop RAILCOM_RX_WINDOW (PB18) and close the loopback receiver gate.
     */
void TI_DccDriver_railcom_window_close(void) {

#ifdef GPIO_GRP_SALEAE_RAILCOM_RX_WINDOW_PIN
    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_RAILCOM_RX_WINDOW_PIN);
#endif
    TI_RailcomLoopback_on_window_close();    /* receiver gate closed */
}

    /** @brief Toggle the service-track DCC pin (PB4). */
void TI_DccDriver_svc_pin_toggle(void) {

    DL_GPIO_togglePins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_SERVICE_MODE_DCC_PIN);

}

    /**
     * @brief Service-track power: set the idle level of the service-track DCC pin (PB4).
     *
     * @details Same convention as the main track: the bench has no H-bridge, so power
     * is the idle level of the pin.
     *
     * @verbatim
     * @param enabled  true = pin high, false = pin low.
     * @endverbatim
     */
void TI_DccDriver_svc_track_power_set(bool enabled) {

    /* Same convention as the main track: the bench has no H-bridge, so power
     * is the idle level of the service-track DCC pin. */
    if (enabled) {
        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_SERVICE_MODE_DCC_PIN);
    } else {
        DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_SERVICE_MODE_DCC_PIN);
    }

}

    /** @brief Increment the software timestamp counter (shared DCC timer ISR, every 58 us). */
void TI_DccDriver_timestamp_tick(void) {

    _timestamp_ticks++;
}
