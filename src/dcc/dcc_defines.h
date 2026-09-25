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
 * @file dcc_defines.h
 * @brief Protocol constants for NMRA DCC (S-9.1, S-9.2, S-9.2.1, S-9.3.2).
 *
 * @details Defines bit timing values, instruction byte masks, CV numbers,
 * preamble lengths, RailCom constants, and other protocol-level values derived
 * directly from the NMRA specifications.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_DEFINES__
#define __DCC_DEFINES__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// Bit Timing (S-9.1)
// =============================================================================

    /** @brief One-bit half period in microseconds (nominal) */
#define DCC_ONE_BIT_HALF_PERIOD_US          58

    /** @brief One-bit half period minimum (command station transmit) */
#define DCC_ONE_BIT_HALF_PERIOD_MIN_US      55

    /** @brief One-bit half period maximum (command station transmit) */
#define DCC_ONE_BIT_HALF_PERIOD_MAX_US      61

    /** @brief Zero-bit half period in microseconds: the S-9.1 nominal (95 us
     *  minimum). The shared-timer encoder sends two 58 us ticks = 116 us; this
     *  value is used as a decoder stimulus, not as the encoder period. */
#define DCC_ZERO_BIT_HALF_PERIOD_US         100

    /** @brief Zero-bit half period minimum for decoder acceptance */
#define DCC_ZERO_BIT_HALF_PERIOD_DECODER_MIN_US  90

    /** @brief Zero-bit maximum total duration in microseconds */
#define DCC_ZERO_BIT_MAX_TOTAL_DURATION_US  12000

// =============================================================================
// Preamble (S-9.2)
// =============================================================================

    /** @brief Preamble bits for operations mode, sourced from the required
     *  USER_DEFINED_DCC_PREAMBLE_BITS_OPS (set per project in dcc_user_config.h,
     *  validated in dcc_types.h). RailCom builds require >= 16 so the cutout -- which
     *  can blank up to the 5th preamble bit (S-9.3.2 sec 2.4) -- still leaves >= 11
     *  driven bits for the decoder (10 required); non-RailCom requires >= 14 (S-9.2).
     *  Only ever expanded inside command-station code. */
#define DCC_PREAMBLE_BITS_OPS               USER_DEFINED_DCC_PREAMBLE_BITS_OPS

// =============================================================================
// One-shot packet repeat defaults (repeat_count as set by the packet builders)
//
// repeat_count is how many times the scheduler sends a ONE-SHOT packet: it is
// decremented after each send and the slot is dropped at 0, so a packet handed
// over with 0 is NEVER transmitted. Auto-refresh slots ignore it. Every builder
// therefore sets a spec-correct default; an application may still override the
// field after the builder returns.
// =============================================================================

    /** @brief WRITE BYTE / WRITE BIT: "two identical packets are needed before the
     *  decoder shall modify a configuration variable" -- loco POM (S-9.2.1 sec
     *  2.3.7.3) and accessory decoders, which use "the same method" (sec 2.4.3). */
#define DCC_REPEAT_CV_WRITE                 2

    /** @brief VERIFY BYTE / VERIFY BIT act on the first packet received (S-9.2.1 sec 2.3.7.3). */
#define DCC_REPEAT_CV_VERIFY                1

    /** @brief Model date "is transmitted (at least three times) only if changed" (S-9.2.1 sec 2.3.6.2). */
#define DCC_REPEAT_MODEL_DATE               3

    /** @brief Model time "at most once every (model) minute" (S-9.2.1 sec 2.3.6.2) and
     *  system time (sec 2.3.6.3): one send per update. */
#define DCC_REPEAT_TIME                     1

    /** @brief Accessory NOP (RailCom poll) builder: one send. Maintainer policy; the spec gives no count. */
#define DCC_REPEAT_ACCESSORY_NOP            1

    /** @brief Accessory basic/extended STOP builders: one send. Maintainer policy; the spec gives no count. */
