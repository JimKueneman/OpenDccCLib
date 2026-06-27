// callbacks_dcc.c
//
// Application callback implementations for the command station demo.
// See callbacks_dcc.h for how to add new callbacks.

#include "callbacks_dcc.h"
#include "ti_msp_dl_config.h"
#include "dcc_lib/dcc_defines.h"   /* DCC_SERVICE_MODE_ACK_BLANK_PACKETS */
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "application_drivers/ti_driverlib_dcc_driver.h"

#ifdef DCC_COMPILE_COMMAND_STATION

// Test trigger state. When armed, the next NON-idle packet raises PB3 once so a
// logic analyzer can hardware-trigger on the exact packet under test.
static volatile bool _test_trigger_armed = false;

// When true, the width-test mock fires on the FIRST command packet (before the
// ACK blanking window) instead of in-window -- used to prove the library masks an
// early pulse (S-9.2.3 line 55 boundary test).
static volatile bool _mock_ack_early = false;

// The DCC idle packet (S-9.2): 11111111 00000000 11111111.
static bool _is_idle_packet(const dcc_packet_t *p) {

    return p->byte_count == 3 &&
           p->data[0] == 0xFF && p->data[1] == 0x00 && p->data[2] == 0xFF;
}

void CallbacksDcc_arm_trigger(void) {

    // Drop PB3 low first so the armed packet produces one clean rising edge.
    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_PACKET_LOAD_PIN);
    _test_trigger_armed = true;
}

// ---------------------------------------------------------------------------
// Mock decoder (HIL only). Holds ONE CV value and behaves like a decoder on the
// programming track: Direct WRITE commands update the held value; Direct VERIFY
// commands that match it fire a valid-width mock-ACK pulse (PB24 -> PB9). This
// lets the bench exercise read-back and write+verify end-to-end through the real
// ACK path -- both success (value matches) and failure (mock off / wrong value).
// ---------------------------------------------------------------------------
#define MOCK_DECODER_ACK_US 6000u   /* valid pulse (~103 of the 85..120 sample window) */

static volatile bool     _mock_dec_enabled = false;
static volatile uint16_t _mock_dec_cv      = 0;
static volatile uint8_t  _mock_dec_value   = 0;
static volatile bool     _mock_dec_fired   = false;  /* ACKed once this operation */

/* Command-packet position within the current service-mode op (reset by each
 * reset packet). The mock only ACKs once this passes the library's ACK blanking
 * window -- i.e. it behaves like a compliant decoder that cannot ACK until it
 * has received 2 identical command packets (S-9.2.3 line 55). */
static volatile uint8_t  _mock_cmd_count   = 0;

void CallbacksDcc_mock_decoder_set(uint16_t cv, uint8_t value) {

    _mock_dec_cv = cv;
    _mock_dec_value = value;
    _mock_dec_enabled = true;
}

void CallbacksDcc_mock_decoder_off(void) {

    _mock_dec_enabled = false;
}

void CallbacksDcc_set_mock_ack_early(bool early) {

    _mock_ack_early = early;
}

static void _mock_decoder_handle(const dcc_packet_t *p, bool window_open) {

    if (!_mock_dec_enabled || p->byte_count < 4) {

        return;
    }

    uint8_t b0 = p->data[0];
    if ((b0 & 0xF0) != 0x70) {

        return;                          // not a Direct service-mode command
    }

    // Direct command: 0111CCAA AAAAAAAA <data> EEEEEEEE. CV = (AA:AAAAAAAA) + 1.
    uint16_t cv = (uint16_t)((((b0 & 0x03) << 8) | p->data[1]) + 1);
    if (cv != _mock_dec_cv) {

        return;
    }

    uint8_t cc = (uint8_t)((b0 >> 2) & 0x03);   // 01 verify byte, 11 write byte, 10 bit-manip
    uint8_t db = p->data[2];
    bool match = false;

    if (cc == 0x03) {                           // write byte -> accept into held value

        _mock_dec_value = db;

    } else if (cc == 0x01) {                    // verify byte

        match = (db == _mock_dec_value);

    } else if (cc == 0x02) {                    // bit manipulation: data byte 111KDBBB

        uint8_t bit = (uint8_t)(db & 0x07);
        uint8_t d   = (uint8_t)((db >> 3) & 1);

        if ((db >> 4) & 1) {                    // K=1 write bit -> update held value

            if (d) { _mock_dec_value |= (uint8_t)(1u << bit); }
            else   { _mock_dec_value &= (uint8_t)~(1u << bit); }

        } else {                                // K=0 verify bit

            match = (((_mock_dec_value >> bit) & 1) == d);
        }
    }

    // ACK a matching verify once per operation, but only after the library's ACK
    // scan window has opened (a compliant decoder does not ACK before 2 packets).
    if (match && window_open && !_mock_dec_fired) {

        TI_DccDriver_mock_ack_fire(MOCK_DECODER_ACK_US);
        _mock_dec_fired = true;
    }
}

