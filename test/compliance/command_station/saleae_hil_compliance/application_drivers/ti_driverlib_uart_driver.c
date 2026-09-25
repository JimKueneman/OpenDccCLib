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
 * @brief UART command interface implementation for MSPM0G3507.
 *
 * @details RING BUFFER PATTERN:
 * The RX ISR writes incoming bytes into _rx_ring[] at _rx_head.
 * The main loop reads from _rx_ring[] at _rx_tail (for command parsing)
 * and _echo_tail (for echoing characters back to the terminal).
 * _rx_head is written only by the ISR; _rx_tail and _echo_tail are
 * written only by the main loop. This single-producer / single-consumer
 * design means no locking is needed as long as the head/tail indices
 * are read atomically (they are uint16_t on a 32-bit MCU, so they are).
 *
 * ISR vs MAIN-LOOP SAFETY:
 * - TI_UartDriver_echo_process() and TI_UartDriver_read_line() must be
 *   called from main-loop context only. They read _rx_head (written by ISR)
 *   but never write it, so no race condition exists.
 * - TI_UartDriver_write_string() uses blocking TX and must not be called
 *   from an ISR (it would stall the ISR for the entire string duration).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_driverlib_uart_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

    /** @brief Receive ring depth in bytes (one entry is kept free). */
#define UART_RX_RING_SIZE 256

    /** @brief Receive ring buffer. */
static volatile uint8_t _rx_ring[UART_RX_RING_SIZE];
    /** @brief Ring write index; the RX ISR writes it. */
static volatile uint16_t _rx_head = 0;  /* ISR writes here */
    /** @brief Ring read index for command parsing; the main loop writes it. */
static volatile uint16_t _rx_tail = 0;  /* main loop reads from here */

    /** @brief Ring read index for echo; the main loop writes it. */
static volatile uint16_t _echo_tail = 0;  /* echo read pointer */

    /** @brief Enable the command UART RX interrupt. */
void TI_UartDriver_initialize(void) {

    NVIC_EnableIRQ(UART_CMD_INST_INT_IRQN);
}

    /**
     * @brief Echo every byte received since the last call back to the terminal; CR is echoed as CR+LF.
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
     * @brief Fetch one complete line from the receive ring.
     *
     * @details Algorithm:
     * -# Scan from the tail to the head for a CR or LF; return false if none.
     * -# Copy the bytes before it into buffer, truncating at max_len - 1, and null-terminate.
     * -# Advance the tail past the terminator and past one following CR/LF (so CR+LF counts as one).
     *
     * @verbatim
     * @param buffer   Destination for the line.
     * @param max_len  Size of buffer including the null terminator.
     * @endverbatim
     *
     * @return true when a line was copied, false when none is complete yet.
     */
bool TI_UartDriver_read_line(char *buffer, uint16_t max_len) {

    uint16_t tail = _rx_tail;
    uint16_t head = _rx_head;
    uint16_t i;
    uint16_t line_end = 0xFFFF;

    /* Scan for a newline character in the ring buffer */
    i = tail;
    while (i != head) {

        uint8_t c = _rx_ring[i];

        if (c == '\r' || c == '\n') {
            line_end = i;
            break;
        }

        i = (i + 1) % UART_RX_RING_SIZE;
    }

    if (line_end == 0xFFFF)
        return false;

    /* Copy characters up to the newline into the output buffer */
    uint16_t pos = 0;
    i = tail;
    while (i != line_end && pos < (max_len - 1)) {

        buffer[pos++] = (char)_rx_ring[i];
        i = (i + 1) % UART_RX_RING_SIZE;
    }
    buffer[pos] = '\0';

    /* Skip past the newline character(s) */
    i = (line_end + 1) % UART_RX_RING_SIZE;

    /* Also skip a trailing \n after \r (or vice versa) */
    if (i != _rx_head && (_rx_ring[i] == '\r' || _rx_ring[i] == '\n')) {
        i = (i + 1) % UART_RX_RING_SIZE;
    }

    _rx_tail = i;

    return true;
}

    /**
     * @brief Transmit a null-terminated string byte by byte with blocking TX.
     *
     * @verbatim
     * @param str  Null-terminated string to send.
     * @endverbatim
     */
void TI_UartDriver_write_string(const char *str) {

    while (*str) {
        DL_UART_Main_transmitDataBlocking(UART_CMD_INST, (uint8_t)*str);
        str++;
    }
}

    /**
     * @brief Command UART (UART0) RX interrupt: push the received byte on the ring, dropping it if the ring is full.
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
