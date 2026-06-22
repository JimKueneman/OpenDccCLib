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
 * @file dcc_packet_encoder.h
 * @brief Builds DCC packet byte arrays from high-level commands.
 *
 * @details Pure computational module — no hardware dependencies, no state.
 * Takes command parameters and fills a dcc_packet_t with the correct byte
 * layout and XOR error detection byte per NMRA S-9.2.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#ifndef __DCC_PACKET_ENCODER__
#define __DCC_PACKET_ENCODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// Idle / Reset / Emergency Stop
// =============================================================================

    /** @brief Build an idle packet (0xFF 0x00 0xFF).
     *   packet Pointer to  dcc_packet_t struct to fill. */
extern void DccPacketEncoder_idle(dcc_packet_t *packet);

    /** @brief Build a reset packet (0x00 0x00 0x00).
     *   packet Pointer to  dcc_packet_t struct to fill. */
extern void DccPacketEncoder_reset(dcc_packet_t *packet);

    /** @brief Build a broadcast emergency stop packet.
     *   packet Pointer to  dcc_packet_t struct to fill. */
extern void DccPacketEncoder_estop_all(dcc_packet_t *packet);

// =============================================================================
// Speed Commands
// =============================================================================

    /**
     * @brief Build a 128-step speed-and-direction packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param speed Speed value (0=stop, 1=e-stop, 2-127=speed steps 1-126).
     * @param direction true=forward, false=reverse.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_speed_128(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction);

    /**
     * @brief Build a 28-step speed-and-direction packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param speed Speed value (0=stop, 1=e-stop, 2-29=speed steps 1-28).
     * @param direction true=forward, false=reverse.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_speed_28(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction);

    /**
     * @brief Build a 14-step speed-and-direction packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param speed Speed value (0=stop, 1=e-stop, 2-15=speed steps 1-14).
     * @param direction true=forward, false=reverse.
     * @param headlight FL headlight state (true=on).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_speed_14(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t speed, bool direction, bool headlight);

// =============================================================================
// Function Commands
// =============================================================================

    /**
     * @brief Build a Function Group 1 packet (FL, F1-F4).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit4=FL, bit3=F4, bit2=F3, bit1=F2, bit0=F1.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_group_1(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function Group 2a packet (F5-F8).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit3=F8, bit2=F7, bit1=F6, bit0=F5.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_group_2a(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function Group 2b packet (F9-F12).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit3=F12, bit2=F11, bit1=F10, bit0=F9.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_group_2b(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F13-F20 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F20 .. bit0=F13.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f13_f20(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F21-F28 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F28 .. bit0=F21.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f21_f28(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F29-F36 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F36 .. bit0=F29.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f29_f36(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F37-F44 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F44 .. bit0=F37.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f37_f44(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F45-F52 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F52 .. bit0=F45.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f45_f52(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F53-F60 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F60 .. bit0=F53.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f53_f60(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

    /**
     * @brief Build a Function F61-F68 expansion packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param func_bits Bitmask: bit7=F68 .. bit0=F61.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_func_f61_f68(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t func_bits);

// =============================================================================
// Accessory Commands
// =============================================================================

    /**
     * @brief Build a basic accessory decoder packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output selection (0-7).
     * @param activate true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_basic(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, bool activate);

    /**
     * @brief Build an extended accessory decoder (signal aspect) packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param aspect Signal aspect value (0-255).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_extended(dcc_packet_t *packet, uint16_t address, uint8_t aspect);

// =============================================================================
// Accessory CV Programming (Ops-Mode)
// =============================================================================

    /**
     * @brief Build a basic accessory ops-mode CV write packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_basic_cv_write(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a basic accessory ops-mode CV verify packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_basic_cv_verify(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build a basic accessory ops-mode CV bit manipulation packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param board_address 9-bit board address (0-511).
     * @param output_pair Output pair (0-3) for sub-address within board.
     * @param cv_number CV number (1-1024, 1-based).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_basic_cv_bit(dcc_packet_t *packet, uint16_t board_address, uint8_t output_pair, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

    /**
     * @brief Build an extended accessory ops-mode CV write packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_extended_cv_write(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an extended accessory ops-mode CV verify packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_extended_cv_verify(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an extended accessory ops-mode CV bit manipulation packet.
     *  packet Pointer to dcc_packet_t struct to fill.
     * @param address 11-bit address (0-2047).
     * @param cv_number CV number (1-1024, 1-based).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_accessory_extended_cv_bit(dcc_packet_t *packet, uint16_t address, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

// =============================================================================
// CV Programming (Ops-Mode)
// =============================================================================

    /**
     * @brief Build an ops-mode CV write packet (long form).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address of the decoder.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param value Byte value to write.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_cv_write_ops(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an ops-mode CV verify packet (long form).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address of the decoder.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param value Expected byte value to verify.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_cv_verify_ops(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t value);

    /**
     * @brief Build an ops-mode CV bit manipulation packet (long form).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address of the decoder.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param cv_number CV number (1-1024, 1-based per NMRA convention).
     * @param bit_position Bit position within the CV byte (0-7).
     * @param bit_value Desired bit value (true=1, false=0).
     * @param write true=write the bit, false=verify the bit.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_cv_bit_ops(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t cv_number, uint8_t bit_position, bool bit_value, bool write);

// =============================================================================
// Consist Control
// =============================================================================

    /**
     * @brief Build a consist address set packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address of the locomotive.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param consist_address 7-bit consist address (1-127).
     * @param direction_normal true=normal direction, false=reversed.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_consist_set(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t consist_address, bool direction_normal);

    /**
     * @brief Build a consist address clear packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address of the locomotive.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_consist_clear(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type);

// =============================================================================
// Binary State / Analog Function / Speed Restriction
// =============================================================================

    /**
     * @brief Build a binary state control short form packet (states 0-127).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param state_number State number (0-127).
     * @param active true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_binary_state_short(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t state_number, bool active);

    /**
     * @brief Build a binary state control long form packet (states 0-32767).
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param state_number State number (0-32767).
     * @param active true=activate, false=deactivate.
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_binary_state_long(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint16_t state_number, bool active);

    /**
     * @brief Build an analog function control packet.
     *  packet Pointer to  dcc_packet_t struct to fill.
     * @param address DCC address.
     *  address_type Short or long address ( dcc_address_type_enum).
     * @param output_number Analog output number (0-255).
     * @param value Analog output value (0-255).
     * @return true if packet was built successfully, false if invalid parameters.
     */
extern bool DccPacketEncoder_analog_function(dcc_packet_t *packet, dcc_address_t address, dcc_address_type_enum address_type, uint8_t output_number, uint8_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_PACKET_ENCODER__ */
