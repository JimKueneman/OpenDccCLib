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
 * @file wfplayer_engine.c
 * @brief see wfplayer_engine.h.
 *
 * @details The playback timer (PLAYBACK_TIMER) is a ONE-SHOT, down-counting timer at 1 MHz
 * (prescale 40 from 40 MHz -> 1 us ticks), highest NVIC priority. Each segment:
 * set the pin to its level, load (duration_us - 1), start the one-shot; the ZERO
 * interrupt fires at the end and arms the next segment. Identical pattern to the
 * decoder example's ack_pulse_driver.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "wfplayer_engine.h"
#include "ti_msp_dl_config.h"
#include <ti/driverlib/driverlib.h>

    /** @brief Packed segment buffer. */
static uint32_t          _buf[WF_MAX_SEGMENTS];
    /** @brief Loaded segment count (written by the main loop only). */
static uint16_t          _n        = 0;     /* segment count (written by main loop only) */

    /** @brief Index of the segment on the output. */
static volatile uint16_t _idx      = 0;
    /** @brief Completed passes in the current play. */
static volatile uint32_t _rep      = 0;
    /** @brief Requested passes; 0 = continuous. */
static volatile uint32_t _count    = 0;     /* 0 = continuous */
    /** @brief Playback running. */
static volatile bool     _playing  = false;
    /** @brief A finite play completed and has not been reported yet. */
static volatile bool     _done     = false;

    /** @brief TRIG output armed. */
static volatile bool     _trig_armed = false;
    /** @brief Segment index during which TRIG is high. */
static volatile uint16_t _trig_at    = 0;

/* ---- low-level pin helpers ---- */
    /**
     * @brief Drive DCC_OUT (PB1).
     *
     * @param level  Non-zero = high.
     */
static inline void _dcc(uint32_t level) {
    if (level) DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_DCC_OUT_PIN);
    else       DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_DCC_OUT_PIN);
}
    /**
     * @brief Drive TRIG (PB3).
     *
     * @param high  true = high.
     */
static inline void _trig(bool high) {
    if (high) DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_TRIG_PIN);
    else      DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_TRIG_PIN);
}

    /**
     * @brief Put segment i on the output and start its one-shot.
     *
     * @details The edge happens here; the timer is reloaded with duration - 1 and
     * restarted, and TRIG is driven high only during the chosen segment.
     *
     * @param i  Segment index.
     */
static inline void _arm_segment(uint16_t i) {
    _dcc(WF_SEG_LEVEL(_buf[i]));
    DL_TimerA_setLoadValue(PLAYBACK_TIMER_INST, (uint16_t)(WF_SEG_DUR(_buf[i]) - 1u));
    DL_TimerA_startCounter(PLAYBACK_TIMER_INST);
    _trig(_trig_armed && (i == _trig_at));
}

    /** @brief Reset all state, stop the timer, park DCC_OUT and TRIG low. */
void WfEngine_initialize(void) {
    _n = 0; _idx = 0; _rep = 0; _count = 0;
    _playing = false; _done = false;
    _trig_armed = false; _trig_at = 0;
    DL_TimerA_stopCounter(PLAYBACK_TIMER_INST);
    _dcc(0); _trig(false);
}

    /** @brief Empty the buffer unless playing. */
void WfEngine_clear(void) {
    if (_playing) return;
    _n = 0;
}

    /**
     * @brief Append one packed segment.
     *
     * @verbatim
     * @param segment  Packed (level, duration_us) segment.
     * @endverbatim
     *
     * @return false if playing or full, true otherwise.
     */
bool WfEngine_append(uint32_t segment) {
    if (_playing || _n >= WF_MAX_SEGMENTS) return false;
    _buf[_n++] = segment;
    return true;
}

    /**
     * @brief Number of segments loaded.
     *
     * @return Segment count.
     */
uint16_t WfEngine_count(void) { return _n; }

    /**
     * @brief CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the buffer as big-endian uint32s.
     *
     * @details Matches wfplayer.py buffer_crc() so the host can verify the upload.
     *
     * @return The CRC.
     */
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

    /**
     * @brief Start playback at segment 0.
     *
     * @verbatim
     * @param count  Passes over the buffer; 0 = continuous.
     * @endverbatim
     *
     * @return false if already playing or the buffer is empty.
     */
bool WfEngine_play(uint32_t count) {
    if (_playing || _n == 0) return false;
    _idx = 0; _rep = 0; _count = count;
    _done = false; _playing = true;
    _trig(false);
    _arm_segment(0);
    return true;
}

    /** @brief Stop the timer, clear playing, park DCC_OUT and TRIG low. */
void WfEngine_stop(void) {
    DL_TimerA_stopCounter(PLAYBACK_TIMER_INST);
    _playing = false;
    _dcc(0); _trig(false);
}

    /**
     * @brief Whether playback is running.
     *
     * @return true while playing.
     */
bool     WfEngine_is_playing(void) { return _playing; }
    /**
     * @brief Segment index on the output.
     *
     * @return Segment index.
     */
uint16_t WfEngine_cur_index(void)  { return _idx; }
    /**
     * @brief Completed passes in the current play.
     *
     * @return Pass count.
     */
uint32_t WfEngine_cur_rep(void)    { return _rep; }
    /**
     * @brief Requested pass count (0 = continuous).
     *
     * @return Requested passes.
     */
uint32_t WfEngine_play_count(void) { return _count; }

    /**
     * @brief Arm or disarm the TRIG pulse for a segment index.
     *
     * @verbatim
     * @param armed          true to arm.
     * @param segment_index  Segment during which TRIG is high.
     * @endverbatim
     */
void WfEngine_set_trig(bool armed, uint16_t segment_index) {
    _trig_armed = armed;
    _trig_at = segment_index;
}

    /**
     * @brief Consume the finite-play-complete flag.
     *
     * @return true once after a finite play completes.
     */
bool WfEngine_take_done(void) {
    if (_done) { _done = false; return true; }
    return false;
}

    /**
     * @brief Playback timer ISR (TIMA1, highest priority): one-shot down-count ZERO event.
     *
     * @details Algorithm:
     * -# Raise PA15 (ISR_TIME) to bracket the ISR for scoping.
     * -# Advance to the next segment; past the end of the buffer wrap to 0 and count a pass.
     * -# If the play is finite and the pass count is reached: stop the timer, park
     *    DCC_OUT and TRIG low, clear playing, set done, drop PA15 and return.
     * -# Otherwise arm the next segment (edge + reload + restart) and drop PA15.
     */
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
