// wfplayer_engine.h -- generic (level, duration) segment playback engine.
//
// DCC-agnostic: plays a list of (level, duration_us) segments on DCC_OUT using a
// one-shot, DOWN-counting 1 MHz timer (PLAYBACK_TIMER), reloaded per segment. All
// signal meaning lives in the Python host -- see PROTOCOL.md / IMPLEMENTATION.md.
//
// Segment packing (uint32): bit 31 = level (1 = high), bits 0..30 = duration in
// microseconds. The host guarantees 1 <= duration <= 65535 (it splits longer runs).

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
