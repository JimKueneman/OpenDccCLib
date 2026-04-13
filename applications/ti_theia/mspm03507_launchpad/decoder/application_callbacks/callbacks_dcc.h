/*
 * callbacks_dcc.h -- DCC decoder callback declarations for this demo.
 *
 * CUSTOMIZE THESE for your decoder hardware.
 *
 * Every callback below is called from ISR context (GPIO edge interrupt ->
 * bit decoder -> packet decoder -> your callback).  That means:
 *   - No printf, no malloc, no blocking calls.
 *   - Set a flag, write to a ring buffer, or update a volatile variable.
 *   - Keep execution time short (< 50 us).
 *
 * This demo just logs each command into a ring buffer (the "RECV" lines
 * you see on the UART).  In a real decoder you would drive motors, LEDs,
 * servos, or solenoids here instead.
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

/* Initialize CV storage defaults and clear the RECV ring buffer. */
extern void CallbacksDcc_initialize(void);

/* Push pending RECV lines to UART.  Call from main loop, not from ISR. */
extern void CallbacksDcc_drain(void);

/* Discard all pending RECV lines. */
extern void CallbacksDcc_clear(void);

/* --- CV storage callbacks (wired to dcc_config_t.cv_read / .cv_write) ---
 * The library calls these to read and write Configuration Variables.
 * This demo stores CVs in a RAM array; replace with Flash or EEPROM
 * for persistent storage (see callbacks_dcc.c for the implementation). */

extern bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value);
extern bool CallbacksDcc_cv_write(uint16_t cv_number, uint8_t value);

/* --- DCC command callbacks (wired to dcc_config_t.on_xxx) ---
 * Override these with your application logic.  Each one fires from ISR
 * context when the library decodes the corresponding DCC packet type.
 *
 * Speed and direction -- drive your motor here. */
extern void CallbacksDcc_on_speed_command(uint16_t address, uint8_t speed,
                                           bool direction,
                                           dcc_speed_mode_enum mode);

/* Emergency stop -- cut motor power immediately. */
extern void CallbacksDcc_on_emergency_stop(uint16_t address);

/* Function on/off -- control lights, sound triggers, couplers, etc. */
extern void CallbacksDcc_on_function_command(uint16_t address,
                                              uint8_t function_number,
                                              bool state);

/* Basic accessory command (turnouts/switches). */
extern void CallbacksDcc_on_accessory_basic_command(uint16_t board_address,
                                                     uint8_t output_pair,
                                                     bool activate);

/* Extended accessory command (signal aspects). */
extern void CallbacksDcc_on_accessory_extended_command(uint16_t address,
                                                        uint8_t aspect);

/* CV programming callbacks -- the library handled the CV read/write
 * already; these notify you that it happened so you can log or react. */
extern void CallbacksDcc_on_cv_write(uint16_t cv_number, uint8_t value,
                                     bool service_mode);
extern void CallbacksDcc_on_cv_verify(uint16_t cv_number, uint8_t value,
                                      bool service_mode);
extern void CallbacksDcc_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                                    bool bit_value, bool service_mode);

/* Consist (multi-unit) control. */
extern void CallbacksDcc_on_consist_command(uint16_t address,
                                             uint8_t consist_address,
                                             bool direction_normal);

/* Binary state control -- short range (1-127) and long range (1-32767). */
extern void CallbacksDcc_on_binary_state_short(uint16_t address,
                                                uint8_t state_number,
                                                bool active);
extern void CallbacksDcc_on_binary_state_long(uint16_t address,
                                               uint16_t state_number,
                                               bool active);

/* Analog function output (0-255 value). */
extern void CallbacksDcc_on_analog_function(uint16_t address,
                                             uint8_t output_number,
                                             uint8_t value);

/* Speed restriction (layout-imposed speed limit). */
extern void CallbacksDcc_on_speed_restriction(uint16_t address, bool enabled,
                                               uint8_t speed_limit);

/* Failsafe -- fires when no valid DCC packets arrive for too long,
 * and again when valid packets resume.  Stop the motor on enter. */
extern void CallbacksDcc_on_failsafe_entered(void);
extern void CallbacksDcc_on_failsafe_exited(void);

#ifdef __cplusplus
}
#endif

#endif /* DCC_COMPILE_DECODER */

#endif /* __CALLBACKS_DCC__ */