#define DCC_REPEAT_ACCESSORY_STOP           1

    /** @brief Every one-shot the spec leaves uncounted -- speed, function groups,
     *  F13-F68, accessory ON/OFF, consist, binary state, analog, reset, idle.
     *  S-9.2 sec C only asks that packets be "repeated as frequently as possible",
     *  so this is a maintainer policy: 2 = one repeat to survive a single lost packet. */
#define DCC_REPEAT_ONE_SHOT_DEFAULT         2

// =============================================================================
// Auto-refresh pacing (command station scheduler, issue #5)
//
// A refresh slot is sent at full rate for DCC_REFRESH_PROMPT_SENDS turns after
// each insert, then drops to a keep-alive: once every DCC_REFRESH_COLD_CYCLES
// packet cycles, and never less often than every DCC_REFRESH_COLD_MAX_CYCLES.
// Counts are packet cycles (one packet on the track each, about 6-7 ms for a
// speed packet; longer packets make a cycle longer). Each can be overridden in
// dcc_user_config.h.
//
// The ceiling is what a decoder's packet time-out (CV11, S-9.2.4 sec 4) has to
// cover for an idle locomotive: at ~6.7 ms per cycle the default is ~0.8 s,
// plus one packet per other slot that falls overdue on the same cycle. Keep
// CV11 on such decoders at 0 (off) or at least DCC_REFRESH_CV11_FLOOR.
// =============================================================================

    /** @brief Full-rate sends after each insert, before the slot goes cold. Like
     *  DCC_REPEAT_ONE_SHOT_DEFAULT, more than one so a single lost packet does not
     *  lose the change. */
#ifndef DCC_REFRESH_PROMPT_SENDS
#define DCC_REFRESH_PROMPT_SENDS            3
#endif

    /** @brief Keep-alive interval of a cold slot, in packet cycles (~0.4 s).
     *  0 disables the cold tier: every refresh slot is sent in turn, as a flat ring. */
#ifndef DCC_REFRESH_COLD_CYCLES
#define DCC_REFRESH_COLD_CYCLES             60
#endif

    /** @brief Longest any refresh slot may go unsent, in packet cycles (~0.8 s).
     *  An overdue slot is sent ahead of changed ones, so a stream of throttle
     *  changes cannot hold a locomotive off the track. Holds while there are no
     *  more active refresh slots than this: beyond that some are always overdue,
     *  overdue and changed slots then take turns, and the overdue ones share
     *  their half of the cycles. */
#ifndef DCC_REFRESH_COLD_MAX_CYCLES
#define DCC_REFRESH_COLD_MAX_CYCLES         120
#endif

    /** @brief The smallest packet time-out (CV11, S-9.2.4 sec 4, in this library's
     *  DCC_FAILSAFE_CV11_UNIT_US units) a decoder on this command station's track can be
     *  set to without an idle locomotive stopping: 20 = 2.0 s. Worst case, every one of
     *  DCC_REFRESH_COLD_MAX_CYCLES cycles carries the longest packet (all zero bits) plus
     *  a RailCom cutout, about 15 ms each: 120 x 15 ms = 1.83 s. The scheduler test
     *  cold_max_cycles_is_below_cv11_minimum checks it; lower this with the ceiling. */
#ifndef DCC_REFRESH_CV11_FLOOR
#define DCC_REFRESH_CV11_FLOOR              20
#endif

#if DCC_REFRESH_COLD_CYCLES > 0 && DCC_REFRESH_COLD_MAX_CYCLES < DCC_REFRESH_COLD_CYCLES
#error "DCC_REFRESH_COLD_MAX_CYCLES must be >= DCC_REFRESH_COLD_CYCLES"
#endif
#if DCC_REFRESH_COLD_CYCLES > 0 && DCC_REFRESH_PROMPT_SENDS < 1
#error "DCC_REFRESH_PROMPT_SENDS must be >= 1 while the cold tier is on, or a change waits a whole keep-alive interval"
#endif
#if DCC_REFRESH_COLD_MAX_CYCLES > 65535 || DCC_REFRESH_PROMPT_SENDS > 255
#error "DCC_REFRESH_COLD_MAX_CYCLES must fit 16 bits and DCC_REFRESH_PROMPT_SENDS 8 bits"
#endif

    /** @brief Minimum preamble bits for service mode */
