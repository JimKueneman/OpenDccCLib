// ti_driverlib_railcom_loopback.c
//
// RailCom loopback for the HIL bench. See the header for the design.
//
// ISR vs MAIN-LOOP SAFETY: the receive ring is single-producer (RX ISR writes
// _rx_head) / single-consumer (uart_read, main loop, writes _rx_tail). The one
// exception is the flush at cutout begin, which resets _rx_tail from the cutout
// timer ISR; the library only reads the ring after a cutout COMPLETES, ~7 ms
// before the next one begins, so on this bench the flush never races a read.

#include "ti_driverlib_railcom_loopback.h"
#include "ti_msp_dl_config.h"
#include "dcc_user_config.h"
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_interrupt.h>
#include <string.h>

#if defined(DCC_COMPILE_COMMAND_STATION) && defined(DCC_COMPILE_RAILCOM)

#define RC_RX_RING_SIZE 32u
#define RC_CH1_MAX      2u
#define RC_CH2_MAX      6u
#define RC_TX_MAX       (RC_CH1_MAX + RC_CH2_MAX)

/* --- receive side ------------------------------------------------------- */
static volatile uint8_t  _rx_ring[RC_RX_RING_SIZE];
static volatile uint8_t  _rx_head = 0;          /* RX ISR writes */
static volatile uint8_t  _rx_tail = 0;          /* uart_read (main loop) writes; cutout-begin flush resets */
static volatile bool     _gate_open = false;    /* true only while a Ch1/Ch2 window is open */

/* --- mock transmit side ------------------------------------------------- */
/* An arm from the UART command (main loop) can land at any point of the packet
 * cycle, including inside a cutout whose Channel 1 window has already opened.
 * So the arm only sets _armed; the cutout-begin hook latches it into _play for
 * that whole cutout, and the window / cutout-end hooks act on _play. A reply
 * therefore always plays complete on the NEXT cutout, never half of this one. */
static volatile bool               _armed = false;   /* set by arm(), consumed at cutout begin */
static volatile bool               _play  = false;   /* this cutout carries the reply */
static volatile rc_loopback_mode_t _mode = RC_LOOPBACK_MODE_WINDOW;
static uint8_t                     _ch1[RC_CH1_MAX];
static uint8_t                     _ch2[RC_CH2_MAX];
static volatile uint8_t            _n1 = 0;
static volatile uint8_t            _n2 = 0;
static volatile uint8_t            _window_index = 0;   /* windows opened in this cutout: 1 = Ch1, 2 = Ch2 */

static uint8_t          _tx_buf[RC_TX_MAX];
static volatile uint8_t _tx_len = 0;
static volatile uint8_t _tx_pos = 0;

/* --- counters ----------------------------------------------------------- */
static volatile uint32_t _cutouts = 0;
static volatile uint32_t _rx_accepted = 0;
static volatile uint32_t _rx_dropped = 0;
static volatile uint32_t _tx_bytes = 0;

/* Queue n bytes on the mock transmitter: fill the TX FIFO now, let the TX
 * interrupt top it up. Fast enough to call from the cutout timer ISR. */
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

bool TI_RailcomLoopback_uart_read(uint8_t *byte) {

    if (_rx_tail == _rx_head) {
        return false;
    }

    *byte = _rx_ring[_rx_tail];
    _rx_tail = (uint8_t)((_rx_tail + 1u) % RC_RX_RING_SIZE);
    return true;
}

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

void TI_RailcomLoopback_on_window_close(void) {

    _gate_open = false;
}

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

void TI_RailcomLoopback_disarm(void) {

    _armed = false;
    _play = false;
}

void TI_RailcomLoopback_get_stats(rc_loopback_stats_t *out) {

    out->armed = _armed || _play;
    out->cutouts = _cutouts;
    out->rx_accepted = _rx_accepted;
    out->rx_dropped = _rx_dropped;
    out->tx_bytes = _tx_bytes;
}

void TI_RailcomLoopback_reset_stats(void) {

    _cutouts = 0;
    _rx_accepted = 0;
    _rx_dropped = 0;
    _tx_bytes = 0;
}

/* RailCom receiver: one interrupt per byte (RX FIFO threshold = one entry). */
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

/* Mock transmitter: top up the TX FIFO until the queued reply is out. */
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

void TI_RailcomLoopback_initialize(void) {}
bool TI_RailcomLoopback_uart_read(uint8_t *byte) { (void)byte; return false; }
void TI_RailcomLoopback_on_cutout_begin(void) {}
void TI_RailcomLoopback_on_cutout_end(void) {}
void TI_RailcomLoopback_on_window_open(void) {}
void TI_RailcomLoopback_on_window_close(void) {}
bool TI_RailcomLoopback_arm(const uint8_t *ch1, uint8_t n1, const uint8_t *ch2, uint8_t n2,
                            rc_loopback_mode_t mode) {
    (void)ch1; (void)n1; (void)ch2; (void)n2; (void)mode; return false;
}
void TI_RailcomLoopback_disarm(void) {}
void TI_RailcomLoopback_get_stats(rc_loopback_stats_t *out) { memset(out, 0, sizeof(*out)); }
void TI_RailcomLoopback_reset_stats(void) {}

#endif /* DCC_COMPILE_COMMAND_STATION && DCC_COMPILE_RAILCOM */
