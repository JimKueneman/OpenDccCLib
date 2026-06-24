// saleae_hil_compliance.c
//
// Main entry point for the Saleae hardware-in-the-loop DCC compliance test
// firmware on the MSPM0G3507 LaunchPad. Cloned from the command_station demo:
// same UART command interface, used to drive the DCC signal under test while a
// Saleae logic analyzer captures and verifies it against the NMRA standards.
//
// INTEGRATION PATTERN:
//   1. Fill a dcc_config_t struct with function pointers to your hardware
//      drivers and (optionally) your application callbacks.
//   2. Call DccConfig_initialize(&config) once at startup.
//   3. Wire ISRs:
//        - Shared 58us timer ISR must call DccConfig_58us_timer_isr().
//        - RailCom cutout one-shot timer ISR must call DccConfig_railcom_oneshot_timer_isr().
//        - A 100 ms periodic timer (or SysTick) must call DccConfig_100ms_timer_tick().
//   4. Call DccConfig_run() repeatedly in your main loop.
//
// HOW TO ADD YOUR OWN CALLBACKS:
//   Replace any NULL in the dcc_config struct below with a pointer to your
//   function. For example, to get notified on every transmitted packet:
//       .on_packet_sent = &MyApp_on_packet_sent,
//   Your function signature must match the corresponding function pointer
//   typedef in dcc_config.h.

#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

#include "dcc_lib/dcc_config.h"

#include "application_drivers/ti_driverlib_dcc_driver.h"
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "application_callbacks/callbacks_dcc.h"
#include "uart_command_parser.h"

/* ========================================================================== */
/* DCC library configuration (function pointer wiring)                        */
/*                                                                            */
/* This struct is how you connect your hardware to the DCC library.           */
/*                                                                            */
/* REQUIRED fields must point to valid functions or the library will not work. */
/* NULL-optional fields disable a feature when set to NULL (e.g. RailCom).    */
/* OPTIONAL callbacks are purely for your application -- NULL means you just  */
/* do not get notified of that event.                                         */
/* ========================================================================== */

/* ========================================================================== */
/* RailCom cutout (compliance rig)                                            */
/*                                                                            */
/* Continuous-clock model: the bit encoder never stalls, so the DCC line (PB1) */
/* runs continuously -- IDENTICAL with or without RailCom. The cutout is a     */
/* separate cutout-active SIGNAL: begin/end (TI_DccDriver_main_cutout_begin/    */
/* end) raise/drop PB2 (DCC_MIRROR). Real H-bridge hardware would mux on that   */
/* signal to tristate the track during the window; here PB2 is just the Saleae  */
/* cutout-window marker. Fires only when RAILCOM ON has been sent AND .railcom  */
/* is wired. uart_rx_* / uart_read are NULL: no RailCom receive path on this    */
/* rig.                                                                        */
/* ========================================================================== */

static const dcc_railcom_hw_t _main_railcom_hw = {
    .begin_railcom_cutout       = &TI_DccDriver_main_cutout_begin,
    .end_railcom_cutout         = &TI_DccDriver_main_cutout_end,
    .uart_rx_enable             = NULL,
    .uart_rx_disable            = NULL,
    .uart_read                  = NULL,
    .on_railcom_datagram_result = NULL,
};

static const dcc_config_t dcc_config = {

    // REQUIRED -- these three are needed for every role (command station or decoder).
    .lock_shared_resources   = &TI_DccDriver_lock_shared_resources,
    .unlock_shared_resources = &TI_DccDriver_unlock_shared_resources,
    .get_timestamp_usec      = &TI_DccDriver_get_timestamp_usec,

    // Shared fixed-period timer (58us) drives both main and service track
    // bit encoders via DccConfig_58us_timer_isr().
    .shared_timer_start      = &TI_DccDriver_shared_timer_start,
    .shared_timer_stop       = &TI_DccDriver_shared_timer_stop,

    // RailCom cutout one-shot timer drives the cutout state machine
    // via DccConfig_railcom_cutout_timer_isr().
    .railcom_timer_start     = &TI_DccDriver_railcom_timer_start,
    .railcom_timer_stop      = &TI_DccDriver_railcom_timer_stop,

    // RailCom cutout state-machine timing (microseconds), HARDWARE-CALIBRATED for
    // this rig. The library periods are durations; real ISR/dispatch latency adds
    // ~6 us by T_CS and ~18 us by T_CE. Measured at spec defaults: T_CS hit the
    // 32 us ceiling and T_CE landed at 472 us. These values trim the durations so
    // the measured cutout centers in the S-9.3.2 windows (T_CS ~29 / 26-32,
    // T_CE ~471 / 454-488). Set any field to 0 to fall back to the spec default.
    .railcom_cutout_start_delay_us = 23,   /* DELAY    -> T_CS  ~29us (was 26; -3 for latency) */
    .railcom_uart_rx_delay_us      = 54,   /* SETTLING -> T_TS1                                */
    .railcom_ch1_window_us         = 97,   /* CH1      -> T_TC1                                */
    .railcom_ch1_ch2_gap_us        = 16,   /* GAP      -> T_TS2                                */
    .railcom_ch2_window_us         = 263,  /* CH2      -> T_CE  ~471us (was 261; +2 re-center) */

    // Main track hardware -- runs the scheduler (normal DCC operations).
    .main_track = {
        .timer_start      = NULL,    /* not used with shared timer */
        .timer_stop       = NULL,    /* not used with shared timer */
        .pin_toggle       = &TI_DccDriver_main_pin_toggle,
        .track_power_set  = &TI_DccDriver_track_power_set,
        .current_sense_read = NULL,  /* no current sensing on main track */
        .railcom          = &_main_railcom_hw,  /* cutout marker on PB2 (DCC_MIRROR) */
    },

    // Service track hardware -- runs service mode (programming).
    .service_track = {
        .timer_start        = NULL,  /* not used with shared timer */
        .timer_stop         = NULL,  /* not used with shared timer */
        .pin_toggle         = &TI_DccDriver_svc_pin_toggle,
        .track_power_set    = &TI_DccDriver_svc_track_power_set,
        .current_sense_read = &TI_DccDriver_current_sense_read,
        .railcom            = NULL,
    },

    // OPTIONAL application callbacks.
    .on_packet_sent          = &CallbacksDcc_on_packet_sent,
};

