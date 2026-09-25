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
 * @brief DCC decoder callback implementations.
 *
 * @details HOW THIS WORKS:
 * The DCC library dispatches your callbacks from DccConfig_run() in
 * main-loop context.  The demo still keeps them short and defers the slow
 * UART output through a ring buffer, so the edge drain in main() is never
 * starved and the code stays safe if the dispatch is ever moved into an ISR:
 *
 *   DccConfig_run() -> callback -> _recv_enqueue("RECV ...") -> ring buffer
 *   main loop -> CallbacksDcc_drain() -> UART output
 *
 * TO ADAPT FOR A REAL DECODER:
 *   - Replace the _recv_enqueue() calls with your own logic
 *     (e.g. set motor PWM, toggle a GPIO for a light, etc.)
 *   - Replace the RAM-based _cv_storage[] array with Flash or EEPROM
 *     read/write calls so CVs survive power cycles.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#include "callbacks_dcc.h"

#ifdef DCC_COMPILE_DECODER

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../application_drivers/ti_driverlib_uart_driver.h"
#include "../application_drivers/ack_pulse_driver.h"

/* ========================================================================== */
/* RECV ring buffer                                                           */
/*                                                                            */
/* A lock-free single-producer / single-consumer ring buffer.                 */
/*   Producer: library callbacks (writes to _recv_head)                       */
/*   Consumer: CallbacksDcc_drain() (reads from _recv_tail)                   */
/* Both run in the main loop today; the split still matters because it keeps  */
/* exactly one writer and one reader with volatile indices, so no lock would  */
/* be needed even if the producer moved into an ISR.                          */
/* ========================================================================== */

    /** @brief Number of RECV lines the ring can hold (one slot is always kept free). */
#define RECV_RING_SLOTS   64
    /** @brief Bytes per RECV line including the null terminator. */
#define RECV_SLOT_SIZE    80

    /** @brief RECV line ring buffer. */
static char _recv_ring[RECV_RING_SLOTS][RECV_SLOT_SIZE];
    /** @brief Ring write index, advanced by the producer only. */
static volatile uint8_t _recv_head = 0;   /* ISR writes here */
    /** @brief Ring read index, advanced by CallbacksDcc_drain() only. */
static volatile uint8_t _recv_tail = 0;   /* main loop reads here */

    /**
     * @brief Formats a RECV line into the next free ring slot.
     *
     * @details Called by the library callbacks. When the ring is full the line is silently dropped.
     *
     * @param fmt printf-style format string.
     * @param ... Format arguments.
     */
