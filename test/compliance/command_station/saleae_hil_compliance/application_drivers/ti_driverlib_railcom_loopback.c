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
 * @file ti_driverlib_railcom_loopback.c
 * @brief RailCom loopback for the HIL bench. See the header for the design.
 *
 * @details ISR vs MAIN-LOOP SAFETY: the receive ring is single-producer (RX ISR writes
 * _rx_head) / single-consumer (uart_read, main loop, writes _rx_tail). The one
 * exception is the flush at cutout begin, which resets _rx_tail from the cutout
 * timer ISR; the library only reads the ring after a cutout COMPLETES, ~7 ms
 * before the next one begins, so on this bench the flush never races a read.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "ti_driverlib_railcom_loopback.h"
#include "ti_msp_dl_config.h"
#include "dcc_user_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>
#include <string.h>

#if defined(DCC_COMPILE_COMMAND_STATION) && defined(DCC_COMPILE_RAILCOM)

    /** @brief Receive ring depth in bytes (one entry is kept free). */
#define RC_RX_RING_SIZE 32u
    /** @brief Maximum raw Channel 1 bytes in a mock reply. */
#define RC_CH1_MAX      2u
    /** @brief Maximum raw Channel 2 bytes in a mock reply. */
#define RC_CH2_MAX      6u
    /** @brief Mock transmit buffer size: both channels back to back. */
#define RC_TX_MAX       (RC_CH1_MAX + RC_CH2_MAX)

/* --- receive side ------------------------------------------------------- */
    /** @brief Receive ring: bytes accepted while a window was open. */
static volatile uint8_t  _rx_ring[RC_RX_RING_SIZE];
    /** @brief Ring write index; RX ISR writes. */
static volatile uint8_t  _rx_head = 0;          /* RX ISR writes */
    /** @brief Ring read index; uart_read (main loop) writes, the cutout-begin flush resets. */
static volatile uint8_t  _rx_tail = 0;          /* uart_read (main loop) writes; cutout-begin flush resets */
    /** @brief Receiver gate: true only while a Ch1/Ch2 window is open. */
static volatile bool     _gate_open = false;    /* true only while a Ch1/Ch2 window is open */

/* --- mock transmit side ------------------------------------------------- */
/* An arm from the UART command (main loop) can land at any point of the packet
 * cycle, including inside a cutout whose Channel 1 window has already opened.
 * So the arm only sets _armed; the cutout-begin hook latches it into _play for
 * that whole cutout, and the window / cutout-end hooks act on _play. A reply
 * therefore always plays complete on the NEXT cutout, never half of this one. */
    /** @brief A reply is armed; set by arm(), consumed at cutout begin. */
static volatile bool               _armed = false;   /* set by arm(), consumed at cutout begin */
    /** @brief The current cutout carries the reply (latched from _armed at cutout begin). */
static volatile bool               _play  = false;   /* this cutout carries the reply */
    /** @brief Transmit timing of the armed reply. */
static volatile rc_loopback_mode_t _mode = RC_LOOPBACK_MODE_WINDOW;
    /** @brief Armed Channel 1 bytes. */
static uint8_t                     _ch1[RC_CH1_MAX];
    /** @brief Armed Channel 2 bytes. */
static uint8_t                     _ch2[RC_CH2_MAX];
    /** @brief Number of armed Channel 1 bytes. */
static volatile uint8_t            _n1 = 0;
    /** @brief Number of armed Channel 2 bytes. */
static volatile uint8_t            _n2 = 0;
    /** @brief Windows opened in this cutout: 1 = Ch1, 2 = Ch2. */
static volatile uint8_t            _window_index = 0;   /* windows opened in this cutout: 1 = Ch1, 2 = Ch2 */

    /** @brief Bytes queued on the mock transmitter. */
static uint8_t          _tx_buf[RC_TX_MAX];
    /** @brief Number of bytes queued in _tx_buf. */
static volatile uint8_t _tx_len = 0;
    /** @brief Number of queued bytes already pushed into the TX FIFO. */
static volatile uint8_t _tx_pos = 0;

/* --- counters ----------------------------------------------------------- */
    /** @brief Cutouts begun since the last reset. */
static volatile uint32_t _cutouts = 0;
    /** @brief Bytes received while a window was open. */
static volatile uint32_t _rx_accepted = 0;
    /** @brief Bytes received with the gate closed. */
static volatile uint32_t _rx_dropped = 0;
    /** @brief Bytes the mock transmitter queued. */
static volatile uint32_t _tx_bytes = 0;

    /**
     * @brief Queue n bytes on the mock transmitter.
     *
     * @details Copies the bytes, fills the TX FIFO now and lets the TX interrupt top
     * it up if they did not all fit. Fast enough to call from the cutout timer ISR.
     *
     * @param bytes  Bytes to transmit.
     * @param n      Number of bytes, 0 is a no-op.
     */
