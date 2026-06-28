/*
 * callbacks_dcc.c -- DCC decoder callback implementations.
 *
 * HOW THIS WORKS:
 * The DCC library calls your callbacks from ISR context.  Because you
 * cannot safely call printf or UART transmit inside an ISR, this demo
 * uses a ring buffer pattern:
 *
 *   ISR -> callback -> _recv_enqueue("RECV ...") -> ring buffer
 *   main loop -> CallbacksDcc_drain() -> UART output
 *
 * TO ADAPT FOR A REAL DECODER:
 *   - Replace the _recv_enqueue() calls with your own logic
 *     (e.g. set motor PWM, toggle a GPIO for a light, etc.)
 *   - Replace the RAM-based _cv_storage[] array with Flash or EEPROM
 *     read/write calls so CVs survive power cycles.
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
/*   Producer: ISR context  (writes to _recv_head)                            */
/*   Consumer: main loop    (reads from _recv_tail)                           */
/* No mutex is needed because there is exactly one writer and one reader,     */
/* and the head/tail indices are volatile.                                    */
/* ========================================================================== */

#define RECV_RING_SLOTS   64
#define RECV_SLOT_SIZE    80

static char _recv_ring[RECV_RING_SLOTS][RECV_SLOT_SIZE];
static volatile uint8_t _recv_head = 0;   /* ISR writes here */
static volatile uint8_t _recv_tail = 0;   /* main loop reads here */

/* Enqueue a formatted RECV line.  Called from ISR context only.
 * If the ring is full the message is silently dropped. */
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

#define CV_STORAGE_SIZE  1024

static uint8_t _cv_storage[CV_STORAGE_SIZE];

/* ========================================================================== */
/* Public API                                                                 */
/* ========================================================================== */

void CallbacksDcc_initialize(void) {

    /* Clear ring buffer */
    _recv_head = 0;
    _recv_tail = 0;

    /* Restore CV storage to NMRA defaults. */
    CallbacksDcc_factory_reset();

}

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

void CallbacksDcc_drain(void) {

    if (_recv_tail != _recv_head) {

        TI_UartDriver_write_string(_recv_ring[_recv_tail]);
        TI_UartDriver_write_string("\r\n");
        _recv_tail = (_recv_tail + 1) % RECV_RING_SLOTS;

    }

}

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

bool CallbacksDcc_cv_read(uint16_t cv_number, uint8_t *value) {

    if (cv_number < 1 || cv_number > CV_STORAGE_SIZE) {

        return false;

    }

    *value = _cv_storage[cv_number - 1];
    return true;

}

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

static uint8_t _idx_store[4][256];

bool CallbacksDcc_cv_read_indexed(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value) {

    if (page_hi != 0 || page_lo >= 4) {

        return false;

    }

    *value = _idx_store[page_lo][offset];
    return true;

}

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

void CallbacksDcc_on_cv29_config_changed(const dcc_cv29_flags_t *flags) {

    /* The library decoded CV29 for us. This demo reports the flags over UART; a real
     * product would act here (enable RailCom Tx, switch analog, set speed mode, ...). */
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
/* DCC library callbacks (called from ISR context)                            */
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

void CallbacksDcc_on_speed_command(uint16_t address, uint8_t speed,
                                    bool direction,
                                    dcc_speed_mode_enum mode) {

    _recv_enqueue("RECV SPEED addr=%u speed=%u dir=%s mode=%u",
                  address, speed, direction ? "FWD" : "REV",
                  mode == DCC_SPEED_MODE_128 ? 128 :
                  mode == DCC_SPEED_MODE_28  ? 28  : 14);

}

void CallbacksDcc_on_emergency_stop(uint16_t address) {

    _recv_enqueue("RECV ESTOP addr=%u", address);

}

void CallbacksDcc_on_function_command(uint16_t address,
                                       uint8_t function_number,
                                       bool state) {

    _recv_enqueue("RECV FUNC addr=%u func=%u state=%s",
                  address, function_number, state ? "ON" : "OFF");

}

void CallbacksDcc_on_accessory_basic_command(uint16_t board_address,
                                              uint8_t output_pair,
                                              bool activate) {

    _recv_enqueue("RECV ACC board=%u pair=%u activate=%s",
                  board_address, output_pair, activate ? "ON" : "OFF");

}

void CallbacksDcc_on_accessory_extended_command(uint16_t address,
                                                 uint8_t aspect) {

    _recv_enqueue("RECV ACCE addr=%u aspect=%u", address, aspect);

}

void CallbacksDcc_on_cv_write(uint16_t cv_number, uint8_t value,
                               bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_WRITE cv=%u value=%u", cv_number, value);

}

void CallbacksDcc_on_cv_verify(uint16_t cv_number, uint8_t value,
                                bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_VERIFY cv=%u value=%u", cv_number, value);

}

void CallbacksDcc_on_cv_bit(uint16_t cv_number, uint8_t bit_position,
                             bool bit_value, bool service_mode) {

    (void)service_mode;
    _recv_enqueue("RECV CV_BIT cv=%u bit=%u value=%u",
                  cv_number, bit_position, bit_value ? 1 : 0);

}

void CallbacksDcc_on_consist_command(uint16_t address,
                                      uint8_t consist_address,
                                      bool direction_normal) {

    _recv_enqueue("RECV CONSIST addr=%u consist=%u dir=%s",
                  address, consist_address,
                  direction_normal ? "NORMAL" : "REVERSE");

}

void CallbacksDcc_on_binary_state_short(uint16_t address,
                                         uint8_t state_number,
                                         bool active) {

    _recv_enqueue("RECV BSS addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");

}

void CallbacksDcc_on_binary_state_long(uint16_t address,
                                        uint16_t state_number,
                                        bool active) {

    _recv_enqueue("RECV BSL addr=%u state=%u active=%s",
                  address, state_number, active ? "ON" : "OFF");

}

void CallbacksDcc_on_analog_function(uint16_t address,
                                      uint8_t output_number,
                                      uint8_t value) {

    _recv_enqueue("RECV ANALOG addr=%u output=%u value=%u",
                  address, output_number, value);

}

void CallbacksDcc_on_failsafe_entered(void) {

    _recv_enqueue("RECV FAILSAFE_ENTER");

}

void CallbacksDcc_on_failsafe_exited(void) {

    _recv_enqueue("RECV FAILSAFE_EXIT");

}

#endif /* DCC_COMPILE_DECODER */
