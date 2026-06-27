// saleae_hil_compliance.c -- Waveform Player main entry point.
//
// MSPM0G3507 LaunchPad firmware for the generic, DCC-agnostic waveform player
// (the shared HIL stimulus instrument). It receives (level, duration) segments
// over UART and plays them on DCC_OUT via a one-shot, down-counting 1 MHz timer.
// All DCC semantics live in the Python host (wfplayer.py). See SPEC.md /
// PROTOCOL.md / IMPLEMENTATION.md in the parent folder.
//
//   main loop:  emit deferred "OK DONE"  +  parse one command line per pass
//   ISRs:       PLAYBACK_TIMER (highest)  -> WfEngine playback step
//               UART RX (mid)             -> ti_driverlib_uart_driver ring
//               SysTick (lowest)          -> heartbeat LED ("firmware alive")

#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

#include "application_drivers/ti_driverlib_uart_driver.h"
#include "wfplayer_engine.h"
#include "wfplayer_command_parser.h"

/* SysTick -- 100 ms tick. Heartbeat: toggle LED1 every 5 ticks (~500 ms). */
void SysTick_Handler(void) {
    static uint8_t hb = 0;
    if (++hb >= 5) {
        hb = 0;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
    }
}

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
