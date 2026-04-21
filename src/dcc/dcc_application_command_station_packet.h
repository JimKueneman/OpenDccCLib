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
 * @file dcc_application_command_station_packet.h
 * @brief Application-layer API for building DCC command station packets.
 *
 * @details Provides functions that load a @ref dcc_packet_t with the correct
 * byte layout and XOR error detection byte per NMRA S-9.2. Pure computational
 * module with no hardware dependencies and no internal state. The scheduler
 * calls these via an interface struct to build packets on demand.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#ifndef __DCC_APPLICATION_COMMAND_STATION_PACKET__
#define __DCC_APPLICATION_COMMAND_STATION_PACKET__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// Idle / Reset / Emergency Stop
// =============================================================================

    /**
     * @brief Build an idle packet (0xFF 0x00 0xFF).
     *
     * @details Fills the packet with the standard DCC idle byte sequence.
     *     The scheduler sends idle packets when no other traffic is pending
     *     to keep the DCC signal alive on the track.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     */
extern void DccApplicationCommandStationPacket_load_idle(dcc_packet_t *packet);

    /**
     * @brief Build a reset packet (0x00 0x00 0x00).
     *
     * @details Fills the packet with the standard DCC reset byte sequence.
     *     A reset packet commands all decoders on the track to enter their
     *     power-on default state.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     */
extern void DccApplicationCommandStationPacket_load_reset(dcc_packet_t *packet);

    /**
     * @brief Build a broadcast emergency stop packet.
     *
     * @details Sends a 128-step speed command with e-stop value to the
     *     broadcast address, causing all decoders to immediately halt
     *     motor output.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     */
extern void DccApplicationCommandStationPacket_load_estop_all(dcc_packet_t *packet);

