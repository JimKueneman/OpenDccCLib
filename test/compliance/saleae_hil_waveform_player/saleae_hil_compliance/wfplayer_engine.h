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
 * @file wfplayer_engine.h
 * @brief generic (level, duration) segment playback engine.
 *
 * @details DCC-agnostic: plays a list of (level, duration_us) segments on DCC_OUT using a
 * one-shot, DOWN-counting 1 MHz timer (PLAYBACK_TIMER), reloaded per segment. All
 * signal meaning lives in the Python host -- see PROTOCOL.md / IMPLEMENTATION.md.
 *
 * Segment packing (uint32): bit 31 = level (1 = high), bits 0..30 = duration in
 * microseconds. The host guarantees 1 <= duration <= 65535 (it splits longer runs).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __WFPLAYER_ENGINE__
#define __WFPLAYER_ENGINE__

#include <stdbool.h>
#include <stdint.h>

#define WF_MAX_SEGMENTS   4096           /* PROTOCOL.md: ID? maxseg */

#define WF_SEG_LEVEL(s)   (((s) >> 31) & 1u)
#define WF_SEG_DUR(s)     ((s) & 0x7FFFFFFFu)

void     WfEngine_initialize(void);                 /* park output low, idle */
void     WfEngine_clear(void);                      /* empty the buffer (ignored while playing) */
bool     WfEngine_append(uint32_t segment);         /* append one segment; false if full/playing */
uint16_t WfEngine_count(void);                      /* segments currently loaded */
uint16_t WfEngine_crc16(void);                      /* CRC-16/CCITT-FALSE over the buffer (big-endian) */

bool     WfEngine_play(uint32_t count);             /* start; count = 0 -> continuous; false if busy/empty */
void     WfEngine_stop(void);                       /* halt, park low */
bool     WfEngine_is_playing(void);
uint16_t WfEngine_cur_index(void);
uint32_t WfEngine_cur_rep(void);
uint32_t WfEngine_play_count(void);

void     WfEngine_set_trig(bool armed, uint16_t segment_index);  /* pulse TRIG when idx == this */

bool     WfEngine_take_done(void);                  /* true once, after a finite play completes */

#endif /* __WFPLAYER_ENGINE__ */
