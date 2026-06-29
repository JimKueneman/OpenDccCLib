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
 * @file dcc_railcom_decoder.c
 * @brief RailCom 4/8 encoding and datagram transmission for decoders.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#include "dcc_railcom_decoder.h"
#include "dcc_railcom_utilities.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)

#include <string.h>

// =============================================================================
// Static state
// =============================================================================

static const interface_dcc_railcom_decoder_t *_interface;

    /** @brief This decoder's active address, pushed by the packet decoder. */
static dcc_address_t _decoder_address;

    /** @brief This decoder's address type (short / long). */
static dcc_address_type_enum _decoder_address_type;

    /** @brief Toggles ADR1 (HIGH) / ADR2 (LOW) on successive cutouts. */
static bool _adr_alternate;

    /** @brief Encoded Channel 1 (ADR) bytes pending transmit in the next cutout. */
static uint8_t _pending_ch1[DCC_RAILCOM_CH1_MAX_BYTES];

    /** @brief A complete addressed command was recognized; the next byte is the XOR. */
static bool _pending_armed;

    /** @brief Encoded Channel 2 bytes (datagram or raw token) pending transmit. */
static uint8_t _pending_ch2[DCC_RAILCOM_DATAGRAM_MAX_BYTES + 1];

    /** @brief Number of pending Channel 2 bytes (0 = no Channel 2 this cutout). */
static uint8_t _pending_ch2_count;

// =============================================================================
// Public API
// =============================================================================

void DccRailcomDecoder_initialize(const interface_dcc_railcom_decoder_t *interface) {

    _interface = interface;
    _adr_alternate = false;
    _pending_armed = false;
    _pending_ch2_count = 0;

}

void DccRailcomDecoder_set_address(dcc_address_t address, dcc_address_type_enum type) {

    _decoder_address = address;
    _decoder_address_type = type;

}

// =============================================================================
// Decoder-side recognizer -- pure DCC packet-length logic (no side effects)
// =============================================================================

    /**
     * @brief Address byte count implied by a multifunction packet's leading byte.
     * @param first First packet byte.
     * @return 1 (broadcast / short / idle), 2 (long), or 0 (accessory / reserved --
     *         not a multifunction-decoder packet this module sizes).
     */
static uint8_t _address_byte_count(uint8_t first) {

    if (first == DCC_ADDRESS_BROADCAST_VALUE) {

        return 1;

    }

    if (first <= DCC_ADDRESS_SHORT_MAX) {

        return 1;   /* 0x01-0x7F short address */

    }

    if (first == DCC_ADDRESS_IDLE_VALUE) {

        return 1;   /* 0xFF idle */

    }

    if (first >= 0xC0 && first <= 0xE7) {

        return 2;   /* 0xC0-0xE7 long (14-bit) address */

    }

    return 0;       /* 0x80-0xBF accessory, 0xE8-0xFE reserved */

}

    /**
     * @brief Instruction byte count implied by an instruction opcode (S-9.2.1).
     * @param opcode First instruction byte (after the address).
     * @return Instruction length in bytes, or 0 if not sizable from the opcode alone
     *         (extended XPOM form, or an instruction not sized by this module).
     */
