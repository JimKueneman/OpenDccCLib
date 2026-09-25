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
 * @file decoder_command_parser.h
 * @brief Simple UART command parser for the demo.
 *
 * @details Supports: ADDR, CLEAR, ACK, STATUS, HELP.
 * This is demo infrastructure only -- a real decoder would not normally
 * need a UART command interface.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __DECODER_COMMAND_PARSER__
#define __DECODER_COMMAND_PARSER__

#ifdef __cplusplus
extern "C" {
#endif

    /** @brief Set up the parser (resets the reported address to the default 3/SHORT). */
extern void DecoderCommandParser_initialize(void);

    /**
     * @brief Check for a complete UART line and execute the command.
     *
     * @details Call from the main loop. Non-blocking; every executed line gets one
     * reply and a fresh prompt.
     */
extern void DecoderCommandParser_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __DECODER_COMMAND_PARSER__ */