#define DCC_PREAMBLE_BITS_SERVICE           20

    /** @brief Minimum preamble bits a decoder must accept */
#define DCC_PREAMBLE_BITS_DECODER_MIN       10

// =============================================================================
// Address Ranges (S-9.2)
// =============================================================================

    /** @brief Broadcast address */
#define DCC_ADDRESS_BROADCAST_VALUE         0

    /** @brief Maximum short address */
#define DCC_ADDRESS_SHORT_MAX               127

    /** @brief Minimum long address */
#define DCC_ADDRESS_LONG_MIN                128

    /** @brief Maximum long address */
#define DCC_ADDRESS_LONG_MAX                10239

    /** @brief Idle packet address byte */
#define DCC_ADDRESS_IDLE_VALUE              255

// =============================================================================
// Instruction Byte Masks (S-9.2)
// =============================================================================

    /** @brief Decoder control instruction prefix (0000xxxx) */
#define DCC_INST_DECODER_CONTROL            0x00

    /** @brief Advanced operations instruction prefix (001xxxxx) */
#define DCC_INST_ADVANCED_OPS               0x20

    /** @brief Speed/direction (reverse) instruction prefix (010xxxxx) */
#define DCC_INST_SPEED_REVERSE              0x40

    /** @brief Speed/direction (forward) instruction prefix (011xxxxx) */
#define DCC_INST_SPEED_FORWARD              0x60

    /** @brief Function group 1 instruction prefix (100xxxxx) */
#define DCC_INST_FUNC_GROUP_1               0x80

    /** @brief Function group 2a instruction prefix (1011xxxx) -- F5-F8 */
#define DCC_INST_FUNC_GROUP_2A              0xB0

    /** @brief Function group 2b instruction prefix (1010xxxx) -- F9-F12 */
#define DCC_INST_FUNC_GROUP_2B              0xA0

    /** @brief Feature expansion instruction prefix (110xxxxx) */
#define DCC_INST_FEATURE_EXPANSION          0xC0

    /** @brief CV access (ops-mode) long form instruction prefix (111xxxxx) */
#define DCC_INST_CV_ACCESS_LONG             0xE0

// =============================================================================
// Advanced Operations Sub-Instructions (S-9.2.1)
// =============================================================================

    /** @brief 128-step speed control (full instruction byte: 001 11111) */
#define DCC_ADV_OPS_128_SPEED               0x3F

    /** @brief Analog function control (full instruction byte: 001 11101) */
#define DCC_ADV_OPS_ANALOG_FUNCTION         0x3D

// =============================================================================
// Feature Expansion Sub-Instructions (S-9.2.1)
// =============================================================================

    /** @brief Binary state control short form (full instruction byte: 110 11101) */
#define DCC_FEAT_BINARY_STATE_SHORT         0xDD

    /**
     * @brief Binary state control long form (full instruction byte: 110 00000).
     * @note S-9.2.1 §2.3.6.1: feature-expansion sub-code GGGGG = 00000, i.e.
     *       11000000 = 0xC0 — a distinct opcode from F61-F68 (0xDC). The two do
     *       NOT share a code; they are told apart by their opcode, not byte count.
     */
#define DCC_FEAT_BINARY_STATE_LONG          0xC0

    /** @brief F13-F20 expansion */
#define DCC_FEAT_F13_F20                    0xDE

    /** @brief F21-F28 expansion */
#define DCC_FEAT_F21_F28                    0xDF

    /** @brief F29-F36 expansion */