static uint8_t _instruction_byte_count(uint8_t opcode) {

    if ((opcode & 0xF0) == 0x10) {

        return 2;   /* consist control: 0001xxxx + consist address */

    }

    if ((opcode & 0xC0) == 0x40) {

        return 1;   /* speed / direction 14/28-step: 01Dxxxxx */

    }

    if ((opcode & 0xE0) == 0x20) {

        if (opcode == DCC_ADV_OPS_128_SPEED) {

            return 2;   /* 0x3F + speed */

        }

        if (opcode == DCC_ADV_OPS_ANALOG_FUNCTION) {

            return 3;   /* 0x3D + output + data */

        }

        return 0;       /* other advanced ops: not sized here */

    }

    if ((opcode & 0xE0) == 0x80) {

        return 1;   /* function group 1: 100xxxxx */

    }

    if ((opcode & 0xE0) == 0xA0) {

        return 1;   /* function group 2: 101xxxxx */

    }

    if ((opcode & 0xE0) == 0xC0) {

        if (opcode == DCC_FEAT_BINARY_STATE_LONG) {

            return 3;   /* 0xC0 + 2 data bytes */

        }

        if (opcode == DCC_FEAT_BINARY_STATE_SHORT) {

            return 2;   /* 0xDD + 1 data byte */

        }

        if (opcode == DCC_FEAT_F13_F20 || opcode == DCC_FEAT_F21_F28 ||
            opcode == DCC_FEAT_F29_F36 || opcode == DCC_FEAT_F37_F44 ||
            opcode == DCC_FEAT_F45_F52 || opcode == DCC_FEAT_F53_F60 ||
            opcode == DCC_FEAT_F61_F68) {

            return 2;   /* feature-expansion function groups: opcode + 1 data */

        }

        return 0;       /* time/date, system time, others: not sized here */

    }

    if ((opcode & 0xE0) == 0xE0) {

        if ((opcode & 0xF0) == 0xF0) {

            return 2;   /* short-form CV access: 1111GGGG + data */

        }

        if (opcode == DCC_CV_LONG_VERIFY || opcode == DCC_CV_LONG_BIT ||
            opcode == DCC_CV_LONG_WRITE) {

            return 3;   /* standard POM long form: 1110GGVV VVVVVVVV DDDDDDDD */

        }

        return 0;       /* XPOM (1110GGSS) and other 0xE0 forms: length not yet grounded */

    }

    return 0;           /* decoder control (0000xxxx) and anything else: not sized */

}

    /**
     * @brief Total DCC packet length (including XOR) implied by the bytes so far.
     *
     * @details Algorithm:
     * -# Size the address from byte 0 (1 or 2 bytes); 0 if not a multifunction packet.
     * -# Once the opcode byte is present, size the instruction from it.
     * -# Total = address bytes + instruction bytes + 1 (XOR).
     * Returns 0 while still undeterminable, or for an extended form (XPOM) whose
     * length is not yet grounded.
     *
     * @verbatim
     * @param data  Packet bytes received so far.
     * @param count Number of bytes in data.
     * @endverbatim
     *
     * @return Total packet length including XOR, or 0 if undeterminable.
     */
uint8_t DccRailcomDecoder_packet_length(const uint8_t *data, uint8_t count) {

    uint8_t address_bytes;
    uint8_t instruction_bytes;

    if (count < 1) {

        return 0;

    }

    address_bytes = _address_byte_count(data[0]);

    if (address_bytes == 0) {

        return 0;

    }

    if (count < (uint8_t)(address_bytes + 1)) {

        return 0;   /* opcode not yet received */

    }

    instruction_bytes = _instruction_byte_count(data[address_bytes]);

    if (instruction_bytes == 0) {

        return 0;   /* undeterminable, or extended (XPOM) */

    }

    return (uint8_t)(address_bytes + instruction_bytes + 1);   /* + XOR */

}

    /**
     * @brief Extract the multifunction address and its type from a packet's leading bytes.
     *
     * @details Algorithm:
     * -# Size the address (1 or 2 bytes); reject accessory/reserved/short-of-bytes.
     * -# Broadcast (0x00) and idle (0xFF) decode to their own types.
     * -# 1-byte form is a short address; 2-byte form is a 14-bit long address.
     *
     * @verbatim
     * @param data    Packet bytes received so far.
     * @param count   Number of bytes in data.
     * @param address Out: decoded address.
     * @param type    Out: decoded address type.
     * @endverbatim
     *
     * @return true if an address was decoded; false otherwise.
     */