static void _recv_enqueue(const char *fmt, ...) {

    uint8_t next = (_recv_head + 1) % RECV_RING_SLOTS;

    if (next == _recv_tail) {

        return;  /* ring full -- drop */

    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(_recv_ring[_recv_head], RECV_SLOT_SIZE, fmt, args);
    va_end(args);

    _recv_head = next;

}

/* ========================================================================== */
/* CV stub storage (RAM only -- lost on power cycle)                          */
/*                                                                            */
/* A real decoder would read/write CVs from Flash or EEPROM here.             */
/* The array is indexed by (cv_number - 1) because DCC CVs are 1-based.      */
/* ========================================================================== */

    /** @brief Number of CVs held in RAM; CV numbers 1..CV_STORAGE_SIZE are valid. */
#define CV_STORAGE_SIZE  1024

    /** @brief RAM CV store, indexed by (cv_number - 1). Lost on power cycle. */
static uint8_t _cv_storage[CV_STORAGE_SIZE];

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

    /**
     * @brief Clears the RECV ring buffer and restores the CV storage to factory defaults.
     */
void CallbacksDcc_initialize(void) {

    /* Clear ring buffer */
    _recv_head = 0;
    _recv_tail = 0;

    /* Restore CV storage to NMRA defaults. */
    CallbacksDcc_factory_reset();

}

    /**
     * @brief Zeroes the CV store and writes the NMRA defaults for CV1, CV15, CV16 and CV29.
     */
void CallbacksDcc_factory_reset(void) {

    /* Restore CV storage to NMRA defaults.
     * CV1  = primary short address (default 3).
     * CV15 = decoder lock key, CV16 = decoder lock value (both 0 = unlocked).
     * CV29 = configuration register (0x06 = 28/128 speed steps, short addr). */
    memset(_cv_storage, 0, sizeof(_cv_storage));
    _cv_storage[0]  = 3;
    _cv_storage[14] = 0;
    _cv_storage[15] = 0;
    _cv_storage[28] = 0x06;

}

    /**
     * @brief Writes one pending RECV line (if any) to the UART and advances the read index.
     *
     * @details One line per call keeps each main-loop pass short.
     */
void CallbacksDcc_drain(void) {

    if (_recv_tail != _recv_head) {

        TI_UartDriver_write_string(_recv_ring[_recv_tail]);
        TI_UartDriver_write_string("\r\n");
        _recv_tail = (_recv_tail + 1) % RECV_RING_SLOTS;

    }

}

    /**
     * @brief Resets both ring indices, discarding every pending RECV line.
     */
void CallbacksDcc_clear(void) {

    _recv_head = 0;
    _recv_tail = 0;

}

/* ========================================================================== */
/* CV stub read/write                                                         */
/*                                                                            */
/* Replace these two functions with your persistent storage implementation.   */
/* Return true on success, false if cv_number is out of range.                */
/* ========================================================================== */

    /**
     * @brief Reads one CV from the RAM store after a range check.
     *
     * @verbatim
     * @param cv_number 1-based CV number.
     * @param value     Receives the CV value on success.
     * @endverbatim
     *
     * @return true on success, false when the CV number is outside 1..CV_STORAGE_SIZE.
     */
bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value) {

    if (cv_number < 1 || cv_number > CV_STORAGE_SIZE) {

        return false;

    }

    *value = _cv_storage[cv_number - 1];
    return true;

}

    /**
     * @brief Writes one CV to the RAM store after a range check.
     *
     * @verbatim
     * @param cv_number 1-based CV number.
     * @param value     Value to store.
     * @endverbatim
     *
     * @return true on success, false when the CV number is outside 1..CV_STORAGE_SIZE.
     */
bool CallbacksDcc_cv_write(uint16_t cv_number, uint8_t value) {

    if (cv_number < 1 || cv_number > CV_STORAGE_SIZE) {

        return false;

    }

    _cv_storage[cv_number - 1] = value;
    return true;

}

/* ========================================================================== */
/* Indexed CV storage -- 4 demo pages (CV31=0, CV32=0..3), 256 bytes each.     */
/* The library resolves the CV257-512 window via CV31/CV32 and calls these     */
/* with (page, offset).  Unsupported pages return false (NACK).                */
/* ========================================================================== */

    /** @brief Indexed CV pages: CV31 = 0, CV32 = 0..3, 256 bytes each. */
static uint8_t _idx_store[4][256];

    /**
     * @brief Reads one byte from the demo's four indexed CV pages.
     *
     * @verbatim
     * @param page_hi CV31 value; only 0 is supported.
     * @param page_lo CV32 value; 0..3 are supported.
     * @param offset  Byte offset within the page.
     * @param value   Receives the byte on success.
     * @endverbatim
     *
     * @return true on success, false for an unsupported page.
     */
bool CallbacksDcc_cv_read_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value) {

    if (page_hi != 0 || page_lo >= 4) {

        return false;

    }

    *value = _idx_store[page_lo][offset];
    return true;

}

    /**
     * @brief Writes one byte to the demo's four indexed CV pages and logs a RECV CVIDX line.
     *
     * @verbatim
     * @param page_hi CV31 value; only 0 is supported.
     * @param page_lo CV32 value; 0..3 are supported.
     * @param offset  Byte offset within the page.
     * @param value   Byte to store.
     * @endverbatim
     *
     * @return true on success, false for an unsupported page.
     */
bool CallbacksDcc_cv_write_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t value) {

    if (page_hi != 0 || page_lo >= 4) {

        return false;

    }

    _idx_store[page_lo][offset] = value;
    _recv_enqueue("RECV CVIDX page=%u off=%u val=%u",
                  (unsigned)(((unsigned)page_hi << 8) | page_lo),
                  (unsigned)offset, (unsigned)value);
    return true;

}

    /**
     * @brief Clears the CV29 features this demo does not implement, then logs what will be stored.
     *
     * @details Analog operation and the speed table are always cleared; RailCom is cleared only
     * when DCC_COMPILE_RAILCOM is not defined.
     *
     * @verbatim
     * @param flags Decoded dcc_cv29_flags_t to adjust in place.
     * @endverbatim
     */