static void _tx_start(const uint8_t *bytes, uint8_t n) {

    if (n == 0) {
        return;
    }

    memcpy(_tx_buf, bytes, n);
    _tx_len = n;
    _tx_pos = (uint8_t)DL_UART_Main_fillTXFIFO(MOCK_RC_TX_INST, _tx_buf, n);
    _tx_bytes += n;

    if (_tx_pos < _tx_len) {
        DL_UART_Main_enableInterrupt(MOCK_RC_TX_INST, DL_UART_MAIN_INTERRUPT_TX);
    }
}

    /**
     * @brief One-time setup: park the TX interrupt, drain the RX FIFO, enable both IRQs.
     *
     * @details SysConfig enables the TX interrupt at init; with an empty FIFO and the
     * EMPTY threshold that would fire forever, so it is disabled until _tx_start().
     */
void TI_RailcomLoopback_initialize(void) {

    /* SysConfig enables the TX interrupt at init; with an empty FIFO and the
     * EMPTY threshold that would fire forever. Park it until _tx_start(). */
    DL_UART_Main_disableInterrupt(MOCK_RC_TX_INST, DL_UART_MAIN_INTERRUPT_TX);
    DL_UART_Main_clearInterruptStatus(MOCK_RC_TX_INST, DL_UART_MAIN_INTERRUPT_TX);
    NVIC_EnableIRQ(MOCK_RC_TX_INST_INT_IRQN);

    /* Drain anything the receiver caught before the gate logic was live. */
    while (!DL_UART_Main_isRXFIFOEmpty(RAILCOM_RX_INST)) {
        (void)DL_UART_Main_receiveData(RAILCOM_RX_INST);
    }
    NVIC_EnableIRQ(RAILCOM_RX_INST_INT_IRQN);
}

    /**
     * @brief The library .uart_read hook: pop one byte from the receive ring.
     *
     * @verbatim
     * @param byte  Receives the next byte when one is available.
     * @endverbatim
     *
     * @return true when a byte was returned, false when the ring is empty.
     */
bool TI_RailcomLoopback_uart_read(uint8_t *byte) {

    if (_rx_tail == _rx_head) {
        return false;
    }

    *byte = _rx_ring[_rx_tail];
    _rx_tail = (uint8_t)((_rx_tail + 1u) % RC_RX_RING_SIZE);
    return true;
}

    /**
     * @brief T_CS hook: count the cutout, latch the arm into _play, flush the receiver.
     *
     * @details The flush (hardware FIFO and ring) guarantees nothing from an earlier
     * cutout can be read as this one's reply.
     */
void TI_RailcomLoopback_on_cutout_begin(void) {

    _cutouts++;
    _window_index = 0;
    _gate_open = false;

    /* Latch the arm for this whole cutout (see _play). */
    _play = _armed;
    _armed = false;

    /* Flush: nothing from an earlier cutout may be read as this one's reply. */
    while (!DL_UART_Main_isRXFIFOEmpty(RAILCOM_RX_INST)) {
        (void)DL_UART_Main_receiveData(RAILCOM_RX_INST);
    }
    _rx_tail = _rx_head;
}

    /**
     * @brief T_TS1 / T_TS2 hook: open the gate and, in WINDOW mode, transmit Ch1 (first open) or Ch2 (second open).
     */
void TI_RailcomLoopback_on_window_open(void) {

    _gate_open = true;
    _window_index++;

    if (_play && _mode == RC_LOOPBACK_MODE_WINDOW) {

        if (_window_index == 1u) {
            _tx_start(_ch1, _n1);            /* T_TS1: Channel 1 */
        } else if (_window_index == 2u) {
            _tx_start(_ch2, _n2);            /* T_TS2: Channel 2 */
        }
    }
}

    /** @brief T_TC1 / T_CE hook: close the receiver gate. */
void TI_RailcomLoopback_on_window_close(void) {

    _gate_open = false;
}

    /**
     * @brief T_CE hook: close the gate; in LATE mode transmit Ch1 then Ch2 back to back; consume the arm.
     */
void TI_RailcomLoopback_on_cutout_end(void) {

    _gate_open = false;

    if (_play && _mode == RC_LOOPBACK_MODE_LATE) {

        /* Everything after the gate closed: Ch1 then Ch2 back to back. */
        uint8_t all[RC_TX_MAX];
        uint8_t n = 0;
        memcpy(&all[n], _ch1, _n1); n = (uint8_t)(n + _n1);
        memcpy(&all[n], _ch2, _n2); n = (uint8_t)(n + _n2);
        _tx_start(all, n);
    }

    _play = false;                           /* one reply per arm, either mode */
}

    /**
     * @brief Arm one mock reply for the next cutout.
     *
     * @details Clears _armed first so the ISR can never see a half-written arm, copies
     * both channels and the mode, then sets _armed.
     *
     * @verbatim
     * @param ch1   Channel 1 bytes.
     * @param n1    Number of Channel 1 bytes, 0..2.
     * @param ch2   Channel 2 bytes.
     * @param n2    Number of Channel 2 bytes, 0..6.
     * @param mode  When to transmit (WINDOW or LATE).
     * @endverbatim
     *
     * @return false if a length is out of range or both are zero, true when armed.
     */
