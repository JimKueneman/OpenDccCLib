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
 * @file callbacks_dcc.c
 * @brief Application callback implementations for the command station demo. See callbacks_dcc.h for how to add new callbacks.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "callbacks_dcc.h"
#include "ti_msp_dl_config.h"
#include "dcc_lib/dcc_defines.h"   /* DCC_SERVICE_MODE_ACK_BLANK_PACKETS */
#include "application_drivers/ti_driverlib_uart_driver.h"
#include "application_drivers/ti_driverlib_dcc_driver.h"
#include <stdio.h>

#ifdef DCC_COMPILE_COMMAND_STATION

    /** @brief Packet-trigger state: when true the next NON-idle packet dispatched raises PB3 once. */
static volatile bool _test_trigger_armed = false;

    /** @brief Insert-trigger state: when true the next main-track insert raises PB3 at once (from the parser, not a packet) for scheduler latency. */
static volatile bool _test_trigger_on_insert = false;

    /** @brief When true the width-test mock fires on the FIRST command packet (inside the ACK blanking window) to prove the library masks it (S-9.2.3 line 55). */
static volatile bool _mock_ack_early = false;

    /**
     * @brief True when p is the DCC idle packet (S-9.2): 11111111 00000000 11111111.
     *
     * @param p  Pointer to the packet to test.
     *
     * @return true for the 3-byte FF 00 FF idle packet, false otherwise.
     */
static bool _is_idle_packet(const dcc_packet_t *p) {

    return p->byte_count == 3 &&
           p->data[0] == 0xFF && p->data[1] == 0x00 && p->data[2] == 0xFF;
}

    /**
     * @brief Arm the PB3 test trigger for the next non-idle packet.
     *
     * @details Drops PB3 low first so the armed packet produces one clean rising
     * edge, cancels any pending insert-trigger arm, then sets the packet arm.
     */
void CallbacksDcc_arm_trigger(void) {

    // Drop PB3 low first so the armed packet produces one clean rising edge.
    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_PACKET_LOAD_PIN);
    _test_trigger_on_insert = false;
    _test_trigger_armed = true;
}

    /**
     * @brief Arm the PB3 test trigger for the next main-track insert.
     *
     * @details Drops PB3 low, cancels any pending packet-trigger arm, then sets the
     * insert arm so CallbacksDcc_on_main_track_insert() raises PB3.
     */
void CallbacksDcc_arm_trigger_on_insert(void) {

    DL_GPIO_clearPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_PACKET_LOAD_PIN);
    _test_trigger_armed = false;
    _test_trigger_on_insert = true;
}

    /**
     * @brief Raise PB3 on a main-track insert when the insert trigger is armed, then disarm.
     */
void CallbacksDcc_on_main_track_insert(void) {

    if (_test_trigger_on_insert) {

        DL_GPIO_setPins(GPIO_GRP_SALEAE_PORT, GPIO_GRP_SALEAE_PACKET_LOAD_PIN);
        _test_trigger_on_insert = false;
    }
}

// ---------------------------------------------------------------------------
// Mock decoder (HIL only). Holds ONE CV value and behaves like a decoder on the
// programming track: Direct WRITE commands update the held value; Direct VERIFY
// commands that match it fire a valid-width mock-ACK pulse (PB24 -> PB9). This
// lets the bench exercise read-back and write+verify end-to-end through the real
// ACK path -- both success (value matches) and failure (mock off / wrong value).
// ---------------------------------------------------------------------------
    /** @brief Mock-decoder ACK pulse width in microseconds: a valid 6 ms pulse (~103 of the 85..120 sample window). */
#define MOCK_DECODER_ACK_US 6000u   /* valid pulse (~103 of the 85..120 sample window) */

    /** @brief Mock decoder enabled (SVC MOCKCV set). */
static volatile bool     _mock_dec_enabled = false;
    /** @brief The single 1-based CV number the mock decoder holds. */
static volatile uint16_t _mock_dec_cv      = 0;
    /** @brief Current value of the held CV; updated by Direct/bit writes. */
static volatile uint8_t  _mock_dec_value   = 0;
    /** @brief True once the mock has ACKed during the current service-mode operation (reset by each reset packet). */
static volatile bool     _mock_dec_fired   = false;  /* ACKed once this operation */

    /** @brief Command-packet position within the current service-mode op (reset by each reset packet); the mock ACKs only once this passes the library ACK blanking window (S-9.2.3 line 55). */
static volatile uint8_t  _mock_cmd_count   = 0;

    /**
     * @brief Enable the HIL mock decoder holding one CV value.
     *
     * @verbatim
     * @param cv     1-based CV number the mock decoder holds.
     * @param value  Initial value of that CV.
     * @endverbatim
     */
void CallbacksDcc_mock_decoder_set(uint16_t cv, uint8_t value) {

    _mock_dec_cv = cv;
    _mock_dec_value = value;
    _mock_dec_enabled = true;
}

    /** @brief Disable the HIL mock decoder. */
