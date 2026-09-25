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
 * @file callbacks_dcc.h
 * @brief DCC decoder callback declarations for this demo.
 *
 * @details CUSTOMIZE THESE for your decoder hardware.
 *
 * On this bench the GPIO edge ISR only captures timestamps; the main loop feeds
 * them to the bit decoder and DccConfig_run() dispatches the decoded packets, so
 * every command callback below runs in main-loop context. They still keep the
 * ring-buffer pattern (no blocking UART output inside a callback) so the same
 * code works on a port that calls the edge ISR directly.
 *
 * This demo just logs each command into a ring buffer (the "RECV" lines
 * you see on the UART).  In a real decoder you would drive motors, LEDs,
 * servos, or solenoids here instead.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __CALLBACKS_DCC__
#define __CALLBACKS_DCC__

#include "dcc_lib/dcc_types.h"

#ifdef DCC_COMPILE_DECODER

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Ring buffer management --- */

    /** @brief Clear the RECV ring buffer and restore the CV storage to factory defaults. */
extern void CallbacksDcc_initialize(void);

    /** @brief Push one pending RECV line to the UART. Call from the main loop, not from an ISR. */
extern void CallbacksDcc_drain(void);

    /** @brief Discard all pending RECV lines. */
extern void CallbacksDcc_clear(void);

    /**
     * @brief CV read hook (wired to dcc_config_t.cv_read).
     *
     * @details This demo stores CVs in a RAM array; replace with Flash or EEPROM for
     * persistent storage (see callbacks_dcc.c).
     *
     * @param cv_number  1-based CV number.
     * @param value      Receives the CV value.
     *
     * @return true on success, false if cv_number is out of range.
     */
extern bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value);
    /**
     * @brief CV write hook (wired to dcc_config_t.cv_write).
     *
     * @param cv_number  1-based CV number.
     * @param value      Value to store.
     *
     * @return true on success, false if cv_number is out of range.
     */
extern bool CallbacksDcc_cv_write(uint16_t cv_number, uint8_t value);

    /**
     * @brief Restore CVs to factory defaults (wired to dcc_config_t.factory_reset).
     *
     * @details Invoked when a write command targets CV8 (read-only Manufacturer ID, S-9.2.2).
     */
extern void CallbacksDcc_factory_reset(void);

    /**
     * @brief Indexed CV read hook (wired to dcc_config_t.cv_read_indexed).
     *
     * @details The library resolves the CV257-512 window via CV31/CV32 and calls this
     * with the page and offset.
     *
     * @param page_hi  CV31 value (page high byte).
     * @param page_lo  CV32 value (page low byte).
     * @param offset   Offset within the 256-byte page.
     * @param value    Receives the value.
     *
     * @return true on success, false for an unsupported page.
     */
extern bool CallbacksDcc_cv_read_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value);
    /**
     * @brief Indexed CV write hook (wired to dcc_config_t.cv_write_indexed).
     *
     * @param page_hi  CV31 value (page high byte).
     * @param page_lo  CV32 value (page low byte).
     * @param offset   Offset within the 256-byte page.
     * @param value    Value to store.
     *
     * @return true on success, false for an unsupported page.
     */
extern bool CallbacksDcc_cv_write_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t value);

    /**
     * @brief CV29 supported-feature application (wired to dcc_config_t.cv29_apply_supported_features).
     *
     * @details The library decodes a CV29 write; the app clears any feature it does
     * not implement (this demo clears analog conversion, the speed table, and
     * RailCom when DCC_COMPILE_RAILCOM is off) and the result is stored.
     *
     * @param flags  Decoded CV29 bits, a @ref dcc_cv29_flags_t; modified in place.
     */
extern void CallbacksDcc_cv29_apply_supported_features(dcc_cv29_flags_t *flags);

    /**
     * @brief Speed and direction command (wired to dcc_config_t.on_speed_command); drive your motor here.
     *
     * @details Each on_xxx callback fires from DccConfig_run() when the library
     * decodes the corresponding DCC packet type. This demo logs a RECV line.
     *
     * @param address    Decoder address the packet was for.
     * @param speed      Speed step in the given mode.
     * @param direction  true = forward.
     * @param mode       Speed-step mode, a @ref dcc_speed_mode_enum.
     */
