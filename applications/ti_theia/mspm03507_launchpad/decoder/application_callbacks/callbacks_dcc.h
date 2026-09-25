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
 * Every callback below runs from DccConfig_run() in main-loop context: the GPIO
 * edge ISR only captures timestamps, main() feeds them to the bit decoder, and
 * the packet decoder dispatches queued packets when DccConfig_run() polls it.
 * The demo still keeps each callback short and queues its output in a ring
 * buffer, so the edge drain in main() is never starved and the same code would
 * remain safe if the dispatch were ever moved into an ISR.
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

    /**
     * @brief Clears the RECV ring buffer and restores the CV storage to factory defaults.
     *
     * @details Call before DccConfig_initialize(), which reads CV1/CV29 for the address.
     */
extern void CallbacksDcc_initialize(void);

    /**
     * @brief Writes at most one pending RECV line to the UART.
     *
     * @details Uses blocking transmit; call from the main loop only.
     */
extern void CallbacksDcc_drain(void);

    /**
     * @brief Discards all pending RECV lines.
     */
extern void CallbacksDcc_clear(void);

/* --- CV storage callbacks (wired to dcc_config_t.cv_read / .cv_write) ---
 * The library calls these to read and write Configuration Variables.
 * This demo stores CVs in a RAM array; replace with Flash or EEPROM
 * for persistent storage (see callbacks_dcc.c for the implementation). */

    /**
     * @brief Reads one CV from the demo's RAM storage.
     *
     * @param cv_number 1-based CV number.
     * @param value     Receives the CV value on success.
     *
     * @return true on success, false when the CV number is outside 1..CV_STORAGE_SIZE.
     */
extern bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value);

    /**
     * @brief Writes one CV to the demo's RAM storage.
     *
     * @param cv_number 1-based CV number.
     * @param value     Value to store.
     *
     * @return true on success, false when the CV number is outside 1..CV_STORAGE_SIZE.
     */
extern bool CallbacksDcc_cv_write(uint16_t cv_number, uint8_t value);

    /**
     * @brief Restores every CV to its factory default (wired to dcc_config_t.factory_reset).
     *
     * @details The library invokes this when a write command targets CV8, the read-only
     * Manufacturer ID (S-9.2.2). Also used by CallbacksDcc_initialize().
     */
extern void CallbacksDcc_factory_reset(void);

    /**
     * @brief Reads one byte from the indexed CV area (wired to dcc_config_t.cv_read_indexed).
     *
     * @details The library resolves the CV257-512 window through CV31/CV32 and passes the page
     * and offset here. This demo backs four pages (CV31 = 0, CV32 = 0..3) of 256 bytes each.
     *
     * @param page_hi CV31 value; only 0 is supported.
     * @param page_lo CV32 value; 0..3 are supported.
     * @param offset  Byte offset within the page (CV number minus 257).
     * @param value   Receives the byte on success.
     *
     * @return true on success, false for an unsupported page (the library answers NACK).
     */
extern bool CallbacksDcc_cv_read_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value);

    /**
     * @brief Writes one byte to the indexed CV area (wired to dcc_config_t.cv_write_indexed).
     *
     * @param page_hi CV31 value; only 0 is supported.
     * @param page_lo CV32 value; 0..3 are supported.
     * @param offset  Byte offset within the page (CV number minus 257).
     * @param value   Byte to store.
     *
     * @return true on success, false for an unsupported page (the library answers NACK).
     */
extern bool CallbacksDcc_cv_write_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t value);

    /**
     * @brief Clears CV29 feature bits this product does not implement (wired to dcc_config_t.cv29_apply_supported_features).
     *
     * @details The library decodes a CV29 write and calls this before storing it; whatever the
     * application leaves set is what gets stored. This demo clears analog operation and the speed
     * table, and clears RailCom when DCC_COMPILE_RAILCOM is off.
     *
     * @param flags Decoded @ref dcc_cv29_flags_t to adjust in place.
     */
extern void CallbacksDcc_cv29_apply_supported_features(dcc_cv29_flags_t *flags);

/* --- DCC command callbacks (wired to dcc_config_t.on_xxx) ---
 * Override these with your application logic.  Each one fires from
 * DccConfig_run() when the library dispatches the corresponding DCC
 * packet type. */

    /**
     * @brief Speed and direction command; drive your motor here.
     *
     * @param address   Decoded locomotive address.
     * @param speed     Speed step in the given mode.
     * @param direction true = forward, false = reverse.
     * @param mode      Step mode of the packet, a @ref dcc_speed_mode_enum.
     */