/* ========================================================================== */
/* ISR handlers                                                               */
/* ========================================================================== */

// Shared DCC timer ISR. Fires every 58us (fixed period). Drives the tick ISR
// for both main track and service track bit encoders. Pin toggling is handled
// inside the library via the pin_toggle callbacks — not here.
void DCC_BIT_TIMER_INST_IRQHandler(void) {

    DL_GPIO_setPins(GPIO_ISR_TIME_PORT, GPIO_ISR_TIME_ISR_TIME_PIN);

    switch (DL_TimerA_getPendingInterrupt(DCC_BIT_TIMER_INST)) {

        case DL_TIMER_IIDX_ZERO:
            TI_DccDriver_timestamp_tick();
            DccConfig_58us_timer_isr();
            break;

        default:
            break;
    }

    DL_GPIO_clearPins(GPIO_ISR_TIME_PORT, GPIO_ISR_TIME_ISR_TIME_PIN);
}

// RailCom cutout one-shot timer ISR. Fires at each state expiry of the cutout
// sequence (DELAY 26us, SETTLING 54us, CH1 97us, GAP 16us, CH2 261us).
// Drives the cutout state machine.
void RAILCOM_TIMER_INST_IRQHandler(void) {

    switch (DL_TimerA_getPendingInterrupt(RAILCOM_TIMER_INST)) {

        case DL_TIMER_IIDX_ZERO:
            DccConfig_railcom_oneshot_timer_isr();
            break;

        default:
            break;
    }
}

// SysTick ISR -- fires every 100 ms.
// Calls DccConfig_100ms_timer_tick() for library housekeeping (timeouts,
// periodic maintenance). Also blinks LED1 as a heartbeat (toggles every
// 500 ms = 5 ticks).
void SysTick_Handler(void) {

    static uint8_t heartbeat_count = 0;

    DccConfig_100ms_timer_tick();

    heartbeat_count++;
    if (heartbeat_count >= 5) {
        heartbeat_count = 0;
        DL_GPIO_togglePins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_1_PIN);
    }
}

/* ========================================================================== */
/* Main                                                                       */
/* ========================================================================== */

int main(void) {

    // SysConfig-generated device initialization (clocks, GPIO, timers, UART).
    // This is TI-specific. On other platforms, replace with your own init.
    SYSCFG_DL_init();

    // Initialize hardware drivers before the DCC library
    TI_DccDriver_initialize();
    TI_UartDriver_initialize();

    // Pass our configuration to the DCC library. After this call the library
    // is ready but track power is still off. Call DccApplicationCommandStationMainTrack_power_on()
    // (via a UART command in this demo) to start sending DCC packets.
    DccConfig_initialize(&dcc_config);

    // Initialize command parser
    UartCommandParser_initialize();

    // Startup banner
    TI_UartDriver_write_string("\r\n");
    TI_UartDriver_write_string("DCC Saleae HIL Compliance - MSPM0G3507 LaunchPad\r\n");
    TI_UartDriver_write_string("Type HELP for available commands.\r\n");
    TI_UartDriver_write_string("> ");


    // Main loop -- three things must run continuously:
    //   DccConfig_run()              -- library packet scheduling, callback dispatch
    //   TI_UartDriver_echo_process() -- echo typed characters back to terminal
    //   UartCommandParser_process()  -- parse and execute completed command lines
    while (1) {

        DccConfig_run();
        TI_UartDriver_echo_process();
        UartCommandParser_process();
    }
}
