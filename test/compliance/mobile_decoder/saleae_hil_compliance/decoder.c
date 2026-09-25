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
 * @file decoder.c
 * @brief Main entry point for the DCC Decoder demo.
 *
 * @details This demo receives DCC packets on a GPIO pin, decodes them using the
 * DCC library, and prints the decoded commands over UART.  It shows how
 * to wire together the library using the dcc_config_t configuration struct.
 *
 * To build your own decoder, copy this project and modify:
 *   - application_callbacks/callbacks_dcc.c  (your motor/LED/servo logic)
 *   - application_drivers/                   (your MCU's timer/GPIO/UART)
 *   - dcc_user_config.h                      (feature flags and limits)
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

#include "dcc_lib/dcc_config.h"

#include "application_drivers/ti_driverlib_dcc_driver.h"
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "application_drivers/ack_pulse_driver.h"
#include "application_callbacks/callbacks_dcc.h"
#include "decoder_command_parser.h"

/* ========================================================================== */
/* DCC library configuration (function pointer wiring)                        */
/*                                                                            */
/* The dcc_config_t struct uses function pointers for dependency injection.    */
/* You wire YOUR driver and callback functions here.  The library never calls  */
/* hardware directly -- it always calls through these pointers.               */
/*                                                                            */
/* REQUIRED fields must be non-NULL or the library will fault.                */
/* OPTIONAL callbacks can be NULL -- the library skips them if NULL.           */
/* ========================================================================== */

    /** @brief The DCC library configuration: platform drivers, CV storage, ACK pulse and the application callbacks this bench wires in. */
const dcc_config_t dcc_config = {

    /* --- Platform drivers (REQUIRED) ---
     * These three let the library disable interrupts and read timestamps.
     * Replace these when porting to a different MCU. */
    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,
    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,
    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,

    /* --- CV storage (REQUIRED) ---
     * The library reads and writes Configuration Variables through these.
     * This demo uses a RAM array stub; replace with Flash or EEPROM for
     * a real product (see callbacks_dcc.c). */
    .cv_read                 = &CallbacksDcc_cv_read,
    .cv_write                = &CallbacksDcc_cv_write,
    .factory_reset           = &CallbacksDcc_factory_reset,
    .cv_read_indexed         = &CallbacksDcc_cv_read_indexed,
    .cv_write_indexed        = &CallbacksDcc_cv_write_indexed,
    .cv29_apply_supported_features = &CallbacksDcc_cv29_apply_supported_features,

    /* --- RailCom Tx hardware (REQUIRED as a group if supporting RailCom;
     *     both NULL = no RailCom Tx) ---
     * The library bit-bangs the cutout reply: railcom_tx_pin_set drives the
     * current source and railcom_delay_us provides the cycle-accurate 4 us bit
     * timing. lock_shared_resources masks the DCC edge IRQ during the cutout,
     * so the decoder's injected current cannot self-trigger it. */
    .railcom_tx_pin_set            = NULL,
    .railcom_delay_us              = &TI_DccDriver_railcom_delay_us,

    /* --- ACK pulse hardware (service-mode acknowledge; NULL = no ACK) --- */
    .start_ack_pulse               = &AckPulseDriver_start,
    .stop_ack_pulse                = &AckPulseDriver_stop,

    /* --- Application callbacks (OPTIONAL, NULL = no notification) ---
     * Each of these fires when the library decodes the corresponding DCC
     * command.  Set any to NULL if you do not need that command type.
     *
     * NOTE: Callbacks run from main-loop context (the ISR only captures
     * timestamps; the main loop drains them into the bit decoder). */

    .on_speed_command              = &CallbacksDcc_on_speed_command,
    .on_emergency_stop_command     = &CallbacksDcc_on_emergency_stop,
    .on_function_command           = &CallbacksDcc_on_function_command,
    .on_accessory_basic_command    = &CallbacksDcc_on_accessory_basic_command,
    .on_accessory_extended_command = &CallbacksDcc_on_accessory_extended_command,
    .on_cv_write_command           = &CallbacksDcc_on_cv_write,
    .on_cv_verify_command          = &CallbacksDcc_on_cv_verify,
    .on_cv_bit_command             = &CallbacksDcc_on_cv_bit,
    .on_consist_command            = &CallbacksDcc_on_consist_command,
    .on_binary_state_short_command = &CallbacksDcc_on_binary_state_short,
    .on_binary_state_long_command  = &CallbacksDcc_on_binary_state_long,
    .on_analog_function_command    = &CallbacksDcc_on_analog_function,
    .on_failsafe_entered           = &CallbacksDcc_on_failsafe_entered,
    .on_failsafe_exited            = &CallbacksDcc_on_failsafe_exited,
};

/* ========================================================================== */
/* Edge timestamp ring buffer                                                 */
/*                                                                            */
/* The GPIO ISR captures only a timestamp and stuffs it here.  The main loop  */
/* drains the buffer, checks the track-select mux, and feeds edges into the   */
/* DCC bit decoder.  This keeps the ISR under ~1 us so it never overruns the  */
/* 58 us half-bit period.                                                     */
/* ========================================================================== */

    /** @brief Edge timestamp ring depth; must be a power of 2. */
#define EDGE_BUF_SIZE 256           /* must be power of 2 */
    /** @brief Index wrap mask for the edge ring. */
#define EDGE_BUF_MASK (EDGE_BUF_SIZE - 1)

    /** @brief Edge timestamps captured by the GPIO ISR. */
static volatile uint32_t _edge_buf[EDGE_BUF_SIZE];
    /** @brief Ring write index; written by the ISR only. */
static volatile uint16_t _edge_head;   /* written by ISR only  */
    /** @brief Ring read index; written by the main loop only. */
static volatile uint16_t _edge_tail;   /* read by main loop only */

/* ========================================================================== */
/* ISR handlers                                                               */
/* ========================================================================== */

    /**
     * @brief DCC input edge ISR: DCC_IN (PB1) on GPIOB (GROUP1 interrupt).
     *
     * @details Captures a timestamp into the edge ring (dropping it if the ring is
     * full) and clears the interrupt, nothing else, so the ISR stays well under the
     * 58 us half-bit period. The main loop feeds the edges to the bit decoder.
     */
void GROUP1_IRQHandler(void) {

    uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOB,
        GPIO_GRP_SALEAE_DCC_IN_PIN);

    if (status & GPIO_GRP_SALEAE_DCC_IN_PIN) {
        uint32_t ts = TI_DccDriver_get_timestamp_usec();
        uint16_t next = (_edge_head + 1) & EDGE_BUF_MASK;
        if (next != _edge_tail) {
            _edge_buf[_edge_head] = ts;
            _edge_head = next;
        }
    }

    DL_GPIO_clearInterruptStatus(GPIOB, GPIO_GRP_SALEAE_DCC_IN_PIN);
}

    /**
     * @brief SysTick ISR, every 100 ms: toggle LED1 every 5 ticks (500 ms) as a heartbeat.
     */