bool DccRailcomDecoder_packet_address(const uint8_t *data, uint8_t count,
            dcc_address_t *address, dcc_address_type_enum *type) {

    uint8_t address_bytes;

    if (count < 1) {

        return false;

    }

    address_bytes = _address_byte_count(data[0]);

    if (address_bytes == 0 || count < address_bytes) {

        return false;

    }

    if (data[0] == DCC_ADDRESS_BROADCAST_VALUE) {

        *address = 0;
        *type = DCC_ADDRESS_BROADCAST;
        return true;

    }

    if (data[0] == DCC_ADDRESS_IDLE_VALUE) {

        *address = DCC_ADDRESS_IDLE_VALUE;
        *type = DCC_ADDRESS_IDLE;
        return true;

    }

    if (address_bytes == 1) {

        *address = data[0];
        *type = DCC_ADDRESS_SHORT;
        return true;

    }

    *address = (dcc_address_t)(((uint16_t)(data[0] & 0x3F) << 8) | data[1]);
    *type = DCC_ADDRESS_LONG;
    return true;

}

// =============================================================================
// Decoder-side dispatch -- recognize an addressed command before the XOR and
// answer in the following cutout (ADR on Channel 1)
// =============================================================================

    /**
     * @brief XOR-validate the assembled packet: XOR of all data bytes == last byte.
     * @param data Packet bytes (including the trailing XOR byte).
     * @param count Number of bytes in data.
     * @return true if the packet's XOR check passes.
     */
static bool _xor_valid(const uint8_t *data, uint8_t count) {

    uint8_t xor_val = 0;
    uint8_t byte_index;

    if (count < 2) {

        return false;

    }

    for (byte_index = 0; byte_index < (uint8_t)(count - 1); byte_index++) {

        xor_val ^= data[byte_index];

    }

    return xor_val == data[count - 1];

}

    /**
     * @brief Fill the pending Channel 1 slot with the ADR datagram, alternating
     *  ADR1 (HIGH bits) / ADR2 (LOW bits), encoded from the cached decoder address.
     */
static void _fill_adr(void) {

    if (!_adr_alternate) {

        DccRailcomUtilities_encode_ch1(DCC_RAILCOM_ID_ADR1_HIGH,
                (uint8_t)((_decoder_address >> 8) & 0x3F), _pending_ch1);

    } else {

        DccRailcomUtilities_encode_ch1(DCC_RAILCOM_ID_ADR2_LOW,
                (uint8_t)(_decoder_address & 0xFF), _pending_ch1);

    }

    _adr_alternate = !_adr_alternate;

}

    /**
     * @brief Fill the pending Channel 2 slot from the app's reply to the recognized
     *  command: DATA encodes a datagram; ACK/BUSY/NACK load the raw token code word;
     *  NONE (or no hook) leaves Channel 2 empty.
     * @param data Packet bytes received so far.
     * @param count Number of bytes in data.
     */
static void _fill_ch2(const uint8_t *data, uint8_t count) {

    dcc_railcom_response_t out;
    dcc_railcom_reply_status_enum status;
    uint8_t address_bytes;

    _pending_ch2_count = 0;

    if (!_interface || !_interface->on_railcom_request) {

        return;

    }

    address_bytes = _address_byte_count(data[0]);
    memset(&out, 0, sizeof(out));

    status = _interface->on_railcom_request(&data[address_bytes],
            (uint8_t)(count - address_bytes), &out);

    switch (status) {

        case DCC_RAILCOM_REPLY_DATA:

            _pending_ch2_count = DccRailcomUtilities_encode_ch2(&out, _pending_ch2);

            break;

        case DCC_RAILCOM_REPLY_ACK:

            _pending_ch2[0] = DCC_RAILCOM_CODE_WORD_ACK;
            _pending_ch2_count = 1;

            break;

        case DCC_RAILCOM_REPLY_NACK:

            _pending_ch2[0] = DCC_RAILCOM_CODE_WORD_NACK;
            _pending_ch2_count = 1;

            break;

        case DCC_RAILCOM_REPLY_BUSY:

            _pending_ch2[0] = DCC_RAILCOM_CODE_WORD_BUSY;
            _pending_ch2_count = 1;

            break;

        case DCC_RAILCOM_REPLY_NONE:
        default:

            break;

    }

}

    /**
     * @brief Bit-bang one byte as a 250 kbaud UART frame: start bit "0", 8 data bits
     *  least-significant first, stop bit "1" (S-9.3.2 sec 2.4), 4us per bit. Timing comes
     *  from the app's delay_us, which must be accurate at the 4us bit period.
     */
