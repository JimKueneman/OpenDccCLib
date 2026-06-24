// callbacks_dcc.c
//
// Application callback implementations for the command station demo.
// See callbacks_dcc.h for how to add new callbacks.

#include "callbacks_dcc.h"
#include "ti_msp_dl_config.h"
#include "application_drivers/ti_driverlib_uart_driver.h"

#ifdef DCC_COMPILE_COMMAND_STATION

void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet) {

    (void)packet;
    DL_GPIO_togglePins(GPIO_DEBUG_PORT, GPIO_DEBUG_DEBUG_PIN_PIN);
}

#endif /* DCC_COMPILE_COMMAND_STATION */