void SysTick_Handler(void) {

    static uint8_t heartbeat_count = 0;

    heartbeat_count++;
    if (heartbeat_count >= 5) {

        heartbeat_count = 0;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);

    }

}

/* ========================================================================== */
/* Edge buffer drain — called from main loop                                  */
/* ========================================================================== */

    /**
     * @brief Drain all pending edge timestamps from the ISR ring and feed them to the DCC bit decoder.
     *
     * @details Called from the main loop. Each timestamp goes to
     * DccConfig_decoder_edge_isr() in capture order; nothing else is done per edge.
     */
static void _drain_edge_buffer(void) {

    while (_edge_tail != _edge_head) {

        uint32_t ts = _edge_buf[_edge_tail];
        _edge_tail = (_edge_tail + 1) & EDGE_BUF_MASK;

        DccConfig_decoder_edge_isr(ts);

    }

}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

    /**
     * @brief Firmware entry: bring up the board, hand the config to the library, then run the main loop forever.
     *
     * @details Initialization order matters:
     * -# SYSCFG_DL_init(): clocks, GPIO, timers, UART (generated).
     * -# NVIC_EnableIRQ(...): the GPIOB edge, timestamp and ACK-timer interrupts SysConfig does not enable.
     * -# TI_DccDriver_initialize(): start the free-running timestamp timer.
     * -# TI_UartDriver_initialize(): enable the UART RX interrupt.
     * -# AckPulseDriver_initialize(): park the ACK pin low.
     * -# CallbacksDcc_initialize(): clear the RECV ring, set CV defaults (CV1 = 3, CV29 = 0x06).
     * -# DccConfig_initialize(): hand the config struct to the library, which reads CV1/CV29 for the address.
     * -# DecoderCommandParser_initialize(), then the banner.
     *
     * The main loop then repeats five non-blocking calls: _drain_edge_buffer(),
     * DccConfig_run() (decodes and dispatches packets), CallbacksDcc_drain(),
     * TI_UartDriver_echo_process() and DecoderCommandParser_process().
     *
     * @return Never returns.
     */
int main(void) {

    /* SysConfig-generated device initialization (clocks, GPIO, timers, UART) */
    SYSCFG_DL_init();

    /* Enable NVIC interrupts not enabled by SysConfig */
    NVIC_EnableIRQ(GPIOB_INT_IRQn);
    NVIC_EnableIRQ(TIMESTAMP_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(ACK_PULSE_TIMER_INST_INT_IRQN);

    /* Initialize hardware drivers */
    TI_DccDriver_initialize();
    TI_UartDriver_initialize();
    AckPulseDriver_initialize();

    /* Initialize decoder subsystems */
    CallbacksDcc_initialize();
    DccConfig_initialize(&dcc_config);

    /* Default decoder address is short address 3, set via CV defaults in
     * CallbacksDcc_initialize() (CV1=3, CV29=0x06).  The library reads
     * these CVs during DccConfig_initialize() above. */

    /* Initialize command parser */
    DecoderCommandParser_initialize();

    /* Startup banner */
    TI_UartDriver_write_string("\r\n");
    TI_UartDriver_write_string("DCC Decoder - MSPM0G3507 LaunchPad\r\n");
    TI_UartDriver_write_string("Type HELP for available commands.\r\n");
    TI_UartDriver_write_string("> ");

    /* Main loop -- all five calls are non-blocking */
    while (1) {

        _drain_edge_buffer();
        DccConfig_run();
        CallbacksDcc_drain();
        TI_UartDriver_echo_process();
        DecoderCommandParser_process();

    }

}