#define DCC_FEAT_F29_F36                    0xD8

    /** @brief F37-F44 expansion */
#define DCC_FEAT_F37_F44                    0xD9

    /** @brief F45-F52 expansion */
#define DCC_FEAT_F45_F52                    0xDA

    /** @brief F53-F60 expansion */
#define DCC_FEAT_F53_F60                    0xDB

    /** @brief F61-F68 expansion */
#define DCC_FEAT_F61_F68                    0xDC

    /** @brief Time/date command */
#define DCC_FEAT_TIME_DATE                  0xC1

    /** @brief System time command (full instruction byte: 110 00010) */
#define DCC_FEAT_SYSTEM_TIME                0xC2

// =============================================================================
// CV Access Long Form Masks (S-9.2.1)
// =============================================================================

    /** @brief CV write command (long form) */
#define DCC_CV_LONG_WRITE                   0xEC

    /** @brief CV verify command (long form) */
#define DCC_CV_LONG_VERIFY                  0xE4

    /** @brief CV bit manipulation command (long form) */
#define DCC_CV_LONG_BIT                     0xE8

// =============================================================================
// Accessory Address Masks (S-9.2.1)
// =============================================================================

    /** @brief Basic accessory decoder address byte 1 prefix (10xxxxxx) */
#define DCC_ACCESSORY_BASIC_PREFIX          0x80

    /** @brief Extended accessory decoder address byte 2 format */
#define DCC_ACCESSORY_EXTENDED_PREFIX       0x01

    /** @brief Accessory broadcast address byte */
#define DCC_ACCESSORY_BROADCAST             0xBF

// =============================================================================
// Consist Control (S-9.2.1)
// =============================================================================

    /** @brief Set consist address (normal direction) */
#define DCC_CONSIST_SET_NORMAL              0x12

    /** @brief Set consist address (reversed direction) */
#define DCC_CONSIST_SET_REVERSED            0x13

    /** @brief Clear consist address */
#define DCC_CONSIST_CLEAR                   0x10

// =============================================================================
// Key CV Numbers (1-based, matching NMRA convention)
// =============================================================================

