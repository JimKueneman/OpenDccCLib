// wfplayer_command_parser.h -- UART line -> verb -> OK/ERR, per PROTOCOL.md.
//
// Call WfCmdParser_process() each main-loop iteration; it reads one complete line
// (if available) from the UART driver, dispatches it, and writes exactly one reply.

#ifndef __WFPLAYER_COMMAND_PARSER__
#define __WFPLAYER_COMMAND_PARSER__

void WfCmdParser_initialize(void);
void WfCmdParser_process(void);

#endif /* __WFPLAYER_COMMAND_PARSER__ */
