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
 * @file saleae_hil_compliance.c
 * @brief Waveform Player main entry point.
 *
 * @details MSPM0G3507 LaunchPad firmware for the generic, DCC-agnostic waveform player
 * (the shared HIL stimulus instrument). It receives (level, duration) segments
 * over UART and plays them on DCC_OUT via a one-shot, down-counting 1 MHz timer.
 * All DCC semantics live in the Python host (wfplayer.py). See SPEC.md /
 * PROTOCOL.md / IMPLEMENTATION.md in the parent folder.
 *
 *   main loop:  emit deferred "OK DONE"  +  parse one command line per pass
 *   ISRs:       PLAYBACK_TIMER (highest)  -> WfEngine playback step
 *               UART RX (mid)             -> ti_driverlib_uart_driver ring
 *               SysTick (lowest)          -> heartbeat LED ("firmware alive")
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

#include "application_drivers/ti_driverlib_uart_driver.h"
#include "wfplayer_engine.h"
#include "wfplayer_command_parser.h"

    /**
     * @brief SysTick ISR, every 100 ms: toggle LED1 every 5 ticks (~500 ms) as a heartbeat.
     */
void SysTick_Handler(void) {
    static uint8_t hb = 0;
    if (++hb >= 5) {
        hb = 0;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
    }
}

    /**
     * @brief Firmware entry: bring up the board, lower SysTick below the playback timer, then serve the host forever.
     *
     * @details PLAYBACK_TIMER is NVIC priority 0 from SysConfig and UART is 2; SysTick
     * is set to 3 so the heartbeat can never delay a playback step. Each loop pass
     * emits a deferred "OK DONE" when a finite play has completed, then parses one
     * command line. UART echo is intentionally not called: the host sees only OK/ERR lines.
     *
     * @return Never returns.
     */
int main(void) {

    SYSCFG_DL_init();

    /* The playback timer ISR must never be delayed by the heartbeat, so keep
     * SysTick below it. PLAYBACK_TIMER is priority 0 (highest) from SysConfig;
     * UART is 2; put SysTick at the lowest level. */
    NVIC_SetPriority(SysTick_IRQn, 3);

    NVIC_EnableIRQ(PLAYBACK_TIMER_INST_INT_IRQN);

    TI_UartDriver_initialize();
    WfEngine_initialize();
    WfCmdParser_initialize();

    TI_UartDriver_write_string("\r\nwfplayer v1 ready\r\n");

    while (1) {
        if (WfEngine_take_done())
            TI_UartDriver_write_string("OK DONE\r\n");

        WfCmdParser_process();   /* echo off: only OK/ERR lines go to the host */
    }
}
