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
 * @file ti_driverlib_uart_driver.h
 * @brief UART command interface for the MSPM0G3507 LaunchPad.
 *
 * @details This driver is entirely application-specific -- the DCC library does NOT
 * require it. It provides a simple text terminal over UART0 (230400 baud,
 * backchannel pins PA10 TX / PA11 RX) so you can type commands to control
 * the command station.
 *
 * If your application uses a different interface (SPI, USB, CAN, etc.),
 * you do not need this file at all.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __TI_DRIVERLIB_UART_DRIVER__
#define __TI_DRIVERLIB_UART_DRIVER__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /** @brief Enable the command UART RX interrupt (the peripheral itself is set up by SysConfig). */
extern void TI_UartDriver_initialize(void);

    /**
     * @brief Fetch one complete line (terminated by CR or LF) from the receive ring buffer.
     *
     * @details Copies the line into buffer null-terminated with no newline and
     * consumes it, including a CR+LF pair. Main-loop context only.
     *
     * @param buffer   Destination for the line.
     * @param max_len  Size of buffer including the null terminator; longer lines are truncated.
     *
     * @return true when a complete line was copied, false when none is ready yet.
     */
extern bool TI_UartDriver_read_line(char *buffer, uint16_t max_len);

    /** @brief Echo received characters back to the terminal (CR becomes CR+LF). Main loop only, not ISR-safe. */
extern void TI_UartDriver_echo_process(void);

    /**
     * @brief Transmit a null-terminated string with blocking (polled) TX.
     *
     * @details Safe from the main loop; not ISR-safe because it stalls for the whole string.
     *
     * @param str  Null-terminated string to send.
     */
extern void TI_UartDriver_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_UART_DRIVER__ */
