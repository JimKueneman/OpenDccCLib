/** \copyright
 * Copyright (c) 2026, Jim Kueneman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file uart_command_parser.h
 * @brief Text command parser for the UART terminal interface.
 *
 * @details Reads complete lines from TI_UartDriver_read_line(), tokenizes them,
 * and dispatches to the appropriate DCC library API calls. This is
 * application-level code -- the DCC library does not depend on it.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
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