extern void CallbacksDcc_on_speed_command(uint16_t address, uint8_t speed,
                                           bool direction,
                                           dcc_speed_mode_enum mode);

    /**
     * @brief Emergency stop; cut motor power immediately.
     *
     * @param address Decoded locomotive address (0 for broadcast).
     */
extern void CallbacksDcc_on_emergency_stop(uint16_t address);

    /**
     * @brief Function on/off; control lights, sound triggers, couplers and so on.
     *
     * @param address         Decoded locomotive address.
     * @param function_number Function number, 0 (FL) to 68.
     * @param state           true = on, false = off.
     */
extern void CallbacksDcc_on_function_command(uint16_t address,
                                              uint8_t function_number,
                                              bool state);

    /**
     * @brief Basic accessory command (turnouts and switches).
     *
     * @param board_address 9-bit accessory board address.
     * @param output_pair   Output pair 0-3 on the board.
     * @param activate      true = activate, false = deactivate.
     */
extern void CallbacksDcc_on_accessory_basic_command(uint16_t board_address,
                                                     uint8_t output_pair,
                                                     bool activate);

    /**
     * @brief Extended accessory command (signal aspects).
     *
     * @param address 11-bit extended accessory address.
     * @param aspect  Aspect value 0-255.
     */
extern void CallbacksDcc_on_accessory_extended_command(uint16_t address,
                                                        uint8_t aspect);

/* CV programming callbacks -- the library handled the CV read/write
 * already; these notify you that it happened so you can log or react. */

    /**
     * @brief Notification that a CV byte write has been applied.
     *
     * @param cv_number    1-based CV number.
     * @param value        Value written.
     * @param service_mode true when it arrived in service mode, false for ops mode.
     */
extern void CallbacksDcc_on_cv_write(uint16_t cv_number, uint8_t value,
                                     bool service_mode);

    /**
     * @brief Notification that a CV byte verify has been processed.
     *
     * @param cv_number    1-based CV number.
     * @param value        Value the command station asked to verify.
     * @param service_mode true when it arrived in service mode, false for ops mode.
     */
extern void CallbacksDcc_on_cv_verify(uint16_t cv_number, uint8_t value,
                                      bool service_mode);

    /**
     * @brief Notification that a CV bit manipulation has been processed.
     *
     * @param cv_number    1-based CV number.
     * @param bit_position Bit position 0-7.
     * @param bit_value    Bit value written or verified.
     * @param service_mode true when it arrived in service mode, false for ops mode.
     */
extern void CallbacksDcc_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                                    bool bit_value, bool service_mode);

    /**
     * @brief Consist (multi-unit) control.
     *
     * @param address          Decoded locomotive address.
     * @param consist_address  Consist address, 0 to leave the consist.
     * @param direction_normal true when the loco faces the consist's normal direction.
     */
extern void CallbacksDcc_on_consist_command(uint16_t address,
                                             uint8_t consist_address,
                                             bool direction_normal);

    /**
     * @brief Short-form binary state control (states 1-127).
     *
     * @param address      Decoded locomotive address.
     * @param state_number Binary state number 1-127.
     * @param active       true = on, false = off.
     */
extern void CallbacksDcc_on_binary_state_short(uint16_t address,
                                                uint8_t state_number,
                                                bool active);

    /**
     * @brief Long-form binary state control (states 1-32767).
     *
     * @param address      Decoded locomotive address.
     * @param state_number Binary state number 1-32767.
     * @param active       true = on, false = off.
     */
extern void CallbacksDcc_on_binary_state_long(uint16_t address,
                                               uint16_t state_number,
                                               bool active);

    /**
     * @brief Analog function output.
     *
     * @param address       Decoded locomotive address.
     * @param output_number Analog output number.
     * @param value         Output value 0-255.
     */
extern void CallbacksDcc_on_analog_function(uint16_t address,
                                             uint8_t output_number,
                                             uint8_t value);

    /**
     * @brief Failsafe entered: no valid DCC packets have arrived for too long. Stop the motor here.
     */
extern void CallbacksDcc_on_failsafe_entered(void);

    /**
     * @brief Failsafe exited: valid DCC packets have resumed.
     */
extern void CallbacksDcc_on_failsafe_exited(void);

#ifdef __cplusplus
}
#endif

#endif /* DCC_COMPILE_DECODER */

#endif /* __CALLBACKS_DCC__ */
