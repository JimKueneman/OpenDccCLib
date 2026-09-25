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
 * @file ti_driverlib_uart_driver.c
 * @brief UART driver implementation (MSPM0G3507).
 *
 * @details RX: An ISR stores incoming bytes into a ring buffer.  The main loop
 *     reads complete lines out of that buffer via read_line().
 * TX: Blocking (polling) -- each byte waits for the TX FIFO to be ready.
 *     Fine for a demo; a production driver might use DMA or a TX ring.
 *
 * The ring buffer uses the same single-producer / single-consumer pattern
 * as callbacks_dcc.c -- the ISR writes _rx_head, the main loop reads
 * _rx_tail.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_driverlib_uart_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

    /** @brief Receive ring buffer size in bytes. */
#define UART_RX_RING_SIZE 256

    /** @brief Receive ring buffer; the RX ISR is the only writer. */
static volatile uint8_t _rx_ring[UART_RX_RING_SIZE];
    /** @brief Ring write index, advanced by the RX ISR only. */
static volatile uint16_t _rx_head = 0;  /* ISR writes here */
    /** @brief Ring read index for line parsing, advanced by the main loop only. */
static volatile uint16_t _rx_tail = 0;  /* main loop reads from here */

    /** @brief Ring read index for terminal echo, advanced by the main loop only. */
static volatile uint16_t _echo_tail = 0;  /* echo read pointer */

    /**
     * @brief Enables the UART RX interrupt in the NVIC.
     */
void TI_UartDriver_initialize(void) {

    NVIC_EnableIRQ(UART_CMD_INST_INT_IRQN);

}

    /**
     * @brief Echoes every byte between _echo_tail and _rx_head back to the terminal.
     *
     * @details A received CR is followed by an LF so the terminal moves to the next line.
     */
void TI_UartDriver_echo_process(void) {

    /* Echo any new characters from main loop context (not ISR) */
    while (_echo_tail != _rx_head) {

        uint8_t c = _rx_ring[_echo_tail];
        DL_UART_Main_transmitDataBlocking(UART_CMD_INST, c);
        if (c == '\r') {

            DL_UART_Main_transmitDataBlocking(UART_CMD_INST, '\n');

        }
        _echo_tail = (_echo_tail + 1) % UART_RX_RING_SIZE;

    }

}

    /**
     * @brief Copies the next complete line out of the receive ring buffer.
     *
     * @details Algorithm:
     * -# Scan from _rx_tail to _rx_head for a CR or LF; return false if none is found.
     * -# Copy the bytes before it into buffer, truncating at max_len - 1, and null-terminate.
     * -# Advance past the terminator, and past one more CR/LF if it immediately follows.
     * -# Publish the new _rx_tail.
     *
     * @verbatim
     * @param buffer  Destination for the null-terminated line, without the newline.
     * @param max_len Size of buffer including the null terminator.
     * @endverbatim
     *
     * @return true if a line was copied, false if no complete line is ready yet.
     */
bool TI_UartDriver_read_line(char *buffer, uint16_t max_len) {

    uint16_t tail = _rx_tail;
    uint16_t head = _rx_head;
    uint16_t idx;
    uint16_t line_end = 0xFFFF;

    /* Scan for a newline character in the ring buffer */
    idx = tail;
    while (idx != head) {

        uint8_t c = _rx_ring[idx];

        if (c == '\r' || c == '\n') {

            line_end = idx;
            break;

        }

        idx = (idx + 1) % UART_RX_RING_SIZE;

    }

    if (line_end == 0xFFFF) {

        return false;

    }

    /* Copy characters up to the newline into the output buffer */
    uint16_t pos = 0;
    idx = tail;
    while (idx != line_end && pos < (max_len - 1)) {

        buffer[pos++] = (char)_rx_ring[idx];
        idx = (idx + 1) % UART_RX_RING_SIZE;

    }
    buffer[pos] = '\0';

    /* Skip past the newline character(s) */
    idx = (line_end + 1) % UART_RX_RING_SIZE;

    /* Also skip a trailing \n after \r (or vice versa) */
    if (idx != _rx_head && (_rx_ring[idx] == '\r' || _rx_ring[idx] == '\n')) {

        idx = (idx + 1) % UART_RX_RING_SIZE;

    }

    _rx_tail = idx;

    return true;

}

    /**
     * @brief Transmits a null-terminated string one byte at a time with blocking writes.
     *
     * @verbatim
     * @param str Null-terminated string to send.
     * @endverbatim
     */
void TI_UartDriver_write_string(const char *str) {

    while (*str) {

        DL_UART_Main_transmitDataBlocking(UART_CMD_INST, (uint8_t)*str);
        str++;

    }

}

    /**
     * @brief UART0 RX interrupt handler; pushes each received byte into the ring buffer.
     *
     * @details The byte is dropped when the ring is full so _rx_head never overtakes _rx_tail.
     */
void UART_CMD_INST_IRQHandler(void) {

    switch (DL_UART_Main_getPendingInterrupt(UART_CMD_INST)) {

        case DL_UART_MAIN_IIDX_RX: {

            uint8_t byte = DL_UART_Main_receiveData(UART_CMD_INST);
            uint16_t next_head = (_rx_head + 1) % UART_RX_RING_SIZE;

            /* Drop byte if ring buffer is full */
            if (next_head != _rx_tail) {

                _rx_ring[_rx_head] = byte;
                _rx_head = next_head;

            }
            break;

        }

        default:

            break;

    }

}