// =============================================================================
// Speed Commands
// =============================================================================

    /**
     * @brief Build a 128-step speed-and-direction packet.
     *
     * @details Encodes a two-byte advanced operations speed instruction per
     *     NMRA S-9.2.1. Speed value 0 is stop, 1 is emergency stop, and
     *     2-127 map to speed steps 1-126.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param speed Speed value (0=stop, 1=e-stop, 2-127=speed steps 1-126).
     * @param direction true=forward, false=reverse.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_speed_128(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction);

    /**
     * @brief Build a 28-step speed-and-direction packet.
     *
     * @details Encodes a baseline speed instruction with interleaved speed
     *     bit mapping per NMRA S-9.2 Figure 2. Speed value 0 is stop, 1 is
     *     emergency stop, and 2-29 map to speed steps 1-28.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param speed Speed value (0=stop, 1=e-stop, 2-29=speed steps 1-28).
     * @param direction true=forward, false=reverse.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_speed_28(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction);

    /**
     * @brief Build a 14-step speed-and-direction packet.
     *
     * @details Encodes a baseline speed instruction where the FL headlight
     *     bit shares the instruction byte. Speed value 0 is stop, 1 is
     *     emergency stop, and 2-15 map to speed steps 1-14.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param speed Speed value (0=stop, 1=e-stop, 2-15=speed steps 1-14).
     * @param direction true=forward, false=reverse.
     * @param headlight FL headlight state (true=on).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_speed_14(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction, bool headlight);

// =============================================================================
// Function Commands
// =============================================================================

    /**
     * @brief Build a Function Group 1 packet (FL, F1-F4).
     *
     * @details Encodes the function group one instruction with a 5-bit
     *     bitmask controlling the headlight and first four functions.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit4=FL, bit3=F4, bit2=F3, bit1=F2, bit0=F1.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_group_1(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function Group 2a packet (F5-F8).
     *
     * @details Encodes the function group two upper instruction with a
     *     4-bit bitmask controlling functions F5 through F8.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit3=F8, bit2=F7, bit1=F6, bit0=F5.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_group_2a(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function Group 2b packet (F9-F12).
     *
     * @details Encodes the function group two lower instruction with a
     *     4-bit bitmask controlling functions F9 through F12.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit3=F12, bit2=F11, bit1=F10, bit0=F9.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_group_2b(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F13-F20 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F13 through F20.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F20 .. bit0=F13.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f13_f20(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F21-F28 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F21 through F28.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F28 .. bit0=F21.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f21_f28(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F29-F36 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F29 through F36.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F36 .. bit0=F29.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f29_f36(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F37-F44 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F37 through F44.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F44 .. bit0=F37.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f37_f44(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F45-F52 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F45 through F52.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F52 .. bit0=F45.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f45_f52(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F53-F60 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F53 through F60.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F60 .. bit0=F53.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f53_f60(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F61-F68 expansion packet.
     *
     * @details Encodes a feature expansion instruction with an 8-bit
     *     bitmask controlling functions F61 through F68.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param func_bits Bitmask: bit7=F68 .. bit0=F61.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_func_f61_f68(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

// =============================================================================
// Accessory Commands
// =============================================================================

    /**
     * @brief Build a basic accessory decoder packet.
     *
     * @details Encodes the two-byte basic accessory instruction that controls
     *     turnouts and similar on/off outputs. The board address and output
     *     pair select the specific output on the decoder.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output selection (0-7).
     * @param activate true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_basic(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, bool activate);

    /**
     * @brief Build an extended accessory decoder (signal aspect) packet.
     *
     * @details Encodes the three-byte extended accessory instruction used
     *     for signal decoders. The aspect value selects the signal indication
     *     displayed by the addressed decoder.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param aspect Signal aspect value (0-255).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_extended(dcc_packet_t *packet, uint16_t address, uint8_t aspect);

    /**
     * @brief Build a basic accessory stop (deactivate) packet.
     *
     * @details Encodes a basic accessory instruction with activate=0 for the
     *     given board address and output pair. Functionally identical to calling
     *     load_accessory_basic with activate=false, but provided as a distinct
     *     entry point so the scheduler can distinguish stop-for-SRQ-collection
     *     from a normal deactivate command.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output selection (0-7).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_basic_stop(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair);

    /**
     * @brief Build an extended accessory stop (aspect 0) packet.
     *
     * @details Encodes an extended accessory instruction with aspect=0 (all
     *     stop) per S-9.3.2 Table 36. Provided as a distinct entry point so
     *     the scheduler can distinguish stop-for-SRQ-collection from a normal
     *     aspect-0 command.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_extended_stop(dcc_packet_t *packet, uint16_t address);

// =============================================================================
// Accessory CV Programming (Ops-Mode)
// =============================================================================

    /**
     * @brief Build a basic accessory ops-mode CV write packet.
     *
     * @details Programs a CV value in a basic accessory decoder while it
     *     remains on the main track. The output pair selects the sub-address
     *     within the board.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_basic_cv_write(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a basic accessory ops-mode CV verify packet.
     *
     * @details Verifies a CV value in a basic accessory decoder on the main
     *     track. The decoder acknowledges via a current pulse if the value
     *     matches.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_basic_cv_verify(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a basic accessory ops-mode CV bit manipulation packet.
     *
     * @details Writes or verifies a single bit within a CV of a basic
     *     accessory decoder on the main track. Useful for toggling individual
     *     configuration flags without rewriting the entire CV byte.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_basic_cv_bit(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

    /**
     * @brief Build an extended accessory ops-mode CV write packet.
     *
     * @details Programs a CV value in an extended accessory (signal) decoder
     *     while it remains on the main track.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_extended_cv_write(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an extended accessory ops-mode CV verify packet.
     *
     * @details Verifies a CV value in an extended accessory (signal) decoder
     *     on the main track. The decoder acknowledges via a current pulse if
     *     the value matches.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_extended_cv_verify(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an extended accessory ops-mode CV bit manipulation packet.
     *
     * @details Writes or verifies a single bit within a CV of an extended
     *     accessory (signal) decoder on the main track.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_accessory_extended_cv_bit(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

// =============================================================================
// CV Programming (POM — Programming on the Main)
// =============================================================================

    /**
     * @brief Build a POM CV write packet (long form).
     *
     * @details Programs a CV value in a locomotive or multi-function decoder
     *     while it remains on the main track. Uses the long-form CV access
     *     instruction per NMRA S-9.2.1.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_cv_write_pom(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a POM CV verify packet (long form).
     *
     * @details Verifies a CV value in a locomotive or multi-function decoder
     *     on the main track. The decoder acknowledges via RailCom or current
     *     pulse if the value matches.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_cv_verify_pom(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a POM CV bit manipulation packet (long form).
     *
     * @details Writes or verifies a single bit within a CV of a locomotive
     *     or multi-function decoder on the main track. Useful for toggling
     *     individual configuration flags.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_cv_bit_pom(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

// =============================================================================
// Consist Control
// =============================================================================

    /**
     * @brief Build a consist address set packet.
     *
     * @details Assigns a locomotive to a consist by writing the consist
     *     address into the decoder. The direction flag controls whether
     *     the locomotive runs in its normal or reversed orientation within
     *     the consist.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the locomotive.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param consist_address 7-bit consist address (1-127).
     * @param direction_normal true=normal direction, false=reversed.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_consist_set(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t consist_address, bool direction_normal);

    /**
     * @brief Build a consist address clear packet.
     *
     * @details Removes a locomotive from its consist by writing consist
     *     address zero with normal direction.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the locomotive.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_consist_clear(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type);

// =============================================================================
// Binary State / Analog Function / Speed Restriction
// =============================================================================

    /**
     * @brief Build a binary state control short form packet (states 0-127).
     *
     * @details Activates or deactivates a single binary state in the short
     *     form encoding, supporting up to 128 states per decoder.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param state_number State number (0-127).
     * @param active true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_binary_state_short(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t state_number, bool active);

    /**
     * @brief Build a binary state control long form packet (states 0-32767).
     *
     * @details Activates or deactivates a single binary state in the long
     *     form encoding, supporting up to 32768 states per decoder.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param state_number State number (0-32767).
     * @param active true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_binary_state_long(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t state_number, bool active);

    /**
     * @brief Build an analog function control packet.
     *
     * @details Sends an 8-bit analog value to a numbered output on the
     *     decoder. Commonly used for volume, lighting intensity, or other
     *     continuously variable outputs.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param output_number Analog output number (0-255).
     * @param value Analog output value (0-255).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_analog_function(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t output_number, uint8_t value);

    /**
     * @brief Build a speed restriction packet.
     *
     * @details Applies or removes a speed limit on the addressed decoder.
     *     When enabled, the decoder clamps its speed output to the given
     *     limit regardless of speed commands received.
     *
     * @param packet Pointer to a @ref dcc_packet_t struct to fill.
     * @param address DCC address of the target decoder.
     * @param address_type Short or long address per @ref dcc_address_type_enum.
     * @param enabled true=restriction active, false=restriction removed.
     * @param speed_limit Speed limit value (0-127).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccApplicationCommandStationPacket_load_speed_restriction(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, bool enabled, uint8_t speed_limit);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_APPLICATION_COMMAND_STATION_PACKET__ */
