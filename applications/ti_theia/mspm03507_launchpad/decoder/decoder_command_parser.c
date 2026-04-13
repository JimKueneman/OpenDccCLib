/*
 * decoder_command_parser.c -- UART command parser implementation.
 *
 * This file is demo infrastructure.  You probably will not need to modify
 * it for a real decoder.  It reads lines from the UART driver, tokenizes
 * them, and dispatches to simple command handlers.
 *
 * Commands:
 *   ADDR <n> <SHORT|LONG|ACC|ACCE>  -- Set decoder address
 *   CLEAR                           -- Flush RECV buffer
 *   ACK [ON|OFF|<width_us>]         -- ACK pulse control
 *   STATUS                          -- Show current address
 *   HELP                            -- Show command list
 */

#include "decoder_command_parser.h"
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "application_drivers/ack_pulse_driver.h"
#include "dcc_lib/dcc_config.h"
#include "dcc_lib/dcc_types.h"
#include "dcc_lib/dcc_defines.h"
#include "application_callbacks/callbacks_dcc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* dcc_config is defined in decoder.c — needed for re-init after CV changes. */
extern const dcc_config_t dcc_config;


/* Maximum command line length */
#define CMD_LINE_MAX   128

/* Maximum tokens per command */
#define CMD_MAX_TOKENS 4

static char _line_buf[CMD_LINE_MAX];
static char _resp_buf[128];

/* Current decoder address state (for STATUS reporting) */
static dcc_address_t _current_addr = 3;
static dcc_address_type_enum _current_type = DCC_ADDRESS_SHORT;

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

static void _respond(const char *msg) {

    TI_UartDriver_write_string(msg);
    TI_UartDriver_write_string("\r\n");

}

static void _prompt(void) {

    TI_UartDriver_write_string("> ");

}

static void _strupper(char *s) {

    while (*s) {

        if (*s >= 'a' && *s <= 'z') {

            *s -= 32;

        }
        s++;

    }

}

static int _tokenize(char *line, char *tokens[], int max_tokens) {

    int count = 0;
    char *p = line;

    while (*p && count < max_tokens) {

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') {

            p++;

        }

        if (*p == '\0') {

            break;

        }

        tokens[count++] = p;

        /* Advance to next whitespace */
        while (*p && *p != ' ' && *p != '\t') {

            p++;

        }

        if (*p) {

            *p++ = '\0';

        }

    }

    return count;

}

static const char *_type_to_string(dcc_address_type_enum type) {

    switch (type) {

        case DCC_ADDRESS_SHORT:              return "SHORT";
        case DCC_ADDRESS_LONG:               return "LONG";
        case DCC_ADDRESS_ACCESSORY:          return "ACC";
        case DCC_ADDRESS_ACCESSORY_EXTENDED: return "ACCE";
        default:                             return "UNKNOWN";

    }

}

/* ========================================================================== */
/* Command handlers                                                           */
/* ========================================================================== */

static void _cmd_addr(char *tokens[], int count) {

    if (count < 3) {

        _respond("ERR: ADDR <number> <SHORT|LONG|ACC|ACCE>");
        return;

    }

    uint16_t addr = (uint16_t)atoi(tokens[1]);
    dcc_address_type_enum type;
    uint8_t cv29_val;
    uint8_t cv541_val;

    CallbacksDcc_cv_read(DCC_CV_CONFIG, &cv29_val);
    CallbacksDcc_cv_read(DCC_CV_ACC_CONFIG, &cv541_val);

    if (strcmp(tokens[2], "SHORT") == 0) {

        type = DCC_ADDRESS_SHORT;
        CallbacksDcc_cv_write(DCC_CV_PRIMARY_ADDRESS, (uint8_t)addr);
        cv29_val &= ~DCC_CV29_EXTENDED_ADDRESS_BIT;
        CallbacksDcc_cv_write(DCC_CV_CONFIG, cv29_val);
        cv541_val &= ~DCC_CV541_ACCESSORY_DECODER_BIT;
        CallbacksDcc_cv_write(DCC_CV_ACC_CONFIG, cv541_val);

    } else if (strcmp(tokens[2], "LONG") == 0) {

        type = DCC_ADDRESS_LONG;
        CallbacksDcc_cv_write(DCC_CV_EXTENDED_ADDRESS_HIGH,
                              (uint8_t)(0xC0 | ((addr >> 8) & 0x3F)));
        CallbacksDcc_cv_write(DCC_CV_EXTENDED_ADDRESS_LOW,
                              (uint8_t)(addr & 0xFF));
        cv29_val |= DCC_CV29_EXTENDED_ADDRESS_BIT;
        CallbacksDcc_cv_write(DCC_CV_CONFIG, cv29_val);
        cv541_val &= ~DCC_CV541_ACCESSORY_DECODER_BIT;
        CallbacksDcc_cv_write(DCC_CV_ACC_CONFIG, cv541_val);

    } else if (strcmp(tokens[2], "ACC") == 0) {

        type = DCC_ADDRESS_ACCESSORY;
        CallbacksDcc_cv_write(DCC_CV_ACC_ADDRESS_LSB,
                              (uint8_t)(addr & 0x3F));
        CallbacksDcc_cv_write(DCC_CV_ACC_ADDRESS_MSB,
                              (uint8_t)((addr >> 6) & 0x07));
        cv541_val = DCC_CV541_ACCESSORY_DECODER_BIT;
        CallbacksDcc_cv_write(DCC_CV_ACC_CONFIG, cv541_val);

    } else if (strcmp(tokens[2], "ACCE") == 0) {

        type = DCC_ADDRESS_ACCESSORY_EXTENDED;
        CallbacksDcc_cv_write(DCC_CV_ACC_ADDRESS_LSB,
                              (uint8_t)(addr & 0x3F));
        CallbacksDcc_cv_write(DCC_CV_ACC_ADDRESS_MSB,
                              (uint8_t)((addr >> 6) & 0x07));
        cv541_val = DCC_CV541_ACCESSORY_DECODER_BIT
                  | DCC_CV541_BASIC_EXTENDED_BIT;
        CallbacksDcc_cv_write(DCC_CV_ACC_CONFIG, cv541_val);

    } else {

        _respond("ERR: type must be SHORT, LONG, ACC, or ACCE");
        return;

    }

    /* Re-initialize so the library re-reads the CV address cache. */
    DccConfig_initialize(&dcc_config);

    _current_addr = addr;
    _current_type = type;

    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ADDR=%u TYPE=%s",
             addr, _type_to_string(type));
    _respond(_resp_buf);

}

