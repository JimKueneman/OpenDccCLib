// ti_driverlib_uart_driver.c
//
// UART command interface implementation for MSPM0G3507.
//
// RING BUFFER PATTERN:
// The RX ISR writes incoming bytes into _rx_ring[] at _rx_head.
// The main loop reads from _rx_ring[] at _rx_tail (for command parsing)
// and _echo_tail (for echoing characters back to the terminal).
// _rx_head is written only by the ISR; _rx_tail and _echo_tail are
// written only by the main loop. This single-producer / single-consumer
// design means no locking is needed as long as the head/tail indices
// are read atomically (they are uint16_t on a 32-bit MCU, so they are).
//
// ISR vs MAIN-LOOP SAFETY:
// - TI_UartDriver_echo_process() and TI_UartDriver_read_line() must be
//   called from main-loop context only. They read _rx_head (written by ISR)
//   but never write it, so no race condition exists.
// - TI_UartDriver_write_string() uses blocking TX and must not be called
//   from an ISR (it would stall the ISR for the entire string duration).

#include "ti_driverlib_uart_driver.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>

#define UART_RX_RING_SIZE 256

static volatile uint8_t _rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t _rx_head = 0;  /* ISR writes here */
static volatile uint16_t _rx_tail = 0;  /* main loop reads from here */

static volatile uint16_t _echo_tail = 0;  /* echo read pointer */

void TI_UartDriver_initialize(void) {

    NVIC_EnableIRQ(UART_CMD_INST_INT_IRQN);
}

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

void TI_UartDriver_write_string(const char *str) {

    while (*str) {
        DL_UART_Main_transmitDataBlocking(UART_CMD_INST, (uint8_t)*str);
        str++;
    }
}

/* UART0 RX interrupt handler */
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