#define DCC_CV_PRIMARY_ADDRESS              1   /**< Primary Address: 7-bit short address (1-127) */
#define DCC_CV_VSTART                       2   /**< Vstart: motor voltage at speed step 1 */
#define DCC_CV_ACCELERATION_RATE            3   /**< Acceleration Rate: momentum when speeding up */
#define DCC_CV_DECELERATION_RATE            4   /**< Deceleration Rate: momentum when slowing down */
#define DCC_CV_VHIGH                        5   /**< Vhigh: motor voltage at the top speed step */
#define DCC_CV_VMID                         6   /**< Vmid: motor voltage at the middle speed step */
#define DCC_CV_MANUFACTURER_VERSION         7   /**< Manufacturer Version No.: read-only firmware version */
#define DCC_CV_MANUFACTURER_ID              8   /**< Manufacturer ID: read-only; write 8 for factory reset */
#define DCC_CV_TOTAL_PWM_PERIOD             9   /**< Total PWM Period: motor drive frequency */
#define DCC_CV_EMF_FEEDBACK_CUTOUT          10  /**< EMF Feedback Cutout: speed step at which back-EMF control stops */
#define DCC_CV_PACKET_TIMEOUT               11  /**< Packet Time-Out Value: fail-safe timeout, 0 = disabled (S-9.2.4) */
#define DCC_CV_POWER_SOURCE_CONVERSION      12  /**< Power Source Conversion: alternate power sources the decoder accepts */
#define DCC_CV_ANALOG_MODE_FUNC_FL_F8       13  /**< Alternate Mode Function Status F1-F8: functions active in analog mode */
#define DCC_CV_ANALOG_MODE_FUNC_F9_F12      14  /**< Alternate Mode Function Status FL, F9-F12: functions active in analog mode */
#define DCC_CV_DECODER_LOCK_1               15  /**< Decoder Lock: key written by the command station */
#define DCC_CV_DECODER_LOCK_2               16  /**< Decoder Lock: this decoder's lock ID; writes are refused while CV15 != CV16 */
#define DCC_CV_EXTENDED_ADDRESS_HIGH        17  /**< Extended Address high byte (0xC0-0xE7 on the wire) */
#define DCC_CV_EXTENDED_ADDRESS_LOW         18  /**< Extended Address low byte */
#define DCC_CV_CONSIST_ADDRESS              19  /**< Consist Address: bits 0-6 address, bit 7 = consist direction reversed; 0 = not in a consist */
#define DCC_CV_CONSIST_FUNCTIONS_F1_F8      21  /**< Consist address active for F1-F8: bit 0 = F1 .. bit 7 = F8 */
#define DCC_CV_CONSIST_FUNCTIONS_FL_F9_F12  22  /**< Consist address active for FL (bits 0-1) and F9-F12 (bits 2-5) */
#define DCC_CV22_FL_FORWARD_BIT             0x01  /**< CV22 bit 0: FL answers the consist address when travelling forward */
#define DCC_CV22_FL_REVERSE_BIT             0x02  /**< CV22 bit 1: FL answers the consist address when travelling in reverse */
#define DCC_CV22_F9_BIT                     0x04  /**< CV22 bit 2: F9; bits 3-5 are F10-F12 */
#define DCC_CV_RAILCOM_CONFIG               28  /**< RailCom configuration */
#define DCC_CV_CONFIG                       29  /**< Primary config register */
#define DCC_CV_INDEX_HIGH                   31  /**< Indexed CV page pointer high */
#define DCC_CV_INDEX_LOW                    32  /**< Indexed CV page pointer low */
#define DCC_CV_INDEXED_FIRST               257  /**< First CV of the indexed access window */
#define DCC_CV_INDEXED_LAST                512  /**< Last CV of the indexed access window */
#define DCC_CV_FUNC_MAP_START               33  /**< Function mapping start (F0) */
#define DCC_CV_FUNC_MAP_END                 46  /**< Function mapping end */
#define DCC_CV_SPEED_TABLE_START            67  /**< Speed table start */
#define DCC_CV_SPEED_TABLE_END              94  /**< Speed table end (28 entries) */
#define DCC_CV_USER_ID_1                    105 /**< User Identifier #1: free for the owner's use */
#define DCC_CV_USER_ID_2                    106 /**< User Identifier #2: free for the owner's use */

// =============================================================================
// Accessory Decoder CVs (S-9.2.2 Table 3: CV 513-1024)
// =============================================================================

#define DCC_CV_ACC_ADDRESS_LSB              513 /**< Accessory address LSB (6 bits) */
#define DCC_CV_ACC_ADDRESS_MSB              521 /**< Accessory address MSB (3 bits) */
#define DCC_CV_ACC_CONFIG                   541 /**< Accessory decoder configuration */

// =============================================================================
// CV 541 Bit Masks (Accessory Decoder Configuration)
// =============================================================================

#define DCC_CV541_BASIC_EXTENDED_BIT        0x20  /**< Bit 5: 0=basic, 1=extended */
#define DCC_CV541_ADDRESS_METHOD_BIT        0x40  /**< Bit 6: 0=decoder addr, 1=output addr */
#define DCC_CV541_ACCESSORY_DECODER_BIT     0x80  /**< Bit 7: 1=accessory decoder */

// =============================================================================
// CV 29 Bit Masks
// =============================================================================

