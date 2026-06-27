// wfplayer_engine.c -- see wfplayer_engine.h.
//
// The playback timer (PLAYBACK_TIMER) is a ONE-SHOT, down-counting timer at 1 MHz
// (prescale 40 from 40 MHz -> 1 us ticks), highest NVIC priority. Each segment:
// set the pin to its level, load (duration_us - 1), start the one-shot; the ZERO
// interrupt fires at the end and arms the next segment. Identical pattern to the
// decoder example's ack_pulse_driver.

#include "wfplayer_engine.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>

static uint32_t          _buf[WF_MAX_SEGMENTS];
static uint16_t          _n        = 0;     /* segment count (written by main loop only) */

static volatile uint16_t _idx      = 0;
static volatile uint32_t _rep      = 0;
static volatile uint32_t _count    = 0;     /* 0 = continuous */
static volatile bool     _playing  = false;
static volatile bool     _done     = false;

static volatile bool     _trig_armed = false;
static volatile uint16_t _trig_at    = 0;

/* ---- low-level pin helpers ---- */
static inline void _dcc(uint32_t level) {
    if (level) DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_DCC_OUT_PIN);
    else       DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_DCC_OUT_PIN);
}
static inline void _trig(bool high) {
    if (high) DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_TRIG_PIN);
    else      DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_TRIG_PIN);
}

/* Set the pin for segment i (edge happens here), reload + restart the one-shot,
 * and drive TRIG high only during the chosen segment. */
static inline void _arm_segment(uint16_t i) {
    _dcc(WF_SEG_LEVEL(_buf[i]));
    DL_TimerA_setLoadValue(PLAYBACK_TIMER_INST, (uint16_t)(WF_SEG_DUR(_buf[i]) - 1u));
    DL_TimerA_startCounter(PLAYBACK_TIMER_INST);
    _trig(_trig_armed && (i == _trig_at));
}

void WfEngine_initialize(void) {
    _n = 0; _idx = 0; _rep = 0; _count = 0;
    _playing = false; _done = false;
    _trig_armed = false; _trig_at = 0;
    DL_TimerA_stopCounter(PLAYBACK_TIMER_INST);
    _dcc(0); _trig(false);
}

void WfEngine_clear(void) {
    if (_playing) return;
    _n = 0;
}

bool WfEngine_append(uint32_t segment) {
    if (_playing || _n >= WF_MAX_SEGMENTS) return false;
    _buf[_n++] = segment;
    return true;
}

uint16_t WfEngine_count(void) { return _n; }

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the buffer serialized as
 * big-endian uint32s -- matches wfplayer.py buffer_crc(). */
uint16_t WfEngine_crc16(void) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < _n; i++) {
        uint32_t s = _buf[i];
        uint8_t bytes[4] = { (uint8_t)(s >> 24), (uint8_t)(s >> 16),
                             (uint8_t)(s >> 8),  (uint8_t)s };
        for (int b = 0; b < 4; b++) {
            crc ^= (uint16_t)((uint16_t)bytes[b] << 8);
            for (int k = 0; k < 8; k++)
                crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                     : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool WfEngine_play(uint32_t count) {
    if (_playing || _n == 0) return false;
    _idx = 0; _rep = 0; _count = count;
    _done = false; _playing = true;
    _trig(false);
    _arm_segment(0);
    return true;
}

void WfEngine_stop(void) {
    DL_TimerA_stopCounter(PLAYBACK_TIMER_INST);
    _playing = false;
    _dcc(0); _trig(false);
}

bool     WfEngine_is_playing(void) { return _playing; }
uint16_t WfEngine_cur_index(void)  { return _idx; }
uint32_t WfEngine_cur_rep(void)    { return _rep; }
uint32_t WfEngine_play_count(void) { return _count; }

void WfEngine_set_trig(bool armed, uint16_t segment_index) {
    _trig_armed = armed;
    _trig_at = segment_index;
}

bool WfEngine_take_done(void) {
    if (_done) { _done = false; return true; }
    return false;
}

/* Playback timer ISR -- highest priority. One-shot, down-count, ZERO event.
 * PA15 (ISR_TIME) brackets the ISR for scoping during M1. */
void PLAYBACK_TIMER_INST_IRQHandler(void) {
    switch (DL_TimerA_getPendingInterrupt(PLAYBACK_TIMER_INST)) {
        case DL_TIMER_IIDX_ZERO: {
            DL_GPIO_setPins(GPIO_ISR_TIME_PORT, GPIO_ISR_TIME_ISR_TIME_PIN);

            uint16_t i = (uint16_t)(_idx + 1);
            if (i >= _n) {                       /* wrapped past buffer end */
                i = 0;
                _rep++;
                if (_count != 0 && _rep >= _count) {   /* finite play complete */
                    DL_TimerA_stopCounter(PLAYBACK_TIMER_INST);
                    _dcc(0); _trig(false);
                    _playing = false; _done = true;
                    DL_GPIO_clearPins(GPIO_ISR_TIME_PORT, GPIO_ISR_TIME_ISR_TIME_PIN);
                    return;
                }
            }
            _idx = i;
            _arm_segment(i);

            DL_GPIO_clearPins(GPIO_ISR_TIME_PORT, GPIO_ISR_TIME_ISR_TIME_PIN);
            break;
        }
        default:
            break;
    }
}
