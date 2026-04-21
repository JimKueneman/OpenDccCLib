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

void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet) {

    (void)packet;
    DL_GPIO_togglePins(GPIO_DEBUG_PORT, GPIO_DEBUG_DEBUG_PIN_PIN);
}

#endif /* DCC_COMPILE_COMMAND_STATION */