static void _transmit_byte(uint8_t value) {

    uint8_t bit;

    _interface->tx_pin_set(false);                  /* start bit */
    _interface->delay_us(DCC_RAILCOM_TX_BIT_US);

    for (bit = 0; bit < 8; bit++) {

        _interface->tx_pin_set((value >> bit) & 0x01);
        _interface->delay_us(DCC_RAILCOM_TX_BIT_US);

    }

    _interface->tx_pin_set(true);                   /* stop bit */
    _interface->delay_us(DCC_RAILCOM_TX_BIT_US);

}

    /**
     * @brief Bit-bang the pending ADR (Channel 1) and any Channel 2 reply into the cutout,
     *  timed off the packet end-bit edge: blank -> Ch1 -> gap -> Ch2 (S-9.3.2 sec 2.4).
     *  Runs with all interrupts locked (4us bit timing); the lock also masks the DCC edge
     *  IRQ, so the decoder's own injected current cannot self-trigger it. Blocks ~454us.
     */
static void _transmit_pending(void) {

    uint8_t byte_index;

    if (!_interface || !_interface->tx_pin_set || !_interface->delay_us) {

        return;

    }

    if (_interface->lock_shared_resources) {

        _interface->lock_shared_resources();

    }

    _interface->tx_pin_set(true);                   /* idle (no current) through the blank */
    _interface->delay_us(DCC_RAILCOM_TX_BLANK_US);  /* end-bit edge -> Ch1 start (T_TS1) */

    for (byte_index = 0; byte_index < DCC_RAILCOM_CH1_MAX_BYTES; byte_index++) {

        _transmit_byte(_pending_ch1[byte_index]);

    }

    _interface->delay_us(DCC_RAILCOM_TX_GAP_US);    /* Ch1 end -> Ch2 start (T_TS2) */

    for (byte_index = 0; byte_index < _pending_ch2_count; byte_index++) {

        _transmit_byte(_pending_ch2[byte_index]);

    }

    _interface->tx_pin_set(true);                   /* leave the line idle */

    if (_interface->unlock_shared_resources) {

        _interface->unlock_shared_resources();

    }

}

void DccRailcomDecoder_on_byte_received(const uint8_t *data, uint8_t count) {

    dcc_address_t address;
    dcc_address_type_enum type;

    /* A new packet starts at count 1 -- drop any stale armed state. */
    if (count == 1) {

        _pending_armed = false;

    }

    /* Already recognized this packet -- wait for the end-bit edge (transmit). */
    if (_pending_armed) {

        return;

    }

    /* Recognize a complete command (next byte is the XOR) addressed to this decoder. */
    if (DccRailcomDecoder_packet_length(data, count) != (uint8_t)(count + 1)) {

        return;

    }

    if (DccRailcomDecoder_packet_address(data, count, &address, &type)
            && type == _decoder_address_type && address == _decoder_address) {

        _fill_adr();
        _fill_ch2(data, count);
        _pending_armed = true;

    }

}

void DccRailcomDecoder_transmit(const uint8_t *data, uint8_t count) {

    /* Only respond to a command we recognized as addressed to us before its XOR. */
    if (!_pending_armed) {

        return;

    }

    _pending_armed = false;

    /* Validate the complete packet's XOR; transmit ADR (+ Ch2) only if it passes. */
    if (_xor_valid(data, count)) {

        _transmit_pending();

    }

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