void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet) {

    // When armed, fire the trigger on the first non-idle packet (the packet
    // under test), then disarm. PB3 stays quiet otherwise, so the rising edge
    // is unambiguous for the analyzer's digital trigger.
    if (_test_trigger_armed && !_is_idle_packet(packet)) {

        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_PACKET_LOAD_PIN);
        _test_trigger_armed = false;

    }

    // Track command-packet position within the service-mode op so the mock ACKs
    // only AFTER the library's ACK scan window opens (S-9.2.3 line 55: after the
    // 2nd command packet). Reset packets (00 00 00) bound each op; service-mode
    // command packets are 0111xxxx.
    bool is_reset = (packet->byte_count == 3 &&
                     packet->data[0] == 0 && packet->data[1] == 0 && packet->data[2] == 0);
    bool is_command = (packet->byte_count >= 3 && (packet->data[0] & 0xF0) == 0x70);

    if (is_reset) {

        _mock_cmd_count = 0;
        _mock_dec_fired = false;

    } else if (is_command && _mock_cmd_count < 255) {

        _mock_cmd_count++;
    }

    bool window_open = (_mock_cmd_count > DCC_SERVICE_MODE_ACK_BLANK_PACKETS);

    // Width-test mock (SVC MOCKACK): fire the armed pulse on the first Direct
    // bit-verify command (0111 10AA, byte[0] & 0xFC == 0x78). Normally gated to
    // the open window; in EARLY mode it fires on the first (blanked) packet so a
    // test can confirm the library masks an early pulse.
    if ((window_open || _mock_ack_early) &&
        packet->byte_count >= 1 && (packet->data[0] & 0xFC) == 0x78) {

        TI_DccDriver_mock_ack_on_command();
    }

    // Mock decoder (SVC MOCKCV): writes update the held value always; a matching
    // verify ACKs only once the window is open. Inert unless SVC MOCKCV set.
    _mock_decoder_handle(packet, window_open);
}

// RailCom cutout cancel (HIL only, S-9.3.2 CS-008). `RAILCOM CANCEL` arms this; the
// 58us bit-timer ISR (same priority as the cutout one-shot ISR, so no nesting) calls
// cancel_tick() each tick. To make the cancel land deterministically EARLY in a
// cutout (during SETTLING/CH1, past DELAY so the H-bridge is tristated), we wait for
// a cutout that BEGINS after arming -- detected as a rising edge of "cutout active" --
// then fire ~2 ticks (~58-116us) in. PB2 then shows one short pulse instead of the
// full ~440us. One-shot: disarms after firing.
static volatile bool    _railcom_cancel_armed    = false;
static volatile bool    _railcom_prev_active     = false;
static volatile bool    _railcom_cancel_counting = false;
static volatile uint8_t _railcom_cancel_ticks    = 0;

void CallbacksDcc_arm_railcom_cancel(void) {

    _railcom_cancel_armed = true;
    _railcom_cancel_counting = false;
}

void CallbacksDcc_railcom_cancel_tick(void) {

    bool active = DccConfig_railcom_cutout_is_active();

    if (_railcom_cancel_armed) {

        if (active && !_railcom_prev_active) {   /* a NEW cutout just began */

            _railcom_cancel_counting = true;
            _railcom_cancel_ticks = 0;
        }

        if (_railcom_cancel_counting && active) {

            _railcom_cancel_ticks++;

            if (_railcom_cancel_ticks >= 2) {    /* ~SETTLING/CH1: past DELAY, H-bridge tristated */

                DccConfig_cancel_railcom_cutout();
                _railcom_cancel_armed = false;
                _railcom_cancel_counting = false;
            }
        }
    }

    _railcom_prev_active = active;
}

#endif /* DCC_COMPILE_COMMAND_STATION */
