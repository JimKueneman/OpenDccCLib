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
 * @file uart_command_parser.c
 * @brief Text command parser for the DCC command station UART interface.
 *
 * @details Reads lines from TI_UartDriver_read_line(), splits into tokens, and
 * dispatches to DCC library API calls (speed, function, accessory, service
 * mode, track power, etc.).
 *
 * STRUCTURE:
 *   - _loco_table[] tracks per-locomotive state (speed, direction, functions)
 *     so that function group commands can be sent with the correct bitmask.
 *   - UartCommandParser_process() is the main dispatch -- it matches the first
 *     token against known commands and calls the appropriate handler.
 *   - Each _cmd_*() function parses its arguments, validates them, and
 *     calls the corresponding DCC library API.
 *
 * Type "HELP" at the UART terminal to see all available commands.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "uart_command_parser.h"
#include "ti_msp_dl_config.h"
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "dcc_lib/dcc_config.h"
#include "dcc_lib/dcc_application_command_station_main_track.h"
#include "dcc_lib/dcc_application_command_station_service_track.h"
#include "dcc_lib/dcc_application_command_station_packet.h"
#include "dcc_user_config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

    /** @brief Maximum command line length in bytes, including the null terminator. */
#define CMD_LINE_MAX 128

    /** @brief Maximum tokens parsed from one command line. */
#define CMD_MAX_TOKENS 8

    /** @brief Per-locomotive state so function-group packets carry the full current bitmask. */
typedef struct {

        /** @brief Locomotive address (short, long or broadcast 0). */
    dcc_address_t address;
        /** @brief Address type that goes with address. */
    dcc_address_type_enum address_type;
        /** @brief Last commanded speed step. */
    uint8_t speed;
        /** @brief Last commanded direction; true = forward. */
    bool direction;
        /** @brief Function Group 1 bitmask: FL in bit 4, F1-F4 in bits 0-3. */
    uint8_t func_fl_f4;
        /** @brief Function Group 2a bitmask: F5-F8 in bits 0-3. */
    uint8_t func_f5_f8;
        /** @brief Function Group 2b bitmask: F9-F12 in bits 0-3. */
    uint8_t func_f9_f12;
        /** @brief F13-F20 bitmask, F13 in bit 0. */
    uint8_t func_f13_f20;
        /** @brief F21-F28 bitmask, F21 in bit 0. */
    uint8_t func_f21_f28;
        /** @brief F29-F36 bitmask, F29 in bit 0. */
    uint8_t func_f29_f36;
        /** @brief F37-F44 bitmask, F37 in bit 0. */
    uint8_t func_f37_f44;
        /** @brief F45-F52 bitmask, F45 in bit 0. */
    uint8_t func_f45_f52;
        /** @brief F53-F60 bitmask, F53 in bit 0. */
    uint8_t func_f53_f60;
        /** @brief F61-F68 bitmask, F61 in bit 0. */
    uint8_t func_f61_f68;
        /** @brief true when this table slot is in use. */
    bool active;

} loco_state_t;

    /** @brief Loco state table; one slot per loco up to USER_DEFINED_DCC_MAX_LOCOS. */
static loco_state_t _loco_table[USER_DEFINED_DCC_MAX_LOCOS];

    /** @brief Line buffer filled by TI_UartDriver_read_line(). */
static char _line_buf[CMD_LINE_MAX];
    /** @brief Scratch buffer for formatted responses. */
static char _resp_buf[128];
    /** @brief When true, SPEED and FUNC packets go on the auto-refresh list; when false they are sent once. */
static bool _auto_refresh = true;

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

    /**
     * @brief Writes a response line followed by CR LF to the terminal.
     *
     * @param msg Null-terminated response text.
     */
static void _respond(const char *msg) {

    TI_UartDriver_write_string(msg);
    TI_UartDriver_write_string("\r\n");
}

    /**
     * @brief Parses an address token such as "40", "40S" or "40L".
     *
     * @details An explicit S or L suffix forces short or long. Without a suffix the number
     * decides: 0 = broadcast, 1-127 = short, 128 and up = long. "0S" is also broadcast.
     *
     * @param token    The token string (already uppercased).
     * @param addr_out Receives the numeric address.
     * @param type_out Receives the address type.
     */
static void _parse_address(const char *token, uint16_t *addr_out,
                           dcc_address_type_enum *type_out) {

    uint16_t addr = (uint16_t)atoi(token);
    *addr_out = addr;

    /* Check for trailing S or L suffix */
    const char *p = token;
    while (*p >= '0' && *p <= '9') p++;

    if (*p == 'S') {
        *type_out = (addr == 0) ? DCC_ADDRESS_BROADCAST : DCC_ADDRESS_SHORT;
    } else if (*p == 'L') {
        *type_out = DCC_ADDRESS_LONG;
    } else {
        /* Default: 0=broadcast, 1-127=short, 128+=long */
        if (addr == 0)
            *type_out = DCC_ADDRESS_BROADCAST;
        else if (addr <= 127)
            *type_out = DCC_ADDRESS_SHORT;
        else
            *type_out = DCC_ADDRESS_LONG;
    }
}

    /**
     * @brief Finds the loco table entry for an address, allocating a fresh one if needed.
     *
     * @details A new entry starts with all functions off and direction forward.
     *
     * @param addr      Locomotive address.
     * @param addr_type Address type that goes with addr.
     *
     * @return Pointer to the entry, or NULL when the table is full.
     */
static loco_state_t *_find_or_create_loco(uint16_t addr,
                                           dcc_address_type_enum addr_type) {
    uint16_t i;

    /* Search for existing entry */
    for (i = 0; i < USER_DEFINED_DCC_MAX_LOCOS; i++) {
        if (_loco_table[i].active && _loco_table[i].address == addr &&
            _loco_table[i].address_type == addr_type)
            return &_loco_table[i];
    }

    /* Allocate new entry */
    for (i = 0; i < USER_DEFINED_DCC_MAX_LOCOS; i++) {
        if (!_loco_table[i].active) {
            memset(&_loco_table[i], 0, sizeof(loco_state_t));
            _loco_table[i].address = addr;
            _loco_table[i].address_type = addr_type;
            _loco_table[i].direction = true;
            _loco_table[i].active = true;
            return &_loco_table[i];
        }
    }

    return NULL;
}

    /**
     * @brief Splits a line on spaces and tabs in place.
     *
     * @details Each separator is overwritten with a null so the tokens point into line.
     *
     * @param line       The line to split; modified in place.
     * @param tokens     Receives a pointer to the start of each token.
     * @param max_tokens Capacity of tokens; extra tokens are ignored.
     *
     * @return Number of tokens stored.
     */