void CallbacksDcc_cv29_apply_supported_features(dcc_cv29_flags_t *flags) {

    /* The library decoded the requested CV29 config and forced the reserved bit; our job is
     * to clear any feature this product does not implement so it can never be stored as "on"
     * (S-9.2.2). This decode/dispatch demo implements no analog conversion and no speed table,
     * and no RailCom Tx unless DCC_COMPILE_RAILCOM is defined, so it clears all three. A real
     * product clears only what it actually lacks. The RECV line then reports exactly what the
     * library will store. */
    flags->power_source_conversion = false;   /* demo has no analog operation */
    flags->speed_table_enabled     = false;   /* demo has no speed table */
#if !defined(DCC_COMPILE_RAILCOM)
    flags->railcom_enabled = false;           /* no RailCom Tx compiled in */
#endif

    _recv_enqueue("RECV CV29 dir=%u steps=%u analog=%u railcom=%u sptbl=%u extaddr=%u acc=%u",
                  (unsigned)flags->direction_reversed,
                  (unsigned)flags->speed_steps_28_128,
                  (unsigned)flags->power_source_conversion,
                  (unsigned)flags->railcom_enabled,
                  (unsigned)flags->speed_table_enabled,
                  (unsigned)flags->extended_address,
                  (unsigned)flags->accessory_decoder);

}

/* ========================================================================== */
/* DCC library callbacks (dispatched from DccConfig_run())                    */
/*                                                                            */
/* Each function below is called when the library decodes the corresponding   */
/* DCC command.  This demo just logs them to the ring buffer.                 */
/*                                                                            */
/* TO CUSTOMIZE: replace the _recv_enqueue() call with your own logic.        */
/* For example, in on_speed_command you might set a PWM duty cycle:           */
/*                                                                            */
/*   void CallbacksDcc_on_speed_command(...) {                                */
/*       motor_pwm_set(speed, direction);                                     */
/*   }                                                                        */
/* ========================================================================== */

    /**
     * @brief Logs a RECV SPEED line with the step mode shown as 14, 28 or 128.
     *
     * @verbatim
     * @param address   Decoded locomotive address.
     * @param speed     Speed step in the given mode.
     * @param direction true = forward, false = reverse.
     * @param mode      Step mode of the packet.
     * @endverbatim
     */
void CallbacksDcc_on_speed_command(uint16_t address, uint8_t speed,
                                    bool direction,
                                    dcc_speed_mode_enum mode) {

    _recv_enqueue("RECV SPEED addr=%u speed=%u dir=%s mode=%u",
                  address, speed, direction ? "FWD" : "REV",
                  mode == DCC_SPEED_MODE_128 ? 128 :
                  mode == DCC_SPEED_MODE_28  ? 28  : 14);

}

    /**
     * @brief Logs a RECV ESTOP line.
     *
     * @verbatim
     * @param address Decoded locomotive address.
     * @endverbatim
     */
void CallbacksDcc_on_emergency_stop(uint16_t address) {

    _recv_enqueue("RECV ESTOP addr=%u", address);

}

    /**
     * @brief Logs a RECV FUNC line.
     *
     * @verbatim
     * @param address         Decoded locomotive address.
     * @param function_number Function number 0-68.
     * @param state           true = on, false = off.
     * @endverbatim
     */
void CallbacksDcc_on_function_command(uint16_t address,
                                       uint8_t function_number,
                                       bool state) {

    _recv_enqueue("RECV FUNC addr=%u func=%u state=%s",
                  address, function_number, state ? "ON" : "OFF");

}

    /**
     * @brief Logs a RECV ACC line.
     *
     * @verbatim
     * @param board_address 9-bit accessory board address.
     * @param output_pair   Output pair 0-3.
     * @param activate      true = activate, false = deactivate.
     * @endverbatim
     */
void CallbacksDcc_on_accessory_basic_command(uint16_t board_address,
                                              uint8_t output_pair,
                                              bool activate) {

    _recv_enqueue("RECV ACC board=%u pair=%u activate=%s",
                  board_address, output_pair, activate ? "ON" : "OFF");

}

    /**
     * @brief Logs a RECV ACCE line.
     *
     * @verbatim
     * @param address 11-bit extended accessory address.
     * @param aspect  Aspect value 0-255.
     * @endverbatim
     */