void CallbacksDcc_mock_decoder_off(void) {

    _mock_dec_enabled = false;
}

    /**
     * @brief Select early (blanked) or in-window firing for the width-test mock.
     *
     * @verbatim
     * @param early  true = fire on the first (blanked) command packet, false = fire in-window.
     * @endverbatim
     */
void CallbacksDcc_set_mock_ack_early(bool early) {

    _mock_ack_early = early;
}

    /**
     * @brief Behave like a one-CV decoder on the programming track for the packet just dispatched.
     *
     * @details Algorithm:
     * -# Return at once when the mock is disabled.
     * -# 3-byte non-Direct byte-VERIFY (byte0 0x70-0x77, the register/paged 0..255
     *    scan): ACK when the verified data byte matches the held value, so the scan
     *    terminates early instead of running all 256 values (~90 s). Match on value
     *    only; writes (0x78-0x7F, incl. the 0x7D page preset) are ignored.
     * -# Otherwise require a 4-byte Direct command (0111CCAA AAAAAAAA data EEEEEEEE)
     *    whose CV (AA:AAAAAAAA + 1) equals the held CV.
     * -# CC=11 write byte: accept into the held value. CC=01 verify byte: match when
     *    equal. CC=10 bit manipulation (111KDBBB): K=1 writes bit BBB to D, K=0
     *    matches when bit BBB equals D.
     * -# On a match, fire one valid-width mock ACK, but only once per operation and
     *    only after the library ACK scan window has opened.
     *
     * @param p            Pointer to the packet just dispatched.
     * @param window_open  true once the library ACK blanking window has passed.
     */
