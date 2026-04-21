// uart_command_parser.c
//
// Text command parser for the DCC command station UART interface.
//
// Reads lines from TI_UartDriver_read_line(), splits into tokens, and
// dispatches to DCC library API calls (speed, function, accessory, service
// mode, track power, etc.).
//
// STRUCTURE:
//   - _loco_table[] tracks per-locomotive state (speed, direction, functions)
//     so that function group commands can be sent with the correct bitmask.
//   - UartCommandParser_process() is the main dispatch -- it matches the first
//     token against known commands and calls the appropriate handler.
//   - Each _cmd_*() function parses its arguments, validates them, and
//     calls the corresponding DCC library API.
//
// Type "HELP" at the UART terminal to see all available commands.

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

/* Maximum command line length */
#define CMD_LINE_MAX 128

/* Maximum tokens per command */
#define CMD_MAX_TOKENS 8

/* Per-locomotive state for function bitmask tracking */
typedef struct {

    dcc_address_t address;
    dcc_address_type_enum address_type;
    uint8_t speed;
    bool direction;
    uint8_t func_fl_f4;
    uint8_t func_f5_f8;
    uint8_t func_f9_f12;
    uint8_t func_f13_f20;
    uint8_t func_f21_f28;
    uint8_t func_f29_f36;
    uint8_t func_f37_f44;
    uint8_t func_f45_f52;
    uint8_t func_f53_f60;
    uint8_t func_f61_f68;
    bool active;

} loco_state_t;

static loco_state_t _loco_table[USER_DEFINED_DCC_MAX_LOCOS];

static char _line_buf[CMD_LINE_MAX];
static char _resp_buf[128];
static bool _auto_refresh = true;

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

static void _respond(const char *msg) {

    TI_UartDriver_write_string(msg);
    TI_UartDriver_write_string("\r\n");
}

// Parse an address token like "40", "40S", "40L".
// token: the token string (already uppercased).
// addr_out: receives the numeric address.
// type_out: receives the address type.
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

// Schedule a packet on the main track. When auto_refresh is true the packet
// is added to the scheduler's auto-refresh list; otherwise it is sent once.
static bool _schedule_main_track(const dcc_packet_t *packet, dcc_address_t address,
                                 dcc_tag_enum tag, dcc_priority_enum priority,
                                 bool auto_refresh) {

    if (auto_refresh) {
        return DccApplicationCommandStationMainTrack_add_to_auto_refresh(packet, address, tag, priority);
    }
    return DccApplicationCommandStationMainTrack_send_packet(packet, address, tag, priority);
}

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
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, loco->address, DCC_TAG_SPEED,
                              DCC_PRIORITY_SPEED, _auto_refresh)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: SPEED addr=%u speed=%u dir=%s mode=%u",
             addr, speed, direction ? "FWD" : "REV", mode);
    _respond(_resp_buf);
}