#define DCC_CV29_DIRECTION_BIT              0x01  /**< Bit 0: direction reversed */
#define DCC_CV29_SPEED_STEPS_BIT            0x02  /**< Bit 1: 28/128 steps (vs 14) */
#define DCC_CV29_ANALOG_ENABLE_BIT          0x04  /**< Bit 2: analog mode enabled */
#define DCC_CV29_RAILCOM_ENABLE_BIT         0x08  /**< Bit 3: RailCom enabled */
#define DCC_CV29_SPEED_TABLE_BIT            0x10  /**< Bit 4: speed table enabled */
#define DCC_CV29_EXTENDED_ADDRESS_BIT       0x20  /**< Bit 5: extended address */
#define DCC_CV29_RESERVED_BIT               0x40  /**< Bit 6: reserved (forced to 0 on write) */
#define DCC_CV29_ACCESSORY_BIT              0x80  /**< Bit 7: accessory (vs multifunction) decoder */

// =============================================================================
// CV 28 Bit Masks (RailCom Configuration)
// =============================================================================

#define DCC_CV28_CH1_ENABLE_BIT             0x01  /**< Bit 0: Ch1 enabled */
#define DCC_CV28_CH2_ENABLE_BIT             0x02  /**< Bit 1: Ch2 enabled */
#define DCC_CV28_UNSOLICITED_BIT            0x04  /**< Bit 2: unsolicited expansion */

// =============================================================================
// RailCom Constants (S-9.3.2)
// =============================================================================

    /** @brief Cutout state DELAY duration: tristate H-bridge at T_CS = 26us
     *  after the packet end bit (microseconds). */
#define DCC_RAILCOM_CUTOUT_START_DELAY_US   26

    /** @brief Cutout state SETTLING duration: enable UART Rx at T_TS1 = 80us
     *  (cumulative; 26 + 54) (microseconds). */
#define DCC_RAILCOM_UART_RX_DELAY_US        54

    /** @brief Cutout state CH1 window duration: disable UART Rx at T_TC1 = 177us
     *  (cumulative; 80 + 97) (microseconds). */
#define DCC_RAILCOM_CH1_WINDOW_US           97

    /** @brief Cutout state GAP duration: re-enable UART Rx at T_TS2 = 193us
     *  (cumulative; 177 + 16) (microseconds). */
#define DCC_RAILCOM_CH1_CH2_GAP_US          16

    /** @brief Cutout state CH2 window duration: disable UART Rx and restore
     *  H-bridge at T_CE = 454us (cumulative; 193 + 261) (microseconds). */
#define DCC_RAILCOM_CH2_WINDOW_US           261

    /** @brief RailCom Channel 1 max bytes */
#define DCC_RAILCOM_CH1_MAX_BYTES           2

    /** @brief RailCom Channel 2 max bytes */
#define DCC_RAILCOM_CH2_MAX_BYTES           6

// =============================================================================
// RailCom Mobile Channel 2 Datagram IDs (2026 draft S-9.3.2, Table 19)
// =============================================================================

    /** @brief POM (programming-on-main) response */
#define DCC_RAILCOM_ID_POM                  0

    /** @brief ADR1 / ID1 — HIGH bits of address */
#define DCC_RAILCOM_ID_ADR1_HIGH            1

    /** @brief ADR2 / ID2 — LOW bits of address */
#define DCC_RAILCOM_ID_ADR2_LOW             2

    /** @brief EXT (extended) datagram */
#define DCC_RAILCOM_ID_EXT                  3

    /** @brief DYN (dynamic data) datagram */
#define DCC_RAILCOM_ID_DYN                  7

    /** @brief XPOM (extended programming-on-main) response; IDs 8..11 echo the command's 2-bit SS sequence number (this one: SS = 00) */
#define DCC_RAILCOM_ID_XPOM_8               8

    /** @brief XPOM response, SS = 01 */
#define DCC_RAILCOM_ID_XPOM_9               9

    /** @brief XPOM response, SS = 10 */
#define DCC_RAILCOM_ID_XPOM_10              10

    /** @brief XPOM response, SS = 11 */
#define DCC_RAILCOM_ID_XPOM_11              11

    /** @brief CV auto-transfer datagram */
#define DCC_RAILCOM_ID_CV_AUTO              12

    /** @brief Time datagram */
