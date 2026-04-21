/*
 * decoder.c -- Main entry point for the DCC Decoder demo.
 *
 * This demo receives DCC packets on a GPIO pin, decodes them using the
 * DCC library, and prints the decoded commands over UART.  It shows how
 * to wire together the library using the dcc_config_t configuration struct.
 *
 * To build your own decoder, copy this project and modify:
 *   - application_callbacks/callbacks_dcc.c  (your motor/LED/servo logic)
 *   - application_drivers/                   (your MCU's timer/GPIO/UART)
 *   - dcc_user_config.h                      (feature flags and limits)
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

    /* --- RailCom UART (set to NULL if not used) --- */
    .railcom_uart_write      = NULL,

    /* --- Application callbacks (OPTIONAL, NULL = no notification) ---
     * Each of these fires when the library decodes the corresponding DCC
     * command.  Set any to NULL if you do not need that command type.
     *
     * NOTE: Callbacks run from main-loop context (the ISR only captures
     * timestamps; the main loop drains them into the bit decoder). */
    .railcom_tx_pin_set            = NULL,
    .decoder_edge_irq_enable       = NULL,

    .start_ack_pulse               = &AckPulseDriver_start,
    .stop_ack_pulse                = &AckPulseDriver_stop,

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
    .on_speed_restriction_command  = &CallbacksDcc_on_speed_restriction,
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

#define EDGE_BUF_SIZE 256           /* must be power of 2 */
#define EDGE_BUF_MASK (EDGE_BUF_SIZE - 1)

static volatile uint32_t _edge_buf[EDGE_BUF_SIZE];
static volatile uint16_t _edge_head;   /* written by ISR only  */
static volatile uint16_t _edge_tail;   /* read by main loop only */

/* ========================================================================== */
/* ISR handlers                                                               */
/* ========================================================================== */

/* DCC input edge ISR -- PB1 (main) and PB4 (service) are both on GPIOB,
 * which uses the GROUP1 interrupt.  The ISR captures a timestamp into the
 * ring buffer and clears the interrupt — nothing else.  The main loop
 * decides which input is active and feeds edges to the bit decoder. */
void GROUP1_IRQHandler(void) {

    uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOB,
        GPIO_DCC_INPUT_DCC_IN_PIN | GPIO_DCC_SERVICE_INPUT_DCC_SERVICE_IN_PIN);

    bool track_sel = DL_GPIO_readPins(GPIO_TRACK_SELECT_PORT,
        GPIO_TRACK_SELECT_TRACK_SEL_PIN) & GPIO_TRACK_SELECT_TRACK_SEL_PIN;

    bool accept = false;
    if (!track_sel && (status & GPIO_DCC_INPUT_DCC_IN_PIN)) {
        accept = true;
    } else if (track_sel && (status & GPIO_DCC_SERVICE_INPUT_DCC_SERVICE_IN_PIN)) {
        accept = true;
    }

    if (accept) {
        uint32_t ts = TI_DccDriver_get_timestamp_usec();
        uint16_t next = (_edge_head + 1) & EDGE_BUF_MASK;
        if (next != _edge_tail) {
            _edge_buf[_edge_head] = ts;
            _edge_head = next;
        }
        DL_GPIO_togglePins(GPIO_TEST_PORT, GPIO_TEST_RX_INT_TEST_PIN);
    }

    DL_GPIO_clearInterruptStatus(GPIOB,
        GPIO_DCC_INPUT_DCC_IN_PIN | GPIO_DCC_SERVICE_INPUT_DCC_SERVICE_IN_PIN);

}

/* SysTick ISR -- 100 ms periodic tick.
 * Blinks LED1 as a heartbeat (toggle every 500 ms = 5 ticks). */
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

/* Drains all pending edge timestamps from the ISR ring buffer and feeds
 * them to the DCC bit decoder.  The track-select pin (PB17) determines
 * which physical input is active; edges arriving while the wrong input
 * fired are still valid timestamps because the ISR captured them
 * unconditionally — the mux only matters for future multi-decoder work.
 * Toggles the test pin (PB3) for each edge so the LA still shows ISR
 * activity. */
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

/* Initialization order matters:
 *  1. SYSCFG_DL_init()          -- clocks, GPIO, timers, UART (generated)
 *  2. NVIC_EnableIRQ(...)       -- enable interrupts not covered by SysConfig
 *  3. TI_DccDriver_initialize() -- start the free-running timestamp timer
 *  4. TI_UartDriver_initialize()-- enable UART RX interrupt
 *  5. CallbacksDcc_initialize() -- clear ring buffer, set CV defaults
 *  6. DccConfig_initialize()    -- hand the config struct to the library
 *                                  (reads CV1/CV29 defaults for address)
 *
 * After that, the main loop calls four functions repeatedly:
 *   DccConfig_run()                -- library periodic housekeeping
 *   CallbacksDcc_drain()           -- push queued RECV lines out over UART
 *   TI_UartDriver_echo_process()   -- echo typed characters back to terminal
 *   DecoderCommandParser_process() -- handle typed commands (ADDR, HELP, etc.)
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