static void _cmd_clear(void) {

    CallbacksDcc_clear();
    _respond("OK: CLEAR");

}

static void _cmd_status(void) {

    snprintf(_resp_buf, sizeof(_resp_buf), "STATUS: addr=%u type=%s",
             _current_addr, _type_to_string(_current_type));
    _respond(_resp_buf);

}

static void _cmd_ack(char *tokens[], int count) {

    if (count < 2) {

        /* No argument — report current state */
        snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACK=%s WIDTH=%lu",
                 AckPulseDriver_is_enabled() ? "ON" : "OFF",
                 (unsigned long)AckPulseDriver_get_width_us());
        _respond(_resp_buf);
        return;

    }

    if (strcmp(tokens[1], "ON") == 0) {

        AckPulseDriver_set_enabled(true);
        _respond("OK: ACK=ON");
        return;

    }

    if (strcmp(tokens[1], "OFF") == 0) {

        AckPulseDriver_set_enabled(false);
        _respond("OK: ACK=OFF");
        return;

    }

    if (strcmp(tokens[1], "TEST") == 0) {

        /* ACK TEST [count] — fire one or more ACK pulses for logic analyzer.
         * Temporarily forces enabled so pulse fires regardless of ACK state. */
        uint32_t pulse_count = 1;
        bool was_enabled = AckPulseDriver_is_enabled();

        if (count >= 3) {

            pulse_count = (uint32_t)atoi(tokens[2]);
            if (pulse_count == 0 || pulse_count > 100) {

                _respond("ERR: ACK TEST count must be 1-100");
                return;

            }

        }

        AckPulseDriver_set_enabled(true);

        uint32_t i;
        for (i = 0; i < pulse_count; i++) {

            AckPulseDriver_fire();

            /* Wait for pulse to complete + gap.  Pulse width is at most
             * 20 ms; a simple busy-wait with a generous timeout keeps
             * this command self-contained. */
            volatile uint32_t delay;
            for (delay = 0; delay < 400000; delay++) { }

        }

        AckPulseDriver_set_enabled(was_enabled);
        snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACK_TEST pulses=%lu",
                 (unsigned long)pulse_count);
        _respond(_resp_buf);
        return;

    }

    /* Numeric argument — set pulse width */
    uint32_t width = (uint32_t)atoi(tokens[1]);

    if (width < 1000 || width > 20000) {

        _respond("ERR: ACK width must be 1000-20000 us");
        return;

    }

    AckPulseDriver_set_width_us(width);
    snprintf(_resp_buf, sizeof(_resp_buf), "OK: ACK_WIDTH=%lu",
             (unsigned long)width);
    _respond(_resp_buf);

}

static void _cmd_help(void) {

    _respond("Commands:");
    _respond("  ADDR <n> <SHORT|LONG|ACC|ACCE>  Set decoder address");
    _respond("  CLEAR                           Flush RECV buffer");
    _respond("  ACK                             Show ACK state and width");
    _respond("  ACK ON|OFF                      Enable/disable ACK pulse");
    _respond("  ACK <width_us>                  Set ACK pulse width (1000-20000)");
    _respond("  ACK TEST [count]                Fire ACK pulse(s) for testing");
    _respond("  STATUS                          Show current address");
    _respond("  HELP                            Show this help");

}

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

void DecoderCommandParser_initialize(void) {

    _current_addr = 3;
    _current_type = DCC_ADDRESS_SHORT;

}

void DecoderCommandParser_process(void) {

    if (!TI_UartDriver_read_line(_line_buf, CMD_LINE_MAX)) {

        return;

    }

    _strupper(_line_buf);

    char *tokens[CMD_MAX_TOKENS];
    int count = _tokenize(_line_buf, tokens, CMD_MAX_TOKENS);

    if (count == 0) {

        _prompt();
        return;

    }

    if (strcmp(tokens[0], "ADDR") == 0) {

        _cmd_addr(tokens, count);

    } else if (strcmp(tokens[0], "CLEAR") == 0) {

        _cmd_clear();

    } else if (strcmp(tokens[0], "ACK") == 0) {

        _cmd_ack(tokens, count);

    } else if (strcmp(tokens[0], "STATUS") == 0) {

        _cmd_status();

    } else if (strcmp(tokens[0], "HELP") == 0) {

        _cmd_help();

    } else {

        snprintf(_resp_buf, sizeof(_resp_buf), "ERR: unknown command '%s'",
                 tokens[0]);
        _respond(_resp_buf);

    }

    _prompt();

}