static int _tokenize(char *line, char *tokens[], int max_tokens) {

    int count = 0;
    char *p = line;

    while (*p && count < max_tokens) {

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        tokens[count++] = p;

        /* Advance to next whitespace */
        while (*p && *p != ' ' && *p != '\t')
            p++;

        if (*p) {
            *p = '\0';
            p++;
        }
    }

    return count;
}

    /**
     * @brief Schedules a packet on the main track, either as an auto-refresh entry or a one-shot.
     *
     * @details No handler overrides repeat_count. Every builder in the library sets a spec-correct
     * default (DCC_REPEAT_* in dcc_defines.h: CV writes 2, verifies 1, date 3, time 1, accessory
     * NOP/stop 1, everything else 2). An application that wants a different count may still write
     * packet.repeat_count after the builder returns; 0 means the scheduler never sends it.
     *
     * @param packet       Packet built by one of the DccApplicationCommandStationPacket_load_* functions.
     * @param address      Address the packet targets, used for auto-refresh slot matching.
     * @param tag          Packet class tag so a refresh entry replaces its predecessor.
     * @param priority     Scheduler priority.
     * @param auto_refresh true = add to the auto-refresh list, false = send once.
     *
     * @return true if the scheduler accepted the packet, false if it is full.
     */
static bool _schedule_main_track(const dcc_packet_t *packet, dcc_address_t address,
                                 dcc_tag_enum tag, dcc_priority_enum priority,
                                 bool auto_refresh) {

    if (auto_refresh) {
        return DccApplicationCommandStationMainTrack_add_to_auto_refresh(packet, address, tag, priority);
    }
    return DccApplicationCommandStationMainTrack_send_packet(packet, address, tag, priority);
}

    /**
     * @brief Uppercases an ASCII string in place.
     *
     * @param s Null-terminated string to convert.
     */
static void _strupper(char *s) {

    while (*s) {
        if (*s >= 'a' && *s <= 'z')
            *s -= 32;
        s++;
    }
}

/* ========================================================================== */
/* Command handlers                                                           */
/* ========================================================================== */

    /**
     * @brief POWER ON|OFF: switches main track power through the library.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_power(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: POWER ON|OFF");
        return;
    }

    if (strcmp(tokens[1], "ON") == 0) {
        DccApplicationCommandStationMainTrack_power_on();
        _respond("OK: track power ON");
    } else if (strcmp(tokens[1], "OFF") == 0) {
        DccApplicationCommandStationMainTrack_power_off();
        _respond("OK: track power OFF");
    } else {
        _respond("ERR: usage: POWER ON|OFF");
    }
}

    /**
     * @brief REFRESH ON|OFF: selects whether later SPEED and FUNC packets auto-refresh or are sent once.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_refresh(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: REFRESH ON|OFF");
        return;
    }

    if (strcmp(tokens[1], "ON") == 0) {
        _auto_refresh = true;
        _respond("OK: auto-refresh ON");
    } else if (strcmp(tokens[1], "OFF") == 0) {
        _auto_refresh = false;
        _respond("OK: auto-refresh OFF");
    } else {
        _respond("ERR: usage: REFRESH ON|OFF");
    }
}

    /**
     * @brief SPEED <addr> <speed> <FWD|REV> [14|28|128]: builds and schedules a speed packet.
     *
     * @details Algorithm:
     * -# Parse the address, speed step, direction and optional step mode (default 128).
     * -# Find or create the loco table entry and record speed and direction.
     * -# Build the 14, 28 or 128 step packet for the chosen mode.
     * -# Schedule it with DCC_TAG_SPEED, honouring the REFRESH setting.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_speed(char *tokens[], int count) {

    if (count < 4) {
        _respond("ERR: usage: SPEED <addr> <speed> <FWD|REV> [14|28|128]");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);
    uint8_t speed = (uint8_t)atoi(tokens[2]);
    bool direction = (strcmp(tokens[3], "REV") != 0);

    /* Optional speed mode: 14, 28, or 128 (default) */
    uint8_t mode = 128;
    if (count >= 5) {
        mode = (uint8_t)atoi(tokens[4]);
    }

    loco_state_t *loco = _find_or_create_loco(addr, addr_type);
    if (!loco) {
        _respond("ERR: loco table full");
        return;
    }

    loco->speed = speed;
    loco->direction = direction;

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    bool ok = false;

    switch (mode) {
        case 14:
            ok = DccApplicationCommandStationPacket_load_speed_14(&packet, loco->address,
                                            loco->address_type, speed,
                                            direction, true);
            break;
        case 28:
            ok = DccApplicationCommandStationPacket_load_speed_28(&packet, loco->address,
                                            loco->address_type, speed,
                                            direction);
            break;
        case 128:
            ok = DccApplicationCommandStationPacket_load_speed_128(&packet, loco->address,
                                             loco->address_type, speed,
                                             direction);
            break;
        default:
            _respond("ERR: mode must be 14, 28, or 128");
            return;
    }

    if (!ok) {
        _respond("ERR: invalid speed parameters");
        return;
    }

    if (!_schedule_main_track(&packet, loco->address, DCC_TAG_SPEED,
                              DCC_PRIORITY_SPEED, _auto_refresh)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: SPEED addr=%u speed=%u dir=%s mode=%u",
             addr, speed, direction ? "FWD" : "REV", mode);
    _respond(_resp_buf);
}

    /**
     * @brief ESTOP [addr]: emergency stop for one loco (128-step speed 1) or a broadcast stop packet.
     *
     * @details Always sent as a one-shot at DCC_PRIORITY_ESTOP, never auto-refreshed.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_estop(char *tokens[], int count) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (count >= 2) {
        /* E-stop specific address */
        uint16_t addr;
        dcc_address_type_enum addr_type;
        _parse_address(tokens[1], &addr, &addr_type);

        /* Speed 1 = e-stop in 128-step mode */
        if (!DccApplicationCommandStationPacket_load_speed_128(&packet, addr, addr_type, 1, true)) {
            _respond("ERR: invalid address");
            return;
        }

        if (!_schedule_main_track(&packet, addr, DCC_TAG_SPEED,
                                  DCC_PRIORITY_ESTOP, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        snprintf(_resp_buf, sizeof(_resp_buf), "OK: ESTOP addr=%u", addr);
        _respond(_resp_buf);
    } else {
        /* Broadcast emergency stop */
        DccApplicationCommandStationPacket_load_estop_all(&packet, true);

        if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                                  DCC_PRIORITY_ESTOP, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        _respond("OK: ESTOP broadcast");
    }
}

    /**
     * @brief STOP: broadcast controlled stop (S-9.2 baseline 01DC000S with S=0).
     *
     * @details All decoders decelerate to a stop. ESTOP is the emergency form (S=1).
     */