#define DCC_RAILCOM_ID_TIME                 14

    /** @brief Logon Enable datagram */
#define DCC_RAILCOM_ID_LOGON_ENABLE         15

// =============================================================================
// RailCom Accessory Channel 2 Datagram IDs (2026 draft S-9.3.2, Section 7)
// Accessory ID space -- distinct from the mobile DCC_RAILCOM_ID_* set above.
// SRQ is a 12-bit Channel-1 datagram with NO identifier (Section 7.1 / Table 36).
// =============================================================================

    /** @brief All-pairs status (STAT4) */
#define DCC_RAILCOM_ACC_ID_STATUS_4         3

    /** @brief Single-pair status (STAT1) */
#define DCC_RAILCOM_ACC_ID_STATUS           4

    /** @brief Time report */
#define DCC_RAILCOM_ACC_ID_TIME             5

    /** @brief Error report */
#define DCC_RAILCOM_ACC_ID_ERROR            6

// =============================================================================
// RailCom 4/8 Special Code Words (2026 draft S-9.3.2)
// =============================================================================

    /** @brief ACK special code word (raw byte, bypasses 4/8 table) */
#define DCC_RAILCOM_CODE_WORD_ACK           0xF0

    /** @brief NACK special code word (raw byte, bypasses 4/8 table) */
#define DCC_RAILCOM_CODE_WORD_NACK          0x3C

    /** @brief BUSY special code word (raw byte) -- decoder busy, retry later (Table 2) */
#define DCC_RAILCOM_CODE_WORD_BUSY          0xE1

// =============================================================================
// Decoder RailCom Tx timing (S-9.3.2 sec 2.4, from the packet end-bit edge)
// =============================================================================

    /** @brief One UART bit period. 250 kbit/s +-2% -> 4us per bit. */
#define DCC_RAILCOM_TX_BIT_US               4

    /** @brief Blank from the end-bit edge to Channel 1 start (T_TS1 = 80us). The
     *  cutout has already opened by then (T_CS <= 32us), so the line is free. */
#define DCC_RAILCOM_TX_BLANK_US             80

    /** @brief Gap from Channel 1 end to Channel 2 start. Ch1 is 2 bytes x 10 bits
     *  x 4us = 80us, so the gap is T_TS2 (193) - T_TS1 (80) - 80 = 33us. */
#define DCC_RAILCOM_TX_GAP_US               33

// =============================================================================
// Idle / Reset Packet Constants
// =============================================================================

    /** @brief Idle packet: address byte */
#define DCC_IDLE_ADDR_BYTE                  0xFF

    /** @brief Idle packet: data byte */
#define DCC_IDLE_DATA_BYTE                  0x00

    /** @brief Idle packet: XOR byte (0xFF ^ 0x00) */
#define DCC_IDLE_XOR_BYTE                   0xFF

    /** @brief Reset packet: all bytes are 0x00 */
#define DCC_RESET_BYTE                      0x00

// =============================================================================
// Speed Constants
// =============================================================================

    /** @brief Emergency stop speed value (128-step mode) */
#define DCC_SPEED_128_ESTOP                 1

    /** @brief Stop speed value (128-step mode) */
#define DCC_SPEED_128_STOP                  0

// =============================================================================
// Service Mode Constants (S-9.2.3)
// =============================================================================

    /** @brief Service mode direct write byte prefix (0111 11 AA) */
#define DCC_SERVICE_DIRECT_WRITE_PREFIX     0x7C

    /** @brief Service mode direct verify byte prefix (0111 01 AA) */
#define DCC_SERVICE_DIRECT_VERIFY_PREFIX    0x74

    /** @brief Service mode direct bit manipulation prefix (0111 10 AA) */
#define DCC_SERVICE_DIRECT_BIT_PREFIX       0x78

    /** @brief Service mode register write prefix (0111 1 RRR) */