bool TI_RailcomLoopback_arm(const uint8_t *ch1, uint8_t n1,
                            const uint8_t *ch2, uint8_t n2,
                            rc_loopback_mode_t mode) {

    if (n1 > RC_CH1_MAX || n2 > RC_CH2_MAX || (n1 == 0 && n2 == 0)) {
        return false;
    }

    _armed = false;                          /* the ISR must not see a half-written arm */
    _play = false;
    memcpy(_ch1, ch1, n1);
    memcpy(_ch2, ch2, n2);
    _n1 = n1;
    _n2 = n2;
    _mode = mode;
    _armed = true;
    return true;
}

    /** @brief Drop any armed or in-progress mock reply. */
void TI_RailcomLoopback_disarm(void) {

    _armed = false;
    _play = false;
}

    /**
     * @brief Snapshot the loopback counters; armed reports armed-or-playing.
     *
     * @verbatim
     * @param out  Receives the counters.
     * @endverbatim
     */
void TI_RailcomLoopback_get_stats(rc_loopback_stats_t *out) {

    out->armed = _armed || _play;
    out->cutouts = _cutouts;
    out->rx_accepted = _rx_accepted;
    out->rx_dropped = _rx_dropped;
    out->tx_bytes = _tx_bytes;
}

    /** @brief Zero the loopback counters. */
void TI_RailcomLoopback_reset_stats(void) {

    _cutouts = 0;
    _rx_accepted = 0;
    _rx_dropped = 0;
    _tx_bytes = 0;
}

    /**
     * @brief RailCom receiver ISR: one interrupt per byte (RX FIFO threshold = one entry).
     *
     * @details Drains the FIFO. A byte that arrives with the gate closed is dropped
     * and counted (it can never become a datagram); otherwise it is pushed on the
     * ring unless the ring is full.
     */
void RAILCOM_RX_INST_IRQHandler(void) {

    switch (DL_UART_Main_getPendingInterrupt(RAILCOM_RX_INST)) {

        case DL_UART_MAIN_IIDX_RX:
            while (!DL_UART_Main_isRXFIFOEmpty(RAILCOM_RX_INST)) {

                uint8_t byte = DL_UART_Main_receiveData(RAILCOM_RX_INST);

                if (!_gate_open) {
                    _rx_dropped++;           /* outside every window: never a datagram */
                    continue;
                }

                uint8_t next_head = (uint8_t)((_rx_head + 1u) % RC_RX_RING_SIZE);
                if (next_head != _rx_tail) {
                    _rx_ring[_rx_head] = byte;
                    _rx_head = next_head;
                    _rx_accepted++;
                }
            }
            break;

        default:
            break;
    }
}

    /**
     * @brief Mock transmitter ISR: top up the TX FIFO until the queued reply is out, then park the interrupt.
     */
void MOCK_RC_TX_INST_IRQHandler(void) {

    switch (DL_UART_Main_getPendingInterrupt(MOCK_RC_TX_INST)) {

        case DL_UART_MAIN_IIDX_TX:
            if (_tx_pos < _tx_len) {
                _tx_pos = (uint8_t)(_tx_pos + DL_UART_Main_fillTXFIFO(
                              MOCK_RC_TX_INST, &_tx_buf[_tx_pos], (uint32_t)(_tx_len - _tx_pos)));
            }
            if (_tx_pos >= _tx_len) {
                DL_UART_Main_disableInterrupt(MOCK_RC_TX_INST, DL_UART_MAIN_INTERRUPT_TX);
            }
            break;

        default:
            break;
    }
}

#else  /* loopback compiled out: keep the linker happy for the hook call sites */

    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_initialize(void) {}
    /**
     * @brief Stub: loopback compiled out.
     *
     * @param byte  Unused.
     *
     * @return Always false.
     */
bool TI_RailcomLoopback_uart_read(uint8_t *byte) { (void)byte; return false; }
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_on_cutout_begin(void) {}
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_on_cutout_end(void) {}
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_on_window_open(void) {}
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_on_window_close(void) {}
    /**
     * @brief Stub: loopback compiled out.
     *
     * @verbatim
     * @param ch1   Unused.
     * @param n1    Unused.
     * @param ch2   Unused.
     * @param n2    Unused.
     * @param mode  Unused.
     * @endverbatim
     *
     * @return Always false.
     */
bool TI_RailcomLoopback_arm(const uint8_t *ch1, uint8_t n1, const uint8_t *ch2, uint8_t n2,
                            rc_loopback_mode_t mode) {
    (void)ch1; (void)n1; (void)ch2; (void)n2; (void)mode; return false;
}
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_disarm(void) {}
    /**
     * @brief Stub: loopback compiled out; zeroes the counters.
     *
     * @verbatim
     * @param out  Receives all-zero counters.
     * @endverbatim
     */
void TI_RailcomLoopback_get_stats(rc_loopback_stats_t *out) { memset(out, 0, sizeof(*out)); }
    /** @brief Stub: loopback compiled out. */
void TI_RailcomLoopback_reset_stats(void) {}

#endif /* DCC_COMPILE_COMMAND_STATION && DCC_COMPILE_RAILCOM */
