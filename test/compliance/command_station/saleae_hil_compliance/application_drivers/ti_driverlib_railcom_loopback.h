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
 * @file ti_driverlib_railcom_loopback.h
 * @brief RailCom loopback (HIL only): the command station's REAL RailCom receive path plus a mock decoder transmitter, looped back by one jumper.
 *
 * @details   MOCK_RC_TX (UART1 TX, PB6 / J2.13) --jumper--> RAILCOM_RX (UART2 RX, PB16 / J2.11)
 *                                                   ^ Saleae D6 (blue) taps this pin
 *
 * Receive side (what a real station has): a 250 kbaud UART whose bytes the DCC
 * library pulls through its .uart_read hook. The receiver is GATED by the
 * library's own channel-window hooks (.uart_rx_enable / .uart_rx_disable): a byte
 * that arrives while no window is open is dropped and counted, and the ring is
 * flushed at every cutout begin, so a byte can never be attributed to the wrong
 * cutout. No bit-banging: both directions are hardware UART peripherals.
 *
 * Mock side (bench stimulus): `RC MOCK` arms ONE reply. In WINDOW mode the
 * cutout state machine's window-open hook starts the Channel 1 bytes at T_TS1
 * and the Channel 2 bytes at T_TS2 -- where a decoder transmits -- through the
 * TX FIFO and TX interrupt, so nothing blocks inside the cutout timer ISR. In
 * LATE mode the same bytes go out at T_CE (cutout end, gate closed), to prove
 * that bytes outside a window never become a datagram. The arm is consumed by
 * the next cutout either way.
 *
 * All timing hooks below run in the RailCom cutout timer ISR context.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __TI_DRIVERLIB_RAILCOM_LOOPBACK__
#define __TI_DRIVERLIB_RAILCOM_LOOPBACK__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RC_LOOPBACK_MODE_WINDOW = 0,   /* Ch1 bytes at T_TS1, Ch2 bytes at T_TS2 (normal decoder timing) */
    RC_LOOPBACK_MODE_LATE   = 1    /* everything at T_CE, after the receive gate has closed           */
} rc_loopback_mode_t;

typedef struct {
    bool     armed;
    uint32_t cutouts;       /* cutouts begun since the last reset */
    uint32_t rx_accepted;   /* bytes received while a window was open */
    uint32_t rx_dropped;    /* bytes received with the gate closed (outside any window) */
    uint32_t tx_bytes;      /* bytes the mock transmitter queued */
} rc_loopback_stats_t;

// One-time setup after SYSCFG_DL_init(): enables the two UART interrupts and
// parks the transmitter idle.
extern void TI_RailcomLoopback_initialize(void);

// The library's .uart_read hook: pop one received byte. Main-loop context.
extern bool TI_RailcomLoopback_uart_read(uint8_t *byte);

// Cutout timing hooks, called from the DCC driver's cutout/window functions.
extern void TI_RailcomLoopback_on_cutout_begin(void);   /* T_CS  (begin_railcom_cutout) */
extern void TI_RailcomLoopback_on_cutout_end(void);     /* T_CE  (end_railcom_cutout)   */
extern void TI_RailcomLoopback_on_window_open(void);    /* T_TS1 / T_TS2 (uart_rx_enable)  */
extern void TI_RailcomLoopback_on_window_close(void);   /* T_TC1 / T_CE  (uart_rx_disable) */

// Arm one mock reply: up to 2 raw Channel 1 bytes and up to 6 raw Channel 2
// bytes (already 4/8-encoded by the host). Either may be empty, not both.
// Returns false if the lengths are out of range.
extern bool TI_RailcomLoopback_arm(const uint8_t *ch1, uint8_t n1,
                                   const uint8_t *ch2, uint8_t n2,
                                   rc_loopback_mode_t mode);
extern void TI_RailcomLoopback_disarm(void);

extern void TI_RailcomLoopback_get_stats(rc_loopback_stats_t *out);
extern void TI_RailcomLoopback_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_RAILCOM_LOOPBACK__ */
