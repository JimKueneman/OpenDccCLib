/*
 * decoder_command_parser.h -- Simple UART command parser for the demo.
 *
 * Supports: ADDR, CLEAR, STATUS, HELP.
 * This is demo infrastructure only -- a real decoder would not normally
 * need a UART command interface.
 */

#ifndef __DECODER_COMMAND_PARSER__
#define __DECODER_COMMAND_PARSER__

#ifdef __cplusplus
extern "C" {
#endif

/* Set up the parser (resets address to default 3/SHORT). */
extern void DecoderCommandParser_initialize(void);

/* Check for a complete UART line and execute the command.
 * Call this from the main loop. */
extern void DecoderCommandParser_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __DECODER_COMMAND_PARSER__ */