static void _cmd_estop(char *tokens[], int count) {

    dcc_packet_t packet;

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
        packet.repeat_count = 3;

        if (!_schedule_main_track(&packet, addr, DCC_TAG_SPEED,
                                  DCC_PRIORITY_ESTOP, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        snprintf(_resp_buf, sizeof(_resp_buf), "OK: ESTOP addr=%u", addr);
        _respond(_resp_buf);
    } else {
        /* Broadcast emergency stop */
        DccApplicationCommandStationPacket_load_estop_all(&packet);
        packet.repeat_count = 3;

        if (!_schedule_main_track(&packet, 0, DCC_TAG_SPEED,
                                  DCC_PRIORITY_ESTOP, false)) {
            _respond("ERR: scheduler full");
            return;
        }

        _respond("OK: ESTOP broadcast");
    }
}

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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;
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
        packet.repeat_count = 3;

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
    if (!DccApplicationCommandStationPacket_load_accessory_basic(&packet, board, pair, activate)) {
        _respond("ERR: invalid accessory parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, board, DCC_TAG_ACCESSORY,
                              DCC_PRIORITY_ACCESSORY, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACC board=%u pair=%u %s",
             board, pair, activate ? "ON" : "OFF");
    _respond(_resp_buf);
}

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
        packet.repeat_count = 3;

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
    if (!DccApplicationCommandStationPacket_load_accessory_extended(&packet, addr, aspect)) {
        _respond("ERR: invalid parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_ACCESSORY,
                              DCC_PRIORITY_ACCESSORY, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACCE addr=%u aspect=%u",
             addr, aspect);
    _respond(_resp_buf);
}

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
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_CV,
                              DCC_PRIORITY_CV, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    _respond("OK: CV command scheduled");
}

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
static void _cmd_svc_direct(char *tokens[], int count) {

    /* SVC DIRECT WRITE <cv> <value> */
    /* SVC DIRECT VERIFY <cv> <value> */
    /* SVC DIRECT BITW <cv> <bit> <0|1> */
    /* SVC DIRECT BITV <cv> <bit> <0|1> */

    if (count < 5) {
        _respond("ERR: usage: SVC DIRECT WRITE|VERIFY|BITW|BITV <cv> <value>");
        return;
    }

    uint16_t cv = (uint16_t)atoi(tokens[3]);
    uint8_t value = (uint8_t)atoi(tokens[4]);
    bool ok = false;

    if (strcmp(tokens[2], "WRITE") == 0) {
        ok = DccApplicationCommandStationServiceTrack_direct_write_byte(cv, value);
    } else if (strcmp(tokens[2], "VERIFY") == 0) {
        ok = DccApplicationCommandStationServiceTrack_direct_verify_byte(cv, value);
    } else if (strcmp(tokens[2], "BITW") == 0 && count >= 6) {
        bool bit_val = (atoi(tokens[5]) != 0);
        ok = DccApplicationCommandStationServiceTrack_direct_write_bit(cv, value, bit_val);
    } else if (strcmp(tokens[2], "BITV") == 0 && count >= 6) {
        bool bit_val = (atoi(tokens[5]) != 0);
        ok = DccApplicationCommandStationServiceTrack_direct_verify_bit(cv, value, bit_val);
    } else {
        _respond("ERR: unknown SVC DIRECT subcommand");
        return;
    }

    if (!ok) {
        _respond("ERR: service mode operation failed to start");
        return;
    }

    _respond("OK: service mode operation started");
}
#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
static void _cmd_svc_paged(char *tokens[], int count) {

    if (count < 5) {
        _respond("ERR: usage: SVC PAGED WRITE|VERIFY <cv> <value>");
        return;
    }

    uint16_t cv = (uint16_t)atoi(tokens[3]);
    uint8_t value = (uint8_t)atoi(tokens[4]);
    bool ok = false;

    if (strcmp(tokens[2], "WRITE") == 0) {
        ok = DccApplicationCommandStationServiceTrack_paged_write(cv, value);
    } else if (strcmp(tokens[2], "VERIFY") == 0) {
        ok = DccApplicationCommandStationServiceTrack_paged_verify(cv, value);
    } else {
        _respond("ERR: usage: SVC PAGED WRITE|VERIFY <cv> <value>");
        return;
    }

    if (!ok) {
        _respond("ERR: service mode operation failed to start");
        return;
    }

    _respond("OK: service mode operation started");
}
#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
static void _cmd_svc_register(char *tokens[], int count) {

    if (count < 5) {
        _respond("ERR: usage: SVC REG WRITE|VERIFY <reg> <value>");
        return;
    }

    uint8_t reg = (uint8_t)atoi(tokens[3]);
    uint8_t value = (uint8_t)atoi(tokens[4]);
    bool ok = false;

    if (strcmp(tokens[2], "WRITE") == 0) {
        ok = DccApplicationCommandStationServiceTrack_register_write(reg, value);
    } else if (strcmp(tokens[2], "VERIFY") == 0) {
        ok = DccApplicationCommandStationServiceTrack_register_verify(reg, value);
    } else {
        _respond("ERR: usage: SVC REG WRITE|VERIFY <reg> <value>");
        return;
    }

    if (!ok) {
        _respond("ERR: service mode operation failed to start");
        return;
    }

    _respond("OK: service mode operation started");
}
#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
static void _cmd_svc_address(char *tokens[], int count) {

    if (count < 4) {
        _respond("ERR: usage: SVC ADDR WRITE|VERIFY <addr>");
        return;
    }

    uint8_t addr = (uint8_t)atoi(tokens[3]);
    bool ok = false;

    if (strcmp(tokens[2], "WRITE") == 0) {
        ok = DccApplicationCommandStationServiceTrack_address_write(addr);
    } else if (strcmp(tokens[2], "VERIFY") == 0) {
        ok = DccApplicationCommandStationServiceTrack_address_verify(addr);
    } else {
        _respond("ERR: usage: SVC ADDR WRITE|VERIFY <addr>");
        return;
    }

    if (!ok) {
        _respond("ERR: service mode operation failed to start");
        return;
    }

    _respond("OK: service mode operation started");
}
#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

