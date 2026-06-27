// ti_driverlib_uart_driver.h
//
// UART command interface for the MSPM0G3507 LaunchPad.
//
// This driver is entirely application-specific -- the DCC library does NOT
// require it. It provides a simple text terminal over UART0 (230400 baud,
// backchannel pins PA10 TX / PA11 RX) so you can type commands to control
// the command station.
//
// If your application uses a different interface (SPI, USB, CAN, etc.),
// you do not need this file at all.

#ifndef __TI_DRIVERLIB_UART_DRIVER__
#define __TI_DRIVERLIB_UART_DRIVER__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the UART peripheral and enable the RX interrupt.
extern void TI_UartDriver_initialize(void);

// Check if a complete line (terminated by CR or LF) is available in the
// receive ring buffer. If so, copy it into 'buffer' (null-terminated, no
// newline) and return true. Returns false if no complete line is ready yet.
// max_len is the size of 'buffer' including the null terminator.
extern bool TI_UartDriver_read_line(char *buffer, uint16_t max_len);

// Echo received characters back to the terminal. Must be called from the
// main loop -- NOT safe to call from ISR context.
extern void TI_UartDriver_echo_process(void);

// Transmit a null-terminated string using polling (blocking) TX.
// Safe to call from main loop. Not ISR-safe.
extern void TI_UartDriver_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_UART_DRIVER__ */