#define DCC_SERVICE_REGISTER_WRITE_PREFIX   0x78

    /** @brief Service mode register verify prefix (0111 0 RRR) */
#define DCC_SERVICE_REGISTER_VERIFY_PREFIX  0x70

    /** @brief Number of reset packets before command packets */
#define DCC_SERVICE_MODE_RESET_PRE_COUNT    3

    /** @brief Number of command packet repetitions */
#define DCC_SERVICE_MODE_COMMAND_REPEAT     5

    /** @brief Number of reset packets after command packets */
#define DCC_SERVICE_MODE_RESET_POST_COUNT   6

    /** @brief Number of recovery packets after command packets (write ops).
     *  Per S-9.2.3: command station sends write or reset packets during
     *  Decoder-Recovery-Time while continuing to scan for ACK. */
#define DCC_SERVICE_MODE_RECOVERY_COUNT     6

    /** @brief Page register number for paged mode */
#define DCC_SERVICE_MODE_PAGE_REGISTER      6

    /** @brief S-9.2.3: register-mode VERIFY requires 7 or more command packets */
#define DCC_SERVICE_MODE_REGISTER_VERIFY_REPEAT 7

    /** @brief S-9.2.3: Decoder-Recovery-Time is 10 packets after a write to
     *  Register 1 (and to CV #1 in Address-only mode). */
#define DCC_SERVICE_MODE_RECOVERY_COUNT_LONG    10

    /** @brief S-9.2.3: page-preset sets the page register to page 1, performed
     *  ahead of register-mode and address-only operations. */
#define DCC_SERVICE_MODE_PAGE_PRESET_PAGE       1

    /** @brief S-9.2.3 line 55: the ACK scan window starts at the END of the 2nd
     *  command packet. We blank ACK sampling during the first this-many command
     *  packets so an early decoder mode-switch transient cannot be latched as an
     *  ACK. (A conformant decoder cannot ACK before 2 packets, so this is lossless.) */
#define DCC_SERVICE_MODE_ACK_BLANK_PACKETS      2

// =============================================================================
// ACK Pulse Timing (S-9.2.3)
// =============================================================================

    /** @brief ACK pulse duration in microseconds (6 ms per NMRA S-9.2.3). */
#define DCC_ACK_PULSE_DURATION_US           6000

// =============================================================================
// Decoder Bit Classification (S-9.1)
// =============================================================================

    /** @brief Threshold between one-half and zero-half (microseconds).
     *  Below this = one-bit half, at or above = zero-bit half. */
#define DCC_DECODER_HALF_BIT_THRESHOLD_US   80

    /** @brief Maximum valid half-bit period (microseconds). Beyond = invalid. */
#define DCC_DECODER_HALF_BIT_MAX_US         10000

// =============================================================================
// RailCom 4/8 Decode Constants
// =============================================================================

    /** @brief Invalid RailCom 4/8 decode result */
#define DCC_RAILCOM_DECODE_INVALID          0xFF

    /** @brief RailCom ACK codeword decode result */
#define DCC_RAILCOM_DECODE_ACK              0xFE

    /** @brief RailCom NACK codeword decode result */
#define DCC_RAILCOM_DECODE_NACK             0xFD

// =============================================================================
// Packet-Timeout Fail-Safe (S-9.2.4 §4)
// =============================================================================

    /** @brief Real-time value of one CV11 (DCC_CV_PACKET_TIMEOUT) least-
     *  significant bit, in microseconds.
     *
     *  S-9.2.4 §4 deliberately leaves the CV11 unit manufacturer-defined; it
     *  only constrains the configurable maximum (TIMEOUT_MAX) to be at least
     *  20 seconds. This library maps CV11 to 0.1 s per LSB, giving a 0.1 s ..
     *  25.5 s range that satisfies that floor (CV11 = 200 -> 20.0 s). Redefine
     *  this single constant to choose a different unit. */
#define DCC_FAILSAFE_CV11_UNIT_US           100000u

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DCC_DEFINES__ */