void CallbacksDcc_on_accessory_extended_command(uint16_t address,
                                                 uint8_t aspect) {

    _recv_enqueue("RECV ACCE addr=%u aspect=%u", address, aspect);

}

    /**
     * @brief Logs a RECV CV_WRITE line; the service_mode flag is not reported.
     *
     * @verbatim
     * @param cv_number    1-based CV number.
     * @param value        Value written.
     * @param service_mode true for service mode, false for ops mode (unused).
     * @endverbatim
     */
void CallbacksDcc_on_cv_write(uint16_t cv_number, uint8_t value,
                               bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_WRITE cv=%u value=%u", cv_number, value);

}

    /**
     * @brief Logs a RECV CV_VERIFY line; the service_mode flag is not reported.
     *
     * @verbatim
     * @param cv_number    1-based CV number.
     * @param value        Value the command station asked to verify.
     * @param service_mode true for service mode, false for ops mode (unused).
     * @endverbatim
     */
void CallbacksDcc_on_cv_verify(uint16_t cv_number, uint8_t value,
                                bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_VERIFY cv=%u value=%u", cv_number, value);

}

    /**
     * @brief Logs a RECV CV_BIT line; the service_mode flag is not reported.
     *
     * @verbatim
     * @param cv_number    1-based CV number.
     * @param bit_position Bit position 0-7.
     * @param bit_value    Bit value written or verified.
     * @param service_mode true for service mode, false for ops mode (unused).
     * @endverbatim
     */
void CallbacksDcc_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                             bool bit_value, bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_BIT cv=%u bit=%u value=%u",
                  cv_number, bit_position, bit_value ? 1 : 0);

}

    /**
     * @brief Logs a RECV CONSIST line.
     *
     * @verbatim
     * @param address          Decoded locomotive address.
     * @param consist_address  Consist address, 0 to leave the consist.
     * @param direction_normal true when the loco faces the consist's normal direction.
     * @endverbatim
     */
void CallbacksDcc_on_consist_command(uint16_t address,
                                      uint8_t consist_address,
                                      bool direction_normal) {

    _recv_enqueue("RECV CONSIST addr=%u consist=%u dir=%s",
                  address, consist_address,
                  direction_normal ? "NORMAL" : "REVERSE");

}

    /**
     * @brief Logs a RECV BSS line.
     *
     * @verbatim
     * @param address      Decoded locomotive address.
     * @param state_number Binary state number 1-127.
     * @param active       true = on, false = off.
     * @endverbatim
     */
void CallbacksDcc_on_binary_state_short(uint16_t address,
                                         uint8_t state_number,
                                         bool active) {

    _recv_enqueue("RECV BSS addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");

}

    /**
     * @brief Logs a RECV BSL line.
     *
     * @verbatim
     * @param address      Decoded locomotive address.
     * @param state_number Binary state number 1-32767.
     * @param active       true = on, false = off.
     * @endverbatim
     */
void CallbacksDcc_on_binary_state_long(uint16_t address,
                                        uint16_t state_number,
                                        bool active) {

    _recv_enqueue("RECV BSL addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");

}

    /**
     * @brief Logs a RECV ANALOG line.
     *
     * @verbatim
     * @param address       Decoded locomotive address.
     * @param output_number Analog output number.
     * @param value         Output value 0-255.
     * @endverbatim
     */
void CallbacksDcc_on_analog_function(uint16_t address,
                                      uint8_t output_number,
                                      uint8_t value) {

    _recv_enqueue("RECV ANALOG addr=%u output=%u value=%u",
                  address, output_number, value);

}

    /**
     * @brief Logs a RECV FAILSAFE_ENTER line.
     */
void CallbacksDcc_on_failsafe_entered(void) {

    _recv_enqueue("RECV FAILSAFE_ENTER");

}

    /**
     * @brief Logs a RECV FAILSAFE_EXIT line.
     */
void CallbacksDcc_on_failsafe_exited(void) {

    _recv_enqueue("RECV FAILSAFE_EXIT");

}

#endif /* DCC_COMPILE_DECODER */