extern void CallbacksDcc_on_speed_command(uint16_t address, uint8_t speed,
                                           bool direction,
                                           dcc_speed_mode_enum mode);

    /**
     * @brief Emergency stop; cut motor power immediately.
     *
     * @param address  Decoder address the packet was for.
     */
extern void CallbacksDcc_on_emergency_stop(uint16_t address);

    /**
     * @brief Function on/off; control lights, sound triggers, couplers, etc.
     *
     * @param address          Decoder address the packet was for.
     * @param function_number  Function number F0..F68.
     * @param state            true = on.
     */
extern void CallbacksDcc_on_function_command(uint16_t address,
                                              uint8_t function_number,
                                              bool state);

    /**
     * @brief Basic accessory command (turnouts/switches).
     *
     * @param board_address  9-bit accessory board address.
     * @param output_pair    Output pair 0..3.
     * @param activate       true = activate the output.
     */
extern void CallbacksDcc_on_accessory_basic_command(uint16_t board_address,
                                                     uint8_t output_pair,
                                                     bool activate);

    /**
     * @brief Extended accessory command (signal aspects).
     *
     * @param address  11-bit extended accessory address.
     * @param aspect   Aspect value 0..31.
     */
extern void CallbacksDcc_on_accessory_extended_command(uint16_t address,
                                                        uint8_t aspect);

    /**
     * @brief CV write notification; the library has already stored the value.
     *
     * @param cv_number     1-based CV number.
     * @param value         Value written.
     * @param service_mode  true when received on the programming track.
     */
extern void CallbacksDcc_on_cv_write(uint16_t cv_number, uint8_t value,
                                     bool service_mode);
    /**
     * @brief CV verify notification; the library has already answered the verify.
     *
     * @param cv_number     1-based CV number.
     * @param value         Value verified against.
     * @param service_mode  true when received on the programming track.
     */
extern void CallbacksDcc_on_cv_verify(uint16_t cv_number, uint8_t value,
                                      bool service_mode);
    /**
     * @brief CV bit manipulation notification.
     *
     * @param cv_number     1-based CV number.
     * @param bit_position  Bit 0..7.
     * @param bit_value     Bit value written or verified.
     * @param service_mode  true when received on the programming track.
     */
extern void CallbacksDcc_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                                    bool bit_value, bool service_mode);

    /**
     * @brief Consist (multi-unit) control.
     *
     * @param address           Decoder address the packet was for.
     * @param consist_address   Consist address, 0 = leave consist.
     * @param direction_normal  true = normal direction within the consist.
     */
extern void CallbacksDcc_on_consist_command(uint16_t address,
                                             uint8_t consist_address,
                                             bool direction_normal);

    /**
     * @brief Binary state control, short range (1-127).
     *
     * @param address       Decoder address the packet was for.
     * @param state_number  State number 1..127.
     * @param active        true = on.
     */
extern void CallbacksDcc_on_binary_state_short(uint16_t address,
                                                uint8_t state_number,
                                                bool active);
    /**
     * @brief Binary state control, long range (1-32767).
     *
     * @param address       Decoder address the packet was for.
     * @param state_number  State number 1..32767.
     * @param active        true = on.
     */
extern void CallbacksDcc_on_binary_state_long(uint16_t address,
                                               uint16_t state_number,
                                               bool active);

    /**
     * @brief Analog function output (0-255 value).
     *
     * @param address        Decoder address the packet was for.
     * @param output_number  Analog output number.
     * @param value          Output value 0..255.
     */
extern void CallbacksDcc_on_analog_function(uint16_t address,
                                             uint8_t output_number,
                                             uint8_t value);

    /** @brief Failsafe entered: no valid DCC packets for too long. Stop the motor here. */
extern void CallbacksDcc_on_failsafe_entered(void);
    /** @brief Failsafe exited: valid packets have resumed. */
extern void CallbacksDcc_on_failsafe_exited(void);

#ifdef __cplusplus
}
#endif

#endif /* DCC_COMPILE_DECODER */

#endif /* __CALLBACKS_DCC__ */
