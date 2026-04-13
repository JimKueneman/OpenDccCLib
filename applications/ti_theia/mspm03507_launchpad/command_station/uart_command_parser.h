// uart_command_parser.h
//
// Text command parser for the UART terminal interface.
//
// Reads complete lines from TI_UartDriver_read_line(), tokenizes them,
// and dispatches to the appropriate DCC library API calls. This is
// application-level code -- the DCC library does not depend on it.

#ifndef __UART_COMMAND_PARSER__
#define __UART_COMMAND_PARSER__

#ifdef __cplusplus
extern "C" {
#endif

// Initialize parser state. Call once at startup after TI_UartDriver_initialize().
extern void UartCommandParser_initialize(void);

// Check for a complete command line and execute it if available.
// Call from your main loop. Non-blocking -- returns immediately if no
// complete line is ready.
extern void UartCommandParser_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_COMMAND_PARSER__ */
