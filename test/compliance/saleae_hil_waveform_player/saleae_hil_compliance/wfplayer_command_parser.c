// wfplayer_command_parser.c -- see wfplayer_command_parser.h and PROTOCOL.md.
//
// Commands: PING / ID? / CLEAR / SEG <hex8...> / N? / CRC? / TRIG <index>|OFF /
//           PLAY [count] / STOP / STATE?
// Every line gets exactly one reply line starting with "OK" or "ERR".
// Echo is off (the main loop does not call the UART echo) so the host sees only
// OK/ERR lines.

#include "wfplayer_command_parser.h"
#include "wfplayer_engine.h"
#include "application_drivers/ti_driverlib_uart_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX  280     /* >= longest SEG line (host per_line=32 -> 260 chars) */

static inline void reply(const char *s) { TI_UartDriver_write_string(s); }

/* Parse exactly 8 hex chars at p into *out; false on any non-hex char. */
static bool hex8(const char *p, uint32_t *out) {
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        char c = p[i];
        uint32_t d;
        if      (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
        else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *out = v;
    return true;
}

static void cmd_seg(char *args) {
    /* compact: drop whitespace in place */
    char *w = args, *r = args;
    while (*r) { if (*r != ' ' && *r != '\t') *w++ = *r; r++; }
    *w = '\0';

    size_t len = (size_t)(w - args);
    if (len == 0 || (len % 8) != 0) { reply("ERR badseg\r\n"); return; }

    for (size_t i = 0; i < len; i += 8) {
        uint32_t seg;
        if (!hex8(&args[i], &seg))      { reply("ERR badseg\r\n");   return; }
        if (!WfEngine_append(seg))      { reply("ERR overflow\r\n"); return; }
    }
    char out[24];
    snprintf(out, sizeof out, "OK n=%u\r\n", (unsigned)WfEngine_count());
    reply(out);
}

void WfCmdParser_initialize(void) { /* nothing to set up */ }

void WfCmdParser_process(void) {
    static char line[LINE_MAX];
    if (!TI_UartDriver_read_line(line, sizeof line)) return;

    /* split verb (up to first space) and the remaining args */
    char *args = line;
    while (*args && *args != ' ') args++;
    if (*args == ' ') { *args = '\0'; args++; }   /* terminate verb; args -> rest */

    if (strcmp(line, "PING") == 0) { reply("OK PONG\r\n"); return; }

    if (strcmp(line, "ID?") == 0) {
        reply("OK wfplayer v1 ch=1 maxseg=4096 tick_us=1\r\n"); return;
    }

    if (strcmp(line, "CLEAR") == 0) {
        if (WfEngine_is_playing()) { reply("ERR busy\r\n"); return; }
        WfEngine_clear();
        reply("OK n=0\r\n"); return;
    }

    if (strcmp(line, "SEG") == 0) {
        if (WfEngine_is_playing()) { reply("ERR busy\r\n"); return; }
        cmd_seg(args); return;
    }

    if (strcmp(line, "N?") == 0) {
        char out[24]; snprintf(out, sizeof out, "OK n=%u\r\n", (unsigned)WfEngine_count());
        reply(out); return;
    }

    if (strcmp(line, "CRC?") == 0) {
        char out[16]; snprintf(out, sizeof out, "OK %04X\r\n", (unsigned)WfEngine_crc16());
        reply(out); return;
    }

    if (strcmp(line, "TRIG") == 0) {
        if (strcmp(args, "OFF") == 0) { WfEngine_set_trig(false, 0); reply("OK\r\n"); return; }
        char *end; long idx = strtol(args, &end, 10);
        if (end == args || idx < 0 || idx > 0xFFFF) { reply("ERR syntax\r\n"); return; }
        WfEngine_set_trig(true, (uint16_t)idx);
        reply("OK\r\n"); return;
    }

    if (strcmp(line, "PLAY") == 0) {
        long cnt = 1;
        if (*args) {
            char *end; cnt = strtol(args, &end, 10);
            if (end == args || cnt < 0) { reply("ERR syntax\r\n"); return; }
        }
        if (!WfEngine_play((uint32_t)cnt)) {
            reply(WfEngine_is_playing() ? "ERR busy\r\n" : "ERR empty\r\n");
            return;
        }
        reply("OK PLAYING\r\n"); return;
    }

    if (strcmp(line, "STOP") == 0) { WfEngine_stop(); reply("OK STOPPED\r\n"); return; }

    if (strcmp(line, "STATE?") == 0) {
        char out[48];
        if (WfEngine_is_playing())
            snprintf(out, sizeof out, "OK PLAYING %lu/%lu seg %u\r\n",
                     (unsigned long)WfEngine_cur_rep(),
                     (unsigned long)WfEngine_play_count(),
                     (unsigned)WfEngine_cur_index());
        else
            snprintf(out, sizeof out, "OK IDLE\r\n");
        reply(out); return;
    }

    reply("ERR syntax\r\n");
}
