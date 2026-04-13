/*
 * ti_driverlib_uart_driver.h -- UART driver for the demo's command interface.
 *
 * Uses ISR-driven receive into a ring buffer and blocking (polling) transmit.
 * UART0 at 230400 baud on the LaunchPad backchannel pins (PA10 TX, PA11 RX).
 *
 * This driver is demo infrastructure for the UART command parser.  A real
 * decoder may not need UART at all, or may use it for debug output only.
 */

#ifndef __TI_DRIVERLIB_UART_DRIVER__
#define __TI_DRIVERLIB_UART_DRIVER__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enable the UART RX interrupt. */
extern void TI_UartDriver_initialize(void);

/* Check whether a complete line (terminated by CR or LF) has been received.
 * If so, copy it into 'buffer' (null-terminated, no newline) and return true.
 * If no complete line is available yet, return false. */
extern bool TI_UartDriver_read_line(char *buffer, uint16_t max_len);

/* Echo received characters back to the terminal.
 * Call from main loop -- not ISR-safe. */
extern void TI_UartDriver_echo_process(void);

/* Transmit a null-terminated string by polling the TX register. */
extern void TI_UartDriver_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_UART_DRIVER__ */
