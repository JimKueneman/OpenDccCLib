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
 * @file dcc_packet_decoder.h
 * @brief DCC packet parsing, XOR validation, address matching, and callback
 * dispatch for decoders.
 *
 * @details Receives raw byte arrays from the bit decoder, validates the XOR
 * error detection byte, extracts addresses, matches against the configured
 * decoder address, parses all instruction types, and dispatches to the
 * appropriate application callbacks.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#ifndef __DCC_PACKET_DECODER__
#define __DCC_PACKET_DECODER__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Read a CV value. Routed through cv_storage for decoder lock. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Write a CV value. Routed through cv_storage for decoder lock. */
    bool (*cv_write)(uint16_t cv_number, uint8_t value);

        /** @brief Speed command received. NULL = no notification. */
    void (*on_speed_command)(uint16_t address, uint8_t speed, bool direction, dcc_speed_mode_enum mode);

        /** @brief Emergency stop command received. NULL = no notification. */
    void (*on_emergency_stop_command)(uint16_t address);

        /** @brief Function command received. NULL = no notification. */
    void (*on_function_command)(uint16_t address, uint8_t function_number, bool state);

        /** @brief Basic accessory command received. NULL = no notification.
         *  In decoder-address mode (CV541 bit 6 = 0): board_address is 9-bit
         *  board address, output_pair is the 3-bit DDD field.
         *  In output-address mode (CV541 bit 6 = 1): board_address is the
         *  11-bit flat output address, output_pair is the R bit (0 or 1). */
    void (*on_accessory_basic_command)(uint16_t board_address, uint8_t output_pair, bool activate);

        /** @brief Extended accessory command received. NULL = no notification. */
    void (*on_accessory_extended_command)(uint16_t address, uint8_t aspect);

        /** @brief Accessory CV write (ops-mode) received. NULL = no notification. */
    void (*on_acc_cv_write)(uint16_t cv_number, uint8_t value);

        /** @brief Accessory CV verify (ops-mode) received. NULL = no notification. */
    void (*on_acc_cv_verify)(uint16_t cv_number, uint8_t value);

        /** @brief Accessory CV bit manipulation (ops-mode) received. NULL = no notification. */
    void (*on_acc_cv_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value);

        /** @brief CV write command received. NULL = no notification. */
    void (*on_cv_write_command)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV verify command received. NULL = no notification. */
    void (*on_cv_verify_command)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV bit command received. NULL = no notification. */
    void (*on_cv_bit_command)(uint16_t cv_number, uint8_t bit_position, bool bit_value, bool service_mode);

        /** @brief Consist control received. NULL = no notification. */
    void (*on_consist_command)(uint16_t address, uint8_t consist_address, bool direction_normal);

        /** @brief Binary state short command (1-127) received. NULL = no notification. */
    void (*on_binary_state_short_command)(uint16_t address, uint8_t state_number, bool active);

        /** @brief Binary state long command (1-32767) received. NULL = no notification. */
    void (*on_binary_state_long_command)(uint16_t address, uint16_t state_number, bool active);

        /** @brief Analog function command received. NULL = no notification. */
    void (*on_analog_function_command)(uint16_t address, uint8_t output_number, uint8_t value);

        /** @brief Turn on the ACK current load. Library handles 6ms timing. */
    void (*start_ack_pulse)(void);

        /** @brief A command packet addressed to this decoder was accepted.
         *  NULL = no notification. Fired for multifunction packets whose
         *  address matches ours (or broadcast) in Operations Mode -- used to
         *  re-arm the S-9.2.4 packet-timeout fail-safe. */
    void (*on_addressed_packet)(void);

#if defined(DCC_COMPILE_RAILCOM)
        /** @brief The decoder's resolved address changed (or was first resolved at
         *  init). NULL = no notification. The RailCom Tx engine holds the pushed
         *  address for its ADR datagram instead of re-reading CVs each cutout. */
    void (*on_address_changed)(dcc_address_t address, dcc_address_type_enum type);
#endif /* DCC_COMPILE_RAILCOM */

} interface_dcc_packet_decoder_t;

    /**
     * @brief Initialize the packet decoder module.
     * @param interface Pointer to populated interface struct.
     */
extern void DccPacketDecoder_initialize(const interface_dcc_packet_decoder_t *interface);

    /**
     * @brief Process a complete raw packet from the bit decoder.
     * @param data Raw packet bytes (including XOR byte).
     * @param byte_count Number of bytes in the packet.
     *
     * @details Validates XOR, extracts address, checks address match, parses
     * the instruction, and dispatches to the appropriate callback.
     */
extern void DccPacketDecoder_process_packet(const uint8_t *data, uint8_t byte_count);

    /**
     * @brief Enqueue a raw decoded packet for deferred dispatch.
     *
     * @details Wired to the bit decoder's on_packet_received, so it runs in the end-bit
     * ISR. It only copies the bytes into a FIFO (no dispatch) -- the instruction
     * callbacks fire later from @ref DccPacketDecoder_run, keeping the ISR short and the
     * RailCom cutout window clear. A full FIFO drops the packet (the command station
     * resends).
     *
     * @param data Raw packet bytes (including XOR byte).
     * @param byte_count Number of bytes in the packet.
     */
extern void DccPacketDecoder_enqueue(const uint8_t *data, uint8_t byte_count);

    /**
     * @brief Drain the packet FIFO, dispatching each queued packet.
     *
     * @details Call from the main loop (DccConfig_run). Runs @ref
     * DccPacketDecoder_process_packet for every packet queued since the last call, in
     * the poll context rather than the edge ISR.
     */
extern void DccPacketDecoder_run(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_PACKET_DECODER__ */
