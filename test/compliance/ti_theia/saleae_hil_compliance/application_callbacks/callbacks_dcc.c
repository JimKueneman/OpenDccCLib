// callbacks_dcc.c
//
// Application callback implementations for the command station demo.
// See callbacks_dcc.h for how to add new callbacks.

#include "callbacks_dcc.h"
#include "ti_msp_dl_config.h"
#include "application_drivers/ti_driverlib_uart_driver.h"

#ifdef DCC_COMPILE_COMMAND_STATION

void CallbacksDcc_on_service_mode_result(dcc_service_mode_result_t result) {

    switch (result) {

        case DCC_SERVICE_MODE_SUCCESS:
            TI_UartDriver_write_string("SVC RESULT: SUCCESS\r\n");
            break;

        case DCC_SERVICE_MODE_NO_ACK:
            TI_UartDriver_write_string("SVC RESULT: NO ACK\r\n");
            break;

        case DCC_SERVICE_MODE_VERIFY_FAIL:
            TI_UartDriver_write_string("SVC RESULT: VERIFY FAIL\r\n");
            break;

        case DCC_SERVICE_MODE_BUSY:
            TI_UartDriver_write_string("SVC RESULT: BUSY\r\n");
            break;

        case DCC_SERVICE_MODE_ERROR:
            TI_UartDriver_write_string("SVC RESULT: ERROR\r\n");
            break;

        case DCC_SERVICE_MODE_NOT_IN_SERVICE_MODE:
            TI_UartDriver_write_string("SVC RESULT: NOT IN SERVICE MODE\r\n");
            break;

        default:
            TI_UartDriver_write_string("SVC RESULT: UNKNOWN\r\n");
            break;
    }
}

// Test trigger state. When armed, the next NON-idle packet raises PB3 once so a
// logic analyzer can hardware-trigger on the exact packet under test.
static volatile bool _test_trigger_armed = false;

// The DCC idle packet (S-9.2): 11111111 00000000 11111111.
static bool _is_idle_packet(const dcc_packet_t *p) {

    return p->byte_count == 3 &&
           p->data[0] == 0xFF && p->data[1] == 0x00 && p->data[2] == 0xFF;
}

void CallbacksDcc_arm_trigger(void) {

    // Drop PB3 low first so the armed packet produces one clean rising edge.
    DL_GPIO_clearPins(GPIO_DEBUG_PORT, GPIO_DEBUG_DEBUG_PIN_PIN);
    _test_trigger_armed = true;
}

void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet) {

    // When armed, fire the trigger on the first non-idle packet (the packet
    // under test), then disarm. PB3 stays quiet otherwise, so the rising edge
    // is unambiguous for the analyzer's digital trigger.
    if (_test_trigger_armed && !_is_idle_packet(packet)) {
        DL_GPIO_setPins(GPIO_DEBUG_PORT, GPIO_DEBUG_DEBUG_PIN_PIN);
        _test_trigger_armed = false;
    }
}

#endif /* DCC_COMPILE_COMMAND_STATION */