static void _mock_decoder_handle(const dcc_packet_t *p, bool window_open) {

    if (!_mock_dec_enabled) {

        return;
    }

    // Non-Direct (register / paged / address) byte-VERIFY: 3-byte command
    // 0111 0RRR DDDDDDDD EEEEEEEE (byte0 0x70-0x77). These drive the 0..255 byte-read
    // SCAN that a register/paged read-modify-write performs. ACK when the verified
    // data byte matches the held value so the scan terminates early -- otherwise the
    // blind scan runs all 256 values (~90 s) and the read-modify-write is impossible
    // to capture. Match on value only (register/CV correspondence is irrelevant here);
    // writes (0x78-0x7F, incl. the 0x7D page-preset) are left alone.
    if (p->byte_count == 3 && (p->data[0] & 0xF8) == 0x70) {

        if (p->data[1] == _mock_dec_value && window_open && !_mock_dec_fired) {

            TI_DccDriver_mock_ack_fire(MOCK_DECODER_ACK_US);
            _mock_dec_fired = true;
        }
        return;
    }

    if (p->byte_count < 4) {

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

    /**
     * @brief Library on_packet_sent hook: PB3 trigger, ACK-window tracking, and the two service-mode mocks.
     *
     * @details Algorithm:
     * -# When the packet trigger is armed and this is not an idle packet, raise PB3
     *    and disarm, so the rising edge is unambiguous for the analyzer trigger.
     * -# Track the command-packet position within the service-mode op: a reset
     *    packet (00 00 00) zeroes the count and the fired flag; a service command
     *    packet (0111xxxx) increments it. The ACK window is open once the count
     *    exceeds DCC_SERVICE_MODE_ACK_BLANK_PACKETS (S-9.2.3 line 55).
     * -# Width-test mock (SVC MOCKACK): start the armed pulse on a Direct bit-verify
     *    command (byte0 & 0xFC == 0x78) when the window is open, or on the first
     *    (blanked) packet in EARLY mode.
     * -# Mock decoder (SVC MOCKCV): hand the packet to _mock_decoder_handle().
     *
     * Fires from DccConfig_run() at packet dispatch (transmit start), which is what
     * the bench trigger timing relies on.
     *
     * @verbatim
     * @param packet  Pointer to the dcc_packet_t just handed to the encoder.
     * @endverbatim
     */
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

#if defined(DCC_COMPILE_RAILCOM)

// RailCom cutout cancel (HIL only, S-9.3.2 CS-008). `RAILCOM CANCEL` arms this; the
// 58us bit-timer ISR (same priority as the cutout one-shot ISR, so no nesting) calls
// cancel_tick() each tick. To make the cancel land deterministically EARLY in a
// cutout (during SETTLING/CH1, past DELAY so the PB2 cutout strobe is already up), we
// wait for a cutout that BEGINS after arming -- detected as a rising edge of "cutout
// active" -- then fire ~2 ticks (~58-116us) in. PB2 then shows one short pulse
// instead of the full ~440us. One-shot: disarms after firing.
    /** @brief RAILCOM CANCEL armed; cleared once the cancel fires. */
static volatile bool    _railcom_cancel_armed    = false;
    /** @brief Cutout-active level seen on the previous tick, for rising-edge detection. */
static volatile bool    _railcom_prev_active     = false;
    /** @brief True while counting ticks into the cutout that began after arming. */
static volatile bool    _railcom_cancel_counting = false;
    /** @brief Ticks elapsed since that cutout began; the cancel fires at 2. */
static volatile uint8_t _railcom_cancel_ticks    = 0;

    /**
     * @brief Arm a one-shot cancel of the next RailCom cutout that begins after this call.
     */
void CallbacksDcc_arm_railcom_cancel(void) {

    _railcom_cancel_armed = true;
    _railcom_cancel_counting = false;
}

    /**
     * @brief 58 us ISR tick: fire the armed cutout cancel about two ticks into a new cutout.
     *
     * @details Algorithm:
     * -# Read DccConfig_railcom_cutout_is_active().
     * -# When armed and the level just rose, start counting from zero.
     * -# While counting and still active, count the tick; at 2 ticks (past DELAY,
     *    in SETTLING/CH1) call DccConfig_cancel_railcom_cutout() and disarm.
     * -# Remember the level for the next edge detection.
     */
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


// ---------------------------------------------------------------------------
// RailCom datagram result (HIL loopback). The library decoded one channel of
// the last cutout from the bytes .uart_read tagged with that channel; report
// the result (a datagram, ACK/NACK, or an error code) with the address it was
// tagged with so the host can check content, channel, result AND the two-stage
// address capture on the wire.
// ---------------------------------------------------------------------------

    /** @brief RC RESULT lines reported since the last RC MOCK OFF. */
static volatile uint32_t _rc_result_count = 0;

    /**
     * @brief Short name of a RailCom decode result for the RC RESULT line.
     *
     * @param result  Library decode result.
     *
     * @return The enum name without its DCC_RAILCOM_RESULT_ prefix, or "UNKNOWN".
     */
static const char *_railcom_result_name(dcc_railcom_result_enum result) {

    switch (result) {
        case DCC_RAILCOM_RESULT_OK:               return "OK";
        case DCC_RAILCOM_RESULT_ACK:              return "ACK";
        case DCC_RAILCOM_RESULT_NACK:             return "NACK";
        case DCC_RAILCOM_RESULT_INVALID_CODEWORD: return "INVALID_CODEWORD";
        case DCC_RAILCOM_RESULT_DATA_AFTER_CONTROL_WORD: return "DATA_AFTER_CONTROL_WORD";
        case DCC_RAILCOM_RESULT_TOO_FEW_BYTES:    return "TOO_FEW_BYTES";
        case DCC_RAILCOM_RESULT_TOO_MANY_BYTES:   return "TOO_MANY_BYTES";
        case DCC_RAILCOM_RESULT_INVALID_CHANNEL:  return "INVALID_CHANNEL";
        default:                                  return "UNKNOWN";
    }
}

    /**
     * @brief Library on_railcom_datagram_result hook: print one RC RESULT line and count it.
     *
     * @details Formats "RC RESULT: addr=<n> ch=<1|2|0> res=<result> id=<n> n=<bytes> data=<hex..>"
     * (ch=0 for a bad channel tag; id, n and data are meaningful only for res=OK)
     * into a local buffer, stopping early if the line would overflow, then writes it
     * on the command UART. Runs from DccConfig_run(), so blocking UART output is fine.
     *
     * @verbatim
     * @param address   DCC address the library tagged the datagram with.
     * @param channel   RailCom channel the bytes arrived in (DCC_RAILCOM_CH1 or CH2; other = bad tag).
     * @param datagram  Pointer to the decoded dcc_railcom_datagram_t; check its result.
     * @endverbatim
     */
void CallbacksDcc_on_railcom_datagram(uint16_t address, uint8_t channel,
                                      const dcc_railcom_datagram_t *datagram) {

    char line[96];
    unsigned ch = (channel == DCC_RAILCOM_CH1) ? 1u : ((channel == DCC_RAILCOM_CH2) ? 2u : 0u);
    int  n = snprintf(line, sizeof(line), "RC RESULT: addr=%u ch=%u res=%s id=%u n=%u data=",
                      (unsigned)address, ch, _railcom_result_name(datagram->result),
                      (unsigned)datagram->datagram_id, (unsigned)datagram->count);

    for (uint8_t i = 0; i < datagram->count && n > 0 && n < (int)sizeof(line) - 4; i++) {
        n += snprintf(&line[n], sizeof(line) - (size_t)n, "%02X%s",
                      datagram->data[i], (i + 1 < datagram->count) ? " " : "");
    }

    _rc_result_count++;
    TI_UartDriver_write_string(line);
    TI_UartDriver_write_string("\r\n");
}

    /**
     * @brief Number of RC RESULT lines reported since the last reset.
     *
     * @return Count of decoded RailCom datagrams reported.
     */
uint32_t CallbacksDcc_railcom_result_count(void) {

    return _rc_result_count;
}

    /** @brief Zero the RC RESULT counter. */
void CallbacksDcc_railcom_reset_result_count(void) {

    _rc_result_count = 0;
}

#endif /* DCC_COMPILE_RAILCOM */

#endif /* DCC_COMPILE_COMMAND_STATION */