static void _cmd_stop(void) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    DccApplicationCommandStationPacket_load_estop_all(&packet, false);  /* S=0 */

    if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                              DCC_PRIORITY_ESTOP, false)) {
        _respond("ERR: scheduler full");
        return;
    }
    _respond("OK: broadcast controlled stop (S=0)");
}

    /**
     * @brief FUNC <addr> <0-68> <ON|OFF>: updates one function bit and sends its whole group.
     *
     * @details Algorithm:
     * -# Parse the address, function number and state; find or create the loco entry.
     * -# Pick the group by function number (FL/F1-F4, F5-F8, F9-F12, then eight-wide groups up to F68).
     * -# Set or clear the bit in that group's stored bitmask.
     * -# Build the group packet from the stored mask and schedule it with the group's tag.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_func(char *tokens[], int count) {

    if (count < 4) {
        _respond("ERR: usage: FUNC <addr> <0-68> <ON|OFF>");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);
    uint8_t func_num = (uint8_t)atoi(tokens[2]);
    bool state = (strcmp(tokens[3], "ON") == 0);

    loco_state_t *loco = _find_or_create_loco(addr, addr_type);
    if (!loco) {
        _respond("ERR: loco table full");
        return;
    }

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    bool ok = false;

    if (func_num <= 4) {
        /* Function Group 1: FL (bit4), F1-F4 (bits 0-3) */
        uint8_t bit;
        if (func_num == 0)
            bit = (1u << 4);  /* FL */
        else
            bit = (1u << (func_num - 1));

        if (state)
            loco->func_fl_f4 |= bit;
        else
            loco->func_fl_f4 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_group_1(&packet, loco->address,
                                            loco->address_type, loco->func_fl_f4);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_GROUP_1,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 8) {
        /* Function Group 2a: F5-F8 */
        uint8_t bit = (1u << (func_num - 5));

        if (state)
            loco->func_f5_f8 |= bit;
        else
            loco->func_f5_f8 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_group_2a(&packet, loco->address,
                                             loco->address_type, loco->func_f5_f8);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_GROUP_2A,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 12) {
        /* Function Group 2b: F9-F12 */
        uint8_t bit = (1u << (func_num - 9));

        if (state)
            loco->func_f9_f12 |= bit;
        else
            loco->func_f9_f12 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_group_2b(&packet, loco->address,
                                             loco->address_type, loco->func_f9_f12);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_GROUP_2B,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 20) {
        /* F13-F20 */
        uint8_t bit = (1u << (func_num - 13));

        if (state)
            loco->func_f13_f20 |= bit;
        else
            loco->func_f13_f20 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f13_f20(&packet, loco->address,
                                            loco->address_type, loco->func_f13_f20);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F13_F20,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 28) {
        /* F21-F28 */
        uint8_t bit = (1u << (func_num - 21));

        if (state)
            loco->func_f21_f28 |= bit;
        else
            loco->func_f21_f28 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f21_f28(&packet, loco->address,
                                            loco->address_type, loco->func_f21_f28);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F21_F28,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 36) {
        /* F29-F36 */
        uint8_t bit = (1u << (func_num - 29));

        if (state)
            loco->func_f29_f36 |= bit;
        else
            loco->func_f29_f36 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f29_f36(&packet, loco->address,
                                            loco->address_type, loco->func_f29_f36);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F29_F36,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 44) {
        /* F37-F44 */
        uint8_t bit = (1u << (func_num - 37));

        if (state)
            loco->func_f37_f44 |= bit;
        else
            loco->func_f37_f44 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f37_f44(&packet, loco->address,
                                            loco->address_type, loco->func_f37_f44);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F37_F44,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 52) {
        /* F45-F52 */
        uint8_t bit = (1u << (func_num - 45));

        if (state)
            loco->func_f45_f52 |= bit;
        else
            loco->func_f45_f52 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f45_f52(&packet, loco->address,
                                            loco->address_type, loco->func_f45_f52);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F45_F52,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 60) {
        /* F53-F60 */
        uint8_t bit = (1u << (func_num - 53));

        if (state)
            loco->func_f53_f60 |= bit;
        else
            loco->func_f53_f60 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f53_f60(&packet, loco->address,
                                            loco->address_type, loco->func_f53_f60);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F53_F60,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else if (func_num <= 68) {
        /* F61-F68 */
        uint8_t bit = (1u << (func_num - 61));

        if (state)
            loco->func_f61_f68 |= bit;
        else
            loco->func_f61_f68 &= ~bit;

        ok = DccApplicationCommandStationPacket_load_func_f61_f68(&packet, loco->address,
                                            loco->address_type, loco->func_f61_f68);
        if (ok)
            ok = _schedule_main_track(&packet, loco->address, DCC_TAG_FUNC_F61_F68,
                                      DCC_PRIORITY_FUNCTION, _auto_refresh);

    } else {
        _respond("ERR: function number must be 0-68");
        return;
    }

    if (!ok) {
        _respond("ERR: failed to schedule function command");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: FUNC addr=%u F%u=%s",
             addr, func_num, state ? "ON" : "OFF");
    _respond(_resp_buf);
}

    /**
     * @brief ACC commands for basic accessory decoders.
     *
     * @details ACC <board> <pair> <ON|OFF> sends a basic accessory packet. ACC CV WRITE|VERIFY|BIT
     * sends an ops-mode CV packet addressed to a basic accessory decoder. The board argument is the
     * 9-bit board address; the library derives the on-wire form.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_acc(char *tokens[], int count) {

    /* ACC CV WRITE <board> <pair> <cv> <value> */
    /* ACC CV VERIFY <board> <pair> <cv> <value> */
    /* ACC CV BIT <board> <pair> <cv> <bit_pos> <0|1> */
    if (count >= 2 && strcmp(tokens[1], "CV") == 0) {

        if (count < 6) {
            _respond("ERR: usage: ACC CV WRITE|VERIFY|BIT <board> <pair> <cv> <value>");
            return;
        }

        uint16_t board = (uint16_t)atoi(tokens[3]);
        uint8_t pair = (uint8_t)atoi(tokens[4]);
        uint16_t cv = (uint16_t)atoi(tokens[5]);
        uint8_t value = (count >= 7) ? (uint8_t)atoi(tokens[6]) : 0;

        dcc_packet_t packet;
        memset(&packet, 0, sizeof(packet));
        bool ok = false;

        if (strcmp(tokens[2], "WRITE") == 0 && count >= 7) {
            ok = DccApplicationCommandStationPacket_load_accessory_basic_cv_write(&packet, board, pair, cv, value);
        } else if (strcmp(tokens[2], "VERIFY") == 0 && count >= 7) {
            ok = DccApplicationCommandStationPacket_load_accessory_basic_cv_verify(&packet, board, pair, cv, value);
        } else if (strcmp(tokens[2], "BIT") == 0 && count >= 8) {
            uint8_t bit_pos = value;
            bool bit_val = (atoi(tokens[7]) != 0);
            ok = DccApplicationCommandStationPacket_load_accessory_basic_cv_bit(&packet, board, pair, cv,
                                                          bit_pos, bit_val, true);
        } else {
            _respond("ERR: usage: ACC CV WRITE|VERIFY|BIT <board> <pair> <cv> <value>");
            return;
        }

        if (!ok) {
            _respond("ERR: invalid ACC CV parameters");
            return;
        }

        if (!_schedule_main_track(&packet, board, DCC_TAG_CV,
                                  DCC_PRIORITY_CV, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        _respond("OK: ACC CV command scheduled");
        return;
    }

    /* ACC <board> <pair> <ON|OFF> */
    if (count < 4) {
        _respond("ERR: usage: ACC <board> <pair> <ON|OFF>");
        return;
    }

    uint16_t board = (uint16_t)atoi(tokens[1]);
    uint8_t pair = (uint8_t)atoi(tokens[2]);
    bool activate = (strcmp(tokens[3], "ON") == 0);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    if (!DccApplicationCommandStationPacket_load_accessory_basic(&packet, board, pair, activate)) {
        _respond("ERR: invalid accessory parameters");
        return;
    }

    if (!_schedule_main_track(&packet, board, DCC_TAG_ACCESSORY,
                              DCC_PRIORITY_ACCESSORY, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACC board=%u pair=%u %s",
             board, pair, activate ? "ON" : "OFF");
    _respond(_resp_buf);
}

    /**
     * @brief ACCE commands for extended accessory decoders.
     *
     * @details ACCE <addr> <aspect> sends an extended accessory (signal aspect) packet.
     * ACCE CV WRITE|VERIFY|BIT sends an ops-mode CV packet addressed to an extended accessory decoder.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_acce(char *tokens[], int count) {

    /* ACCE CV WRITE <addr> <cv> <value> */
    /* ACCE CV VERIFY <addr> <cv> <value> */
    /* ACCE CV BIT <addr> <cv> <bit_pos> <0|1> */
    if (count >= 2 && strcmp(tokens[1], "CV") == 0) {

        if (count < 5) {
            _respond("ERR: usage: ACCE CV WRITE|VERIFY|BIT <addr> <cv> <value>");
            return;
        }

        uint16_t addr = (uint16_t)atoi(tokens[3]);
        uint16_t cv = (uint16_t)atoi(tokens[4]);
        uint8_t value = (count >= 6) ? (uint8_t)atoi(tokens[5]) : 0;

        dcc_packet_t packet;
        memset(&packet, 0, sizeof(packet));
        bool ok = false;

        if (strcmp(tokens[2], "WRITE") == 0 && count >= 6) {
            ok = DccApplicationCommandStationPacket_load_accessory_extended_cv_write(&packet, addr, cv, value);
        } else if (strcmp(tokens[2], "VERIFY") == 0 && count >= 6) {
            ok = DccApplicationCommandStationPacket_load_accessory_extended_cv_verify(&packet, addr, cv, value);
        } else if (strcmp(tokens[2], "BIT") == 0 && count >= 7) {
            uint8_t bit_pos = value;
            bool bit_val = (atoi(tokens[6]) != 0);
            ok = DccApplicationCommandStationPacket_load_accessory_extended_cv_bit(&packet, addr, cv,
                                                             bit_pos, bit_val, true);
        } else {
            _respond("ERR: usage: ACCE CV WRITE|VERIFY|BIT <addr> <cv> <value>");
            return;
        }

        if (!ok) {
            _respond("ERR: invalid ACCE CV parameters");
            return;
        }

        if (!_schedule_main_track(&packet, addr, DCC_TAG_CV,
                                  DCC_PRIORITY_CV, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        _respond("OK: ACCE CV command scheduled");
        return;
    }

    /* ACCE <addr> <aspect> */
    if (count < 3) {
        _respond("ERR: usage: ACCE <addr> <aspect>");
        return;
    }

    uint16_t addr = (uint16_t)atoi(tokens[1]);
    uint8_t aspect = (uint8_t)atoi(tokens[2]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    if (!DccApplicationCommandStationPacket_load_accessory_extended(&packet, addr, aspect)) {
        _respond("ERR: invalid parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_ACCESSORY,
                              DCC_PRIORITY_ACCESSORY, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACCE addr=%u aspect=%u",
             addr, aspect);
    _respond(_resp_buf);
}

    /**
     * @brief NOP <addr> [E]: accessory No-Operation packet (S-9.2.1 2.4.6).
     *
     * @details Lets a bi-directional accessory decoder raise an SRQ without changing any output.
     * A trailing E selects the extended-decoder form.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_nop(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: NOP <addr> [E]");
        return;
    }

    uint16_t addr = (uint16_t)atoi(tokens[1]);
    bool is_extended = (count >= 3 && tokens[2][0] == 'E');

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (!DccApplicationCommandStationPacket_load_accessory_nop(&packet, addr, is_extended)) {
        _respond("ERR: invalid NOP address");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_ACCESSORY,
                              DCC_PRIORITY_ACCESSORY, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: NOP addr=%u %s",
             addr, is_extended ? "extended" : "basic");
    _respond(_resp_buf);
}

    /**
     * @brief CV WRITE|VERIFY|BIT <addr> <cv> <value>: ops-mode (POM) CV packet for a mobile decoder.
     *
     * @details For BIT the value argument is the bit position and a sixth token gives the bit value.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_cv(char *tokens[], int count) {

    /* CV WRITE <addr> <cv> <value> */
    /* CV VERIFY <addr> <cv> <value> */
    /* CV BIT <addr> <cv> <bit> <0|1> */

    if (count < 5) {
        _respond("ERR: usage: CV WRITE|VERIFY|BIT <addr> <cv> <value>");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[2], &addr, &addr_type);
    uint16_t cv = (uint16_t)atoi(tokens[3]);
    uint8_t value = (uint8_t)atoi(tokens[4]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    bool ok = false;

    if (strcmp(tokens[1], "WRITE") == 0) {
        ok = DccApplicationCommandStationPacket_load_cv_write_pom(&packet, addr, addr_type, cv, value);
    } else if (strcmp(tokens[1], "VERIFY") == 0) {
        ok = DccApplicationCommandStationPacket_load_cv_verify_pom(&packet, addr, addr_type, cv, value);
    } else if (strcmp(tokens[1], "BIT") == 0 && count >= 6) {
        uint8_t bit_pos = value;
        bool bit_val = (atoi(tokens[5]) != 0);
        ok = DccApplicationCommandStationPacket_load_cv_bit_pom(&packet, addr, addr_type, cv,
                                          bit_pos, bit_val, true);
    } else {
        _respond("ERR: usage: CV WRITE|VERIFY|BIT <addr> <cv> <value>");
        return;
    }

    if (!ok) {
        _respond("ERR: invalid CV parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_CV,
                              DCC_PRIORITY_CV, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    _respond("OK: CV command scheduled");
}

    /**
     * @brief Completion callback shared by every service-mode task.
     *
     * @details The task call returns true as soon as it starts; the outcome arrives here once the
     * operation and its recovery time complete, and is printed as a SVC RESULT line.
     *
     * @param result Outcome of the task.
     * @param value  Byte read (for reads) or the value that was written.
     */
static void _svc_on_complete(dcc_service_mode_result_enum result, uint8_t value) {

    switch (result) {

        case DCC_SERVICE_MODE_SUCCESS:
            snprintf(_resp_buf, sizeof(_resp_buf),
                     "SVC RESULT: SUCCESS value=%u (0x%02X)", value, value);
            _respond(_resp_buf);
            break;

        case DCC_SERVICE_MODE_NO_ACK:
            _respond("SVC RESULT: NO ACK");
            break;

        case DCC_SERVICE_MODE_VERIFY_FAIL:
            _respond("SVC RESULT: VERIFY FAIL");
            break;

        case DCC_SERVICE_MODE_BUSY:
            _respond("SVC RESULT: BUSY");
            break;

        default:
            _respond("SVC RESULT: ERROR");
            break;

    }
}

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    /**
     * @brief Completion callback for SVC DETECT; prints the programming modes the decoder answered.
     *
     * @param result Outcome of the detection task.
     * @param modes  Bitmask of DCC_SERVICE_MODE_SUPPORTED_* flags.
     */
static void _svc_on_detect(dcc_service_mode_result_enum result, uint8_t modes) {

    if (result != DCC_SERVICE_MODE_SUCCESS || modes == 0) {
        _respond("SVC DETECT: none");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "SVC DETECT:%s%s%s%s",
             (modes & DCC_SERVICE_MODE_SUPPORTED_DIRECT)   ? " DIRECT"   : "",
             (modes & DCC_SERVICE_MODE_SUPPORTED_PAGED)    ? " PAGED"    : "",
             (modes & DCC_SERVICE_MODE_SUPPORTED_REGISTER) ? " REGISTER" : "",
             (modes & DCC_SERVICE_MODE_SUPPORTED_ADDRESS)  ? " ADDRESS"  : "");
    _respond(_resp_buf);
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    /**
     * @brief Maps an optional MOBILE|ACC|ACCESSORY token onto the register-mode decoder type.
     *
     * @param tok Token to inspect, or NULL when absent.
     *
     * @return DCC_DECODER_TYPE_ACCESSORY for ACC or ACCESSORY, otherwise DCC_DECODER_TYPE_MOBILE.
     */
static dcc_decoder_type_enum _parse_decoder_type(const char *tok) {

    if (tok && (strcmp(tok, "ACC") == 0 || strcmp(tok, "ACCESSORY") == 0)) {
        return DCC_DECODER_TYPE_ACCESSORY;
    }

    return DCC_DECODER_TYPE_MOBILE;
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

    /**
     * @brief Prints whether a service-mode task was accepted.
     *
     * @param started Return value of the task call.
     */
static void _svc_report_start(bool started) {

    _respond(started ? "OK: service mode operation started"
                     : "ERR: service mode operation failed to start");
}

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    /**
     * @brief SVC DIRECT WRITE|READ|BITW|BITR: starts a Direct-mode CV task on the service track.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_svc_direct(char *tokens[], int count) {

    /* SVC DIRECT WRITE <cv> <value>      */
    /* SVC DIRECT READ  <cv>              */
    /* SVC DIRECT BITW  <cv> <bit> <0|1>  */
    /* SVC DIRECT BITR  <cv> <bit>        */

    if (count < 4) {
        _respond("ERR: usage: SVC DIRECT WRITE|READ|BITW|BITR <cv> ...");
        return;
    }

    uint16_t cv = (uint16_t)atoi(tokens[3]);
    bool started = false;

    if (strcmp(tokens[2], "READ") == 0) {
        started = DccApplicationCommandStationServiceTrack_direct_read_cv(cv, _svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "WRITE") == 0 && count >= 5) {
        uint8_t value = (uint8_t)atoi(tokens[4]);
        started = DccApplicationCommandStationServiceTrack_direct_write_cv(cv, value, _svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "BITR") == 0 && count >= 5) {
        uint8_t bit = (uint8_t)atoi(tokens[4]);
        started = DccApplicationCommandStationServiceTrack_direct_read_bit(cv, bit, _svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "BITW") == 0 && count >= 6) {
        uint8_t bit = (uint8_t)atoi(tokens[4]);
        bool bit_val = (atoi(tokens[5]) != 0);
        started = DccApplicationCommandStationServiceTrack_direct_write_bit(cv, bit, bit_val, _svc_on_complete, NULL);
    } else {
        _respond("ERR: usage: SVC DIRECT WRITE|READ|BITW|BITR <cv> ...");
        return;
    }

    _svc_report_start(started);
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    /**
     * @brief SVC PAGED WRITE|READ: starts a Paged-mode CV task on the service track.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_svc_paged(char *tokens[], int count) {

    /* SVC PAGED WRITE <cv> <value> / SVC PAGED READ <cv> */

    if (count < 4) {
        _respond("ERR: usage: SVC PAGED WRITE|READ <cv> [value]");
        return;
    }

    uint16_t cv = (uint16_t)atoi(tokens[3]);
    bool started = false;

    if (strcmp(tokens[2], "READ") == 0) {
        started = DccApplicationCommandStationServiceTrack_paged_read_cv(cv, _svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "WRITE") == 0 && count >= 5) {
        uint8_t value = (uint8_t)atoi(tokens[4]);
        started = DccApplicationCommandStationServiceTrack_paged_write_cv(cv, value, _svc_on_complete, NULL);
    } else {
        _respond("ERR: usage: SVC PAGED WRITE|READ <cv> [value]");
        return;
    }

    _svc_report_start(started);
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    /**
     * @brief SVC REG WRITE|READ|RESET: starts a Register-mode task, optionally for an accessory decoder.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_svc_register(char *tokens[], int count) {

    /* SVC REG WRITE <cv> <value> [MOBILE|ACC] */
    /* SVC REG READ  <cv> [MOBILE|ACC]         */
    /* SVC REG RESET                           */

    if (count >= 2 && strcmp(tokens[2], "RESET") == 0) {
        _svc_report_start(DccApplicationCommandStationServiceTrack_register_factory_reset(_svc_on_complete));
        return;
    }

    if (count < 4) {
        _respond("ERR: usage: SVC REG WRITE|READ|RESET <cv> [value] [MOBILE|ACC]");
        return;
    }

    uint16_t cv = (uint16_t)atoi(tokens[3]);
    bool started = false;

    if (strcmp(tokens[2], "READ") == 0) {
        dcc_decoder_type_enum dt = _parse_decoder_type(count >= 5 ? tokens[4] : NULL);
        started = DccApplicationCommandStationServiceTrack_register_read_cv(cv, dt, _svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "WRITE") == 0 && count >= 5) {
        uint8_t value = (uint8_t)atoi(tokens[4]);
        dcc_decoder_type_enum dt = _parse_decoder_type(count >= 6 ? tokens[5] : NULL);
        started = DccApplicationCommandStationServiceTrack_register_write_cv(cv, value, dt, _svc_on_complete, NULL);
    } else {
        _respond("ERR: usage: SVC REG WRITE|READ|RESET <cv> [value] [MOBILE|ACC]");
        return;
    }

    _svc_report_start(started);
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    /**
     * @brief SVC ADDR WRITE <addr> | READ: starts an Address-mode task on the service track.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_svc_address(char *tokens[], int count) {

    /* SVC ADDR WRITE <addr> / SVC ADDR READ */

    if (count < 3) {
        _respond("ERR: usage: SVC ADDR WRITE <addr> | SVC ADDR READ");
        return;
    }

    bool started = false;

    if (strcmp(tokens[2], "READ") == 0) {
        started = DccApplicationCommandStationServiceTrack_address_read(_svc_on_complete, NULL);
    } else if (strcmp(tokens[2], "WRITE") == 0 && count >= 4) {
        uint8_t addr = (uint8_t)atoi(tokens[3]);
        started = DccApplicationCommandStationServiceTrack_address_write(addr, _svc_on_complete, NULL);
    } else {
        _respond("ERR: usage: SVC ADDR WRITE <addr> | SVC ADDR READ");
        return;
    }

    _svc_report_start(started);
}
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

    /**
     * @brief SVC dispatcher: ENTER and EXIT service mode, or hand off to the compiled-in task handlers.
     *
     * @details Each task family is only reachable when its DCC_COMPILE_SERVICE_MODE_TASK_* flag is set.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_svc(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: SVC ENTER|EXIT|DETECT|DIRECT|PAGED|REG|ADDR ...");
        return;
    }

    if (strcmp(tokens[1], "ENTER") == 0) {
        if (DccApplicationCommandStationServiceTrack_enter_service_mode())
            _respond("OK: service mode entered");
        else
            _respond("ERR: failed to enter service mode");
        return;
    }

    if (strcmp(tokens[1], "EXIT") == 0) {
        DccApplicationCommandStationServiceTrack_exit_service_mode();
        _respond("OK: service mode exited");
        return;
    }

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    if (strcmp(tokens[1], "DETECT") == 0) {
        _svc_report_start(DccApplicationCommandStationServiceTrack_detect_mode(_svc_on_detect));
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    if (strcmp(tokens[1], "DIRECT") == 0) {
        _cmd_svc_direct(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    if (strcmp(tokens[1], "PAGED") == 0) {
        _cmd_svc_paged(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    if (strcmp(tokens[1], "REG") == 0) {
        _cmd_svc_register(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    if (strcmp(tokens[1], "ADDR") == 0) {
        _cmd_svc_address(tokens, count);
        return;
    }
#endif

    _respond("ERR: unknown SVC subcommand");
}

    /**
     * @brief STATUS: prints the service-mode state and the loco table capacity.
     *
     * @details The active loco count is not computed yet and always prints 0.
     */
static void _cmd_status(void) {

    bool svc_active = DccApplicationCommandStationServiceTrack_is_service_mode_active();

    snprintf(_resp_buf, sizeof(_resp_buf),
             "STATUS: svc_mode=%s locos=%d/%d",
             svc_active ? "ACTIVE" : "IDLE",
             0, /* TODO: count active locos */
             USER_DEFINED_DCC_MAX_LOCOS);
    _respond(_resp_buf);
}

    /**
     * @brief CONSIST <addr> SET <consist_addr> [NORMAL|REVERSE] | CLEAR: ops-mode consist control.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_consist(char *tokens[], int count) {

    /* CONSIST <addr> SET <consist_addr> [NORMAL|REVERSE] */
    /* CONSIST <addr> CLEAR */

    if (count < 3) {
        _respond("ERR: usage: CONSIST <addr> SET <consist_addr> [NORMAL|REVERSE]");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    bool ok = false;

    if (strcmp(tokens[2], "SET") == 0 && count >= 4) {
        uint8_t consist_addr = (uint8_t)atoi(tokens[3]);
        bool direction_normal = true;
        if (count >= 5 && strcmp(tokens[4], "REVERSE") == 0)
            direction_normal = false;
        ok = DccApplicationCommandStationPacket_load_consist_set(&packet, addr, addr_type,
                                           consist_addr, direction_normal);
    } else if (strcmp(tokens[2], "CLEAR") == 0) {
        ok = DccApplicationCommandStationPacket_load_consist_clear(&packet, addr, addr_type);
    } else {
        _respond("ERR: usage: CONSIST <addr> SET|CLEAR ...");
        return;
    }

    if (!ok) {
        _respond("ERR: invalid consist parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_CONSIST,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: CONSIST addr=%u %s", addr, tokens[2]);
    _respond(_resp_buf);
}

    /**
     * @brief BSS <addr> <1-127> <ON|OFF>: short-form binary state control packet.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_bss(char *tokens[], int count) {

    /* BSS <addr> <1-127> <ON|OFF> */

    if (count < 4) {
        _respond("ERR: usage: BSS <addr> <1-127> <ON|OFF>");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);
    uint8_t state_num = (uint8_t)atoi(tokens[2]);
    bool active = (strcmp(tokens[3], "ON") == 0);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    if (!DccApplicationCommandStationPacket_load_binary_state_short(&packet, addr, addr_type,
                                              state_num, active)) {
        _respond("ERR: invalid binary state parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_BINARY_STATE,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: BSS addr=%u state=%u %s",
             addr, state_num, active ? "ON" : "OFF");
    _respond(_resp_buf);
}

    /**
     * @brief BSL <addr> <1-32767> <ON|OFF>: long-form binary state control packet.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_bsl(char *tokens[], int count) {

    /* BSL <addr> <1-32767> <ON|OFF> */

    if (count < 4) {
        _respond("ERR: usage: BSL <addr> <1-32767> <ON|OFF>");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);
    uint16_t state_num = (uint16_t)atoi(tokens[2]);
    bool active = (strcmp(tokens[3], "ON") == 0);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    if (!DccApplicationCommandStationPacket_load_binary_state_long(&packet, addr, addr_type,
                                             state_num, active)) {
        _respond("ERR: invalid binary state parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_BINARY_STATE,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: BSL addr=%u state=%u %s",
             addr, state_num, active ? "ON" : "OFF");
    _respond(_resp_buf);
}

    /**
     * @brief ANALOG <addr> <output> <value>: analog function output packet.
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_analog(char *tokens[], int count) {

    /* ANALOG <addr> <output> <value> */

    if (count < 4) {
        _respond("ERR: usage: ANALOG <addr> <output_0-255> <value_0-255>");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);
    uint8_t output = (uint8_t)atoi(tokens[2]);
    uint8_t value = (uint8_t)atoi(tokens[3]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    if (!DccApplicationCommandStationPacket_load_analog_function(&packet, addr, addr_type, output, value)) {
        _respond("ERR: invalid analog parameters");
        return;
    }

    if (!_schedule_main_track(&packet, addr, DCC_TAG_ANALOG_FUNC,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ANALOG addr=%u output=%u value=%u",
             addr, output, value);
    _respond(_resp_buf);
}

    /**
     * @brief CLEAR: removes every auto-refresh entry and forgets all loco state, leaving an idle-only stream.
     */
static void _cmd_clear(void) {

    DccApplicationCommandStationMainTrack_remove_all_auto_refresh();
    memset(_loco_table, 0, sizeof(_loco_table));
    _respond("OK: cleared (auto-refresh + loco state, idle-only)");
}

    /**
     * @brief RESET: sends one broadcast decoder reset packet (00 00 00).
     */
static void _cmd_reset(void) {

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    DccApplicationCommandStationPacket_load_reset(&packet);

    if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                              DCC_PRIORITY_ESTOP, false)) {
        _respond("ERR: scheduler full");
        return;
    }
    _respond("OK: RESET packet scheduled (00 00 00)");
}

    /**
     * @brief SYSTIME <ms>: broadcast system time packet (S-9.2.1 2.3.6.3).
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_systime(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: SYSTIME <milliseconds>");
        return;
    }

    uint16_t milliseconds = (uint16_t)atoi(tokens[1]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    DccApplicationCommandStationPacket_load_system_time(&packet, milliseconds);

    if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                              DCC_PRIORITY_ESTOP, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: SYSTIME ms=%u", milliseconds);
    _respond(_resp_buf);
}

    /**
     * @brief MTIME <min> <dow> <hours> <update> <accel>: broadcast model time packet (S-9.2.1 2.3.6.2).
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_mtime(char *tokens[], int count) {

    if (count < 6) {
        _respond("ERR: usage: MTIME <minutes> <dow> <hours> <update> <accel>");
        return;
    }

    uint8_t minutes = (uint8_t)atoi(tokens[1]);
    dcc_day_of_week_enum dow = (dcc_day_of_week_enum)atoi(tokens[2]);
    uint8_t hours = (uint8_t)atoi(tokens[3]);
    bool update = (atoi(tokens[4]) != 0);
    uint8_t accel = (uint8_t)atoi(tokens[5]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (!DccApplicationCommandStationPacket_load_model_time(&packet, minutes, dow, hours, update, accel)) {
        _respond("ERR: invalid MTIME parameters");
        return;
    }

    if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                              DCC_PRIORITY_ESTOP, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: MTIME %02u:%02u dow=%u", hours, minutes, (unsigned)dow);
    _respond(_resp_buf);
}

    /**
     * @brief MDATE <day> <month> <year>: broadcast model date packet (S-9.2.1 2.3.6.2).
     *
     * @param tokens Uppercased token array; tokens[0] is the command word.
     * @param count  Number of valid entries in tokens.
     */
static void _cmd_mdate(char *tokens[], int count) {

    if (count < 4) {
        _respond("ERR: usage: MDATE <day> <month> <year>");
        return;
    }

    uint8_t day = (uint8_t)atoi(tokens[1]);
    uint8_t month = (uint8_t)atoi(tokens[2]);
    uint16_t year = (uint16_t)atoi(tokens[3]);

    dcc_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    if (!DccApplicationCommandStationPacket_load_model_date(&packet, day, month, year)) {
        _respond("ERR: invalid MDATE parameters");
        return;
    }

    if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                              DCC_PRIORITY_ESTOP, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: MDATE %u-%02u-%02u", year, month, day);
    _respond(_resp_buf);
}

    /**
     * @brief HELP: prints the command list.
     */
static void _cmd_help(void) {

    _respond("DCC Command Station Commands:");
    _respond("  POWER ON|OFF");
    _respond("  SPEED <addr> <speed> <FWD|REV> [14|28|128]");
    _respond("  ESTOP [addr]  (emergency: stop delivering energy)");
    _respond("  STOP  (broadcast controlled stop, baseline S=0)");
    _respond("  FUNC <addr> <0-68> <ON|OFF>");
    _respond("  ACC <board> <pair> <ON|OFF>");
    _respond("  ACC CV WRITE|VERIFY <board> <pair> <cv> <value>");
    _respond("  ACC CV BIT <board> <pair> <cv> <bit_pos> <0|1>");
    _respond("  ACCE <addr> <aspect>");
    _respond("  NOP <addr> [E]  (accessory NOP; E=extended)");
    _respond("  ACCE CV WRITE|VERIFY <addr> <cv> <value>");
    _respond("  ACCE CV BIT <addr> <cv> <bit_pos> <0|1>");
    _respond("  CV WRITE|VERIFY <addr> <cv> <value>");
    _respond("  CV BIT <addr> <cv> <bit_pos> <0|1>");
    _respond("  SVC ENTER");
    _respond("  SVC EXIT");
    _respond("  SVC DETECT");
    _respond("  SVC DIRECT WRITE <cv> <value> | READ <cv>");
    _respond("  SVC DIRECT BITW <cv> <bit> <0|1> | BITR <cv> <bit>");
    _respond("  SVC PAGED WRITE <cv> <value> | READ <cv>");
    _respond("  SVC REG WRITE <cv> <value> [MOBILE|ACC] | READ <cv> [MOBILE|ACC] | RESET");
    _respond("  SVC ADDR WRITE <addr> | READ");
    _respond("  CONSIST <addr> SET <ca> [NORMAL|REVERSE]");
    _respond("  CONSIST <addr> CLEAR");
    _respond("  BSS <addr> <1-127> <ON|OFF>");
    _respond("  BSL <addr> <1-32767> <ON|OFF>");
    _respond("  ANALOG <addr> <output> <value>");
    _respond("  CLEAR (remove all auto-refresh; idle-only stream)");
    _respond("  RESET (send one broadcast reset packet 00 00 00)");
    _respond("  SYSTIME <ms>  (broadcast system time, S-9.2.1 2.3.6.3)");
    _respond("  MTIME <min> <dow> <hours> <update> <accel>  (model time, 2.3.6.2)");
    _respond("  MDATE <day> <month> <year>  (model date, S-9.2.1 2.3.6.2)");
    _respond("  REFRESH ON|OFF  (auto-refresh speed/func)");
    _respond("  STATUS");
    _respond("  HELP");
}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

    /**
     * @brief Clears the loco state table.
     */
void UartCommandParser_initialize(void) {

    memset(_loco_table, 0, sizeof(_loco_table));
}

    /**
     * @brief Reads one complete line, uppercases and tokenizes it, and dispatches on the first token.
     *
     * @details Returns immediately when no complete line is waiting or the line is empty.
     * Unknown commands produce an ERR line.
     */
void UartCommandParser_process(void) {

    if (!TI_UartDriver_read_line(_line_buf, CMD_LINE_MAX))
        return;

    /* Convert to uppercase for case-insensitive matching */
    _strupper(_line_buf);

    char *tokens[CMD_MAX_TOKENS];
    int count = _tokenize(_line_buf, tokens, CMD_MAX_TOKENS);

    if (count == 0)
        return;

    if (strcmp(tokens[0], "POWER") == 0)
        _cmd_power(tokens, count);
    else if (strcmp(tokens[0], "REFRESH") == 0)
        _cmd_refresh(tokens, count);
    else if (strcmp(tokens[0], "SPEED") == 0)
        _cmd_speed(tokens, count);
    else if (strcmp(tokens[0], "ESTOP") == 0)
        _cmd_estop(tokens, count);
    else if (strcmp(tokens[0], "STOP") == 0)
        _cmd_stop();
    else if (strcmp(tokens[0], "FUNC") == 0)
        _cmd_func(tokens, count);
    else if (strcmp(tokens[0], "ACC") == 0)
        _cmd_acc(tokens, count);
    else if (strcmp(tokens[0], "ACCE") == 0)
        _cmd_acce(tokens, count);
    else if (strcmp(tokens[0], "NOP") == 0)
        _cmd_nop(tokens, count);
    else if (strcmp(tokens[0], "CV") == 0)
        _cmd_cv(tokens, count);
    else if (strcmp(tokens[0], "SVC") == 0)
        _cmd_svc(tokens, count);
    else if (strcmp(tokens[0], "CONSIST") == 0)
        _cmd_consist(tokens, count);
    else if (strcmp(tokens[0], "BSS") == 0)
        _cmd_bss(tokens, count);
    else if (strcmp(tokens[0], "BSL") == 0)
        _cmd_bsl(tokens, count);
    else if (strcmp(tokens[0], "ANALOG") == 0)
        _cmd_analog(tokens, count);
    else if (strcmp(tokens[0], "CLEAR") == 0)
        _cmd_clear();
    else if (strcmp(tokens[0], "RESET") == 0)
        _cmd_reset();
    else if (strcmp(tokens[0], "SYSTIME") == 0)
        _cmd_systime(tokens, count);
    else if (strcmp(tokens[0], "MTIME") == 0)
        _cmd_mtime(tokens, count);
    else if (strcmp(tokens[0], "MDATE") == 0)
        _cmd_mdate(tokens, count);
    else if (strcmp(tokens[0], "STATUS") == 0)
        _cmd_status();
    else if (strcmp(tokens[0], "HELP") == 0)
        _cmd_help();
    else {
        snprintf(_resp_buf, sizeof(_resp_buf), "ERR: unknown command '%s'",
                 tokens[0]);
        _respond(_resp_buf);
    }
}