static void _cmd_svc(char *tokens[], int count) {

    if (count < 2) {
        _respond("ERR: usage: SVC ENTER|EXIT|DIRECT|PAGED|REG|ADDR ...");
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

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
    if (strcmp(tokens[1], "DIRECT") == 0) {
        _cmd_svc_direct(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
    if (strcmp(tokens[1], "PAGED") == 0) {
        _cmd_svc_paged(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
    if (strcmp(tokens[1], "REG") == 0) {
        _cmd_svc_register(tokens, count);
        return;
    }
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
    if (strcmp(tokens[1], "ADDR") == 0) {
        _cmd_svc_address(tokens, count);
        return;
    }
#endif

    _respond("ERR: unknown SVC subcommand");
}

static void _cmd_status(void) {

    bool svc_active = DccApplicationCommandStationServiceTrack_is_service_mode_active();

    snprintf(_resp_buf, sizeof(_resp_buf),
             "STATUS: svc_mode=%s locos=%d/%d",
             svc_active ? "ACTIVE" : "IDLE",
             0, /* TODO: count active locos */
             USER_DEFINED_DCC_MAX_LOCOS);
    _respond(_resp_buf);
}

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
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_CONSIST,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: CONSIST addr=%u %s", addr, tokens[2]);
    _respond(_resp_buf);
}

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
    if (!DccApplicationCommandStationPacket_load_binary_state_short(&packet, addr, addr_type,
                                              state_num, active)) {
        _respond("ERR: invalid binary state parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_BINARY_STATE,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: BSS addr=%u state=%u %s",
             addr, state_num, active ? "ON" : "OFF");
    _respond(_resp_buf);
}

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
    if (!DccApplicationCommandStationPacket_load_binary_state_long(&packet, addr, addr_type,
                                             state_num, active)) {
        _respond("ERR: invalid binary state parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_BINARY_STATE,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: BSL addr=%u state=%u %s",
             addr, state_num, active ? "ON" : "OFF");
    _respond(_resp_buf);
}

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
    if (!DccApplicationCommandStationPacket_load_analog_function(&packet, addr, addr_type, output, value)) {
        _respond("ERR: invalid analog parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_ANALOG_FUNC,
                              DCC_PRIORITY_FUNCTION, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ANALOG addr=%u output=%u value=%u",
             addr, output, value);
    _respond(_resp_buf);
}

static void _cmd_restrict(char *tokens[], int count) {

    /* RESTRICT <addr> ON <speed_limit> */
    /* RESTRICT <addr> OFF */

    if (count < 3) {
        _respond("ERR: usage: RESTRICT <addr> ON <speed_limit>|OFF");
        return;
    }

    uint16_t addr;
    dcc_address_type_enum addr_type;
    _parse_address(tokens[1], &addr, &addr_type);

    bool enabled;
    uint8_t speed_limit = 0;

    if (strcmp(tokens[2], "ON") == 0) {
        enabled = true;
        if (count >= 4)
            speed_limit = (uint8_t)atoi(tokens[3]);
    } else if (strcmp(tokens[2], "OFF") == 0) {
        enabled = false;
    } else {
        _respond("ERR: usage: RESTRICT <addr> ON <speed_limit>|OFF");
        return;
    }

    dcc_packet_t packet;
    if (!DccApplicationCommandStationPacket_load_speed_restriction(&packet, addr, addr_type,
                                             enabled, speed_limit)) {
        _respond("ERR: invalid restrict parameters");
        return;
    }
    packet.repeat_count = 3;

    if (!_schedule_main_track(&packet, addr, DCC_TAG_SPEED,
                              DCC_PRIORITY_SPEED, false)) {
        _respond("ERR: scheduler full");
        return;
    }

    if (enabled)
        snprintf(_resp_buf, sizeof(_resp_buf),
                 "OK: RESTRICT addr=%u ON limit=%u", addr, speed_limit);
    else
        snprintf(_resp_buf, sizeof(_resp_buf),
                 "OK: RESTRICT addr=%u OFF", addr);
    _respond(_resp_buf);
}

static void _cmd_help(void) {

    _respond("DCC Command Station Commands:");
    _respond("  POWER ON|OFF");
    _respond("  SPEED <addr> <speed> <FWD|REV> [14|28|128]");
    _respond("  ESTOP [addr]");
    _respond("  FUNC <addr> <0-68> <ON|OFF>");
    _respond("  ACC <board> <pair> <ON|OFF>");
    _respond("  ACC CV WRITE|VERIFY <board> <pair> <cv> <value>");
    _respond("  ACC CV BIT <board> <pair> <cv> <bit_pos> <0|1>");
    _respond("  ACCE <addr> <aspect>");
    _respond("  ACCE CV WRITE|VERIFY <addr> <cv> <value>");
    _respond("  ACCE CV BIT <addr> <cv> <bit_pos> <0|1>");
    _respond("  CV WRITE|VERIFY <addr> <cv> <value>");
    _respond("  CV BIT <addr> <cv> <bit_pos> <0|1>");
    _respond("  SVC ENTER");
    _respond("  SVC EXIT");
    _respond("  SVC DIRECT WRITE|VERIFY <cv> <value>");
    _respond("  SVC DIRECT BITW|BITV <cv> <bit> <0|1>");
    _respond("  SVC PAGED WRITE|VERIFY <cv> <value>");
    _respond("  SVC REG WRITE|VERIFY <reg> <value>");
    _respond("  SVC ADDR WRITE|VERIFY <addr>");
    _respond("  CONSIST <addr> SET <ca> [NORMAL|REVERSE]");
    _respond("  CONSIST <addr> CLEAR");
    _respond("  BSS <addr> <1-127> <ON|OFF>");
    _respond("  BSL <addr> <1-32767> <ON|OFF>");
    _respond("  ANALOG <addr> <output> <value>");
    _respond("  RESTRICT <addr> ON <limit>|OFF");
    _respond("  REFRESH ON|OFF  (auto-refresh speed/func)");
    _respond("  STATUS");
    _respond("  HELP");
}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

void UartCommandParser_initialize(void) {

    memset(_loco_table, 0, sizeof(_loco_table));
}

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
    else if (strcmp(tokens[0], "FUNC") == 0)
        _cmd_func(tokens, count);
    else if (strcmp(tokens[0], "ACC") == 0)
        _cmd_acc(tokens, count);
    else if (strcmp(tokens[0], "ACCE") == 0)
        _cmd_acce(tokens, count);
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
    else if (strcmp(tokens[0], "RESTRICT") == 0)
        _cmd_restrict(tokens, count);
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
