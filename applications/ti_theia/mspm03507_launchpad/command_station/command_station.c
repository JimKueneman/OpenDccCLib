// command_station.c
//
// Main entry point for the DCC Command Station demo on MSPM0G3507 LaunchPad.
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

    // Main track hardware -- runs the scheduler (normal DCC operations).
    .main_track = {
        .timer_start      = NULL,    /* not used with shared timer */
        .timer_stop       = NULL,    /* not used with shared timer */
        .pin_toggle       = &TI_DccDriver_main_pin_toggle,
        .track_power_set  = &TI_DccDriver_track_power_set,
        .current_sense_read = NULL,  /* no current sensing on main track */
        .railcom          = NULL,    /* TODO: add RailCom detector later */
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
    .on_service_mode_result  = &CallbacksDcc_on_service_mode_result,
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

// RailCom cutout one-shot timer ISR. Fires at each phase boundary of the
// cutout sequence (88us, 464us, 1080us). Drives the cutout state machine.
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
    TI_UartDriver_write_string("DCC Command Station - MSPM0G3507 LaunchPad\r\n");
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
