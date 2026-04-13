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
 * @file dcc_config.h
 * @brief User-facing configuration struct and lifecycle API for OpenDccCLib.
 *
 * @details Users populate one @ref dcc_config_t struct with hardware driver
 * functions and optional application callbacks, then call DccConfig_initialize()
 * to bring up the entire stack. This is the only header users need to include.
 *
 * @author Jim Kueneman
 * @date 11 Apr 2026
 */

#ifndef __DCC_CONFIG__
#define __DCC_CONFIG__

#include <stdbool.h>
#include <stdint.h>

#include "dcc_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// Compile-time feature dependency validation
// =============================================================================

#if !defined(DCC_COMPILE_COMMAND_STATION) && !defined(DCC_COMPILE_DECODER)
#warning "No role enabled. Define DCC_COMPILE_COMMAND_STATION and/or DCC_COMPILE_DECODER in dcc_user_config.h."
#endif

#if defined(DCC_COMPILE_SERVICE_MODE_DIRECT) && !defined(DCC_COMPILE_COMMAND_STATION)
#error "DCC_COMPILE_SERVICE_MODE_DIRECT requires DCC_COMPILE_COMMAND_STATION"
#endif

#if defined(DCC_COMPILE_SERVICE_MODE_PAGED) && !defined(DCC_COMPILE_COMMAND_STATION)
#error "DCC_COMPILE_SERVICE_MODE_PAGED requires DCC_COMPILE_COMMAND_STATION"
#endif

#if defined(DCC_COMPILE_SERVICE_MODE_REGISTER) && !defined(DCC_COMPILE_COMMAND_STATION)
#error "DCC_COMPILE_SERVICE_MODE_REGISTER requires DCC_COMPILE_COMMAND_STATION"
#endif

#if defined(DCC_COMPILE_SERVICE_MODE_ADDRESS) && !defined(DCC_COMPILE_COMMAND_STATION)
#error "DCC_COMPILE_SERVICE_MODE_ADDRESS requires DCC_COMPILE_COMMAND_STATION"
#endif

// =============================================================================
// Command Station: per-channel hardware structs
// =============================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

    /**
     * @brief RailCom detector hardware drivers.
     *
     * @details Allocate one of these and point to it from dcc_output_hw_t::railcom
     * on any channel that has a RailCom detector. The detector analog front-end
     * converts the decoder's current-source signal into 250 kbaud UART data.
     */
typedef struct {

        /** @brief Tristate H-bridge to create the RailCom cutout window.
         *  Used by the old variable-period bit encoder (half_bit_isr). */
    void (*cutout_begin)(void);

        /** @brief Resume H-bridge drive after cutout window.
         *  Used by the old variable-period bit encoder (half_bit_isr). */
    void (*cutout_end)(void);

        /** @brief Disable (tristate) H-bridge for the cutout window.
         *  Used by the one-shot timer cutout module (dcc_railcom_cutout). */
    void (*hbridge_disable)(void);

        /** @brief Re-enable H-bridge after the cutout window.
         *  Used by the one-shot timer cutout module (dcc_railcom_cutout). */
    void (*hbridge_enable)(void);

        /** @brief Enable UART receive for RailCom Ch1.
         *  Used by the one-shot timer cutout module. */
    void (*uart_ch1_enable)(void);

        /** @brief Disable UART receive for RailCom Ch1. */
    void (*uart_ch1_disable)(void);

        /** @brief Enable UART receive for RailCom Ch2. */
    void (*uart_ch2_enable)(void);

        /** @brief Disable UART receive for RailCom Ch2. */
    void (*uart_ch2_disable)(void);

        /** @brief Read one byte from the RailCom 250 kbaud UART. Returns true if byte available. */
    bool (*uart_read)(uint8_t *byte);

        /** @brief RailCom datagram decoded. Fired from DccConfig_run(), NOT ISR. NULL = no notification. */
    void (*on_datagram)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram);

} dcc_railcom_hw_t;

    /**
     * @brief Per-channel DCC output hardware drivers.
     *
     * @details Each DCC output channel (main track, service track) has its own
     * timer, H-bridge, and optional peripherals. The timer drives a hardware
     * output-compare toggle pin for zero-jitter DCC signal generation.
     */
typedef struct {

        /**
         * @brief Start DCC bit timer. User configures their timer in output compare
         *  toggle mode so the track signal pin toggles automatically on compare match
         *  with zero jitter. Timer ISR must call the corresponding
         *  DccConfig_main_track_isr() or DccConfig_service_track_isr(). REQUIRED.
         */
    void (*timer_start)(uint16_t half_bit_period_usec);

        /**
         * @brief Change timer compare value for the next half-bit period. Called from
         *  ISR context, must be fast (single register write). REQUIRED.
         */
    void (*timer_set_period)(uint16_t half_bit_period_usec);

        /** @brief Stop DCC bit timer. REQUIRED. */
    void (*timer_stop)(void);

        /** @brief Toggle DCC output GPIO pin (ISR context). Used by the
         *  fixed-period shared timer architecture (tick_isr). NULL = use
         *  variable-period timer_set_period instead. */
    void (*pin_toggle)(void);

        /** @brief Enable or disable track power for this channel. REQUIRED. */
    void (*track_power_set)(bool enabled);

        /**
         * @brief Read current sense value. Returns milliamps (ADC) or 0/non-zero
         *  (comparator). Used for service mode ACK detection on the service track.
         *  May be used for overcurrent protection on either channel.
         *  NULL = feature disabled on this channel.
         */
    uint16_t (*current_sense_read)(void);

        /**
         * @brief RailCom detector hardware. NULL = no RailCom on this channel.
         *  In practice only the main track has a RailCom detector.
         */
    const dcc_railcom_hw_t *railcom;

} dcc_output_hw_t;

#endif /* DCC_COMPILE_COMMAND_STATION */

// =============================================================================
// User-facing configuration struct
// =============================================================================

    /**
     * @brief User configuration for OpenDccCLib.
     *
     * @details Populate this struct with hardware driver functions and optional
     * application callbacks, then pass it to DccConfig_initialize(). Required
     * fields are marked REQUIRED and must be non-NULL. NULL-optional fields
     * disable the associated feature at runtime.
     *
     * @code
     * static const dcc_railcom_hw_t my_railcom = {
     *     .hbridge_disable = &MyDriver_hbridge_disable,
     *     .hbridge_enable  = &MyDriver_hbridge_enable,
     *     .uart_ch1_enable = &MyDriver_ch1_enable,
     *     .uart_ch1_disable = &MyDriver_ch1_disable,
     *     .uart_ch2_enable = &MyDriver_ch2_enable,
     *     .uart_ch2_disable = &MyDriver_ch2_disable,
     *     .uart_read    = &MyDriver_railcom_uart_read,
     *     .on_datagram  = &MyApp_on_railcom,
     * };
     * static const dcc_config_t my_config = {
     *     .lock_shared_resources   = &MyDriver_lock,
     *     .unlock_shared_resources = &MyDriver_unlock,
     *     .get_timestamp_usec      = &MyDriver_micros,
     *     .shared_timer_start      = &MyDriver_shared_timer_start,
     *     .shared_timer_stop       = &MyDriver_shared_timer_stop,
     *     .railcom_timer_start     = &MyDriver_railcom_timer_start,
     *     .railcom_timer_stop      = &MyDriver_railcom_timer_stop,
     *     .main_track = {
     *         .pin_toggle       = &MyDriver_main_pin_toggle,
     *         .track_power_set  = &MyDriver_main_power_set,
     *         .railcom          = &my_railcom,
     *     },
     *     .service_track = {
     *         .pin_toggle         = &MyDriver_svc_pin_toggle,
     *         .track_power_set    = &MyDriver_svc_power_set,
     *         .current_sense_read = &MyDriver_svc_current_sense,
     *     },
     * };
     * DccConfig_initialize(&my_config);
     * @endcode
     */
typedef struct {

    // =========================================================================
    // REQUIRED: Common (always present)
    // =========================================================================

        /** @brief Disable interrupts / acquire mutex. REQUIRED. */
    void (*lock_shared_resources)(void);

        /** @brief Re-enable interrupts / release mutex. REQUIRED. */
    void (*unlock_shared_resources)(void);

        /** @brief Return current time in microseconds. REQUIRED. */
    uint32_t (*get_timestamp_usec)(void);

    // =========================================================================
    // Command Station: per-channel hardware
    // =========================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

        /** @brief Start the shared fixed-period DCC timer. Both main track and
         *  service track are clocked from this single timer via tick_isr.
         *  Called with DCC_ONE_BIT_HALF_PERIOD_US (58us). NULL = use per-channel
         *  variable-period timers instead. */
    void (*shared_timer_start)(uint16_t period_usec);

        /** @brief Stop the shared fixed-period DCC timer. */
    void (*shared_timer_stop)(void);

        /** @brief Start the RailCom cutout one-shot timer with the given period
         *  in microseconds. NULL = no RailCom cutout timing. */
    void (*railcom_timer_start)(uint16_t period_usec);

        /** @brief Stop the RailCom cutout one-shot timer. */
    void (*railcom_timer_stop)(void);

        /** @brief Main track output hardware. Runs the scheduler (normal DCC). */
    dcc_output_hw_t main_track;

        /** @brief Service track output hardware. Runs service mode (programming). */
    dcc_output_hw_t service_track;

    // =========================================================================
    // Command Station: OPTIONAL application callbacks (NULL = no notification)
    // =========================================================================

        /** @brief Scheduler has no packets to send (idle packets still generated). */
    void (*on_idle)(void);

        /** @brief Packet fully transmitted on the wire. */
    void (*on_packet_sent)(const dcc_packet_t *packet);

        /** @brief Service mode programming operation finished. */
    void (*on_service_mode_complete)(dcc_service_mode_result_t result);

#endif /* DCC_COMPILE_COMMAND_STATION */

    // =========================================================================
    // Decoder: REQUIRED hardware drivers
    // =========================================================================

#ifdef DCC_COMPILE_DECODER

        /** @brief Read CV from persistent storage. Returns true on success. REQUIRED. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Write CV to persistent storage. Returns true on success. REQUIRED. */
    bool (*cv_write)(uint16_t cv_number, uint8_t value);

    // =========================================================================
    // Decoder: NULL-optional hardware drivers
    // =========================================================================

        /**
         * @brief Transmit 4/8-encoded RailCom response byte.
         *  NULL = no RailCom responses.
         */
    void (*railcom_uart_write)(uint8_t byte);

    // =========================================================================
    // Decoder: OPTIONAL application callbacks (NULL = no notification)
    // =========================================================================

        /** @brief Speed command received. */
    void (*on_speed_command)(uint16_t address, uint8_t speed, bool direction, dcc_speed_mode_enum mode);

        /** @brief Emergency stop received for specific address. */
    void (*on_emergency_stop)(uint16_t address);

        /** @brief Function command received. */
    void (*on_function_command)(uint16_t address, uint8_t function_number, bool state);

        /** @brief Basic accessory command received.
         *  In decoder-address mode (CV541 bit 6 = 0): board_address is 9-bit
         *  board address, output_pair is the 3-bit DDD field.
         *  In output-address mode (CV541 bit 6 = 1): board_address is the
         *  11-bit flat output address, output_pair is the R bit (0 or 1). */
    void (*on_accessory_basic_command)(uint16_t board_address, uint8_t output_pair, bool activate);

        /** @brief Extended accessory (signal aspect) command received. */
    void (*on_accessory_extended_command)(uint16_t address, uint8_t aspect);

        /** @brief CV write completed on this decoder. */
    void (*on_cv_write)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV verify received on this decoder. */
    void (*on_cv_verify)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV bit manipulation received on this decoder. */
    void (*on_cv_bit)(uint16_t cv_number, uint8_t bit_position, bool bit_value, bool service_mode);

        /** @brief Consist control command received. */
    void (*on_consist_command)(uint16_t address, uint8_t consist_address, bool direction_normal);

        /** @brief Binary state short (1-127) received. */
    void (*on_binary_state_short)(uint16_t address, uint8_t state_number, bool active);

        /** @brief Binary state long (1-32767) received. */
    void (*on_binary_state_long)(uint16_t address, uint16_t state_number, bool active);

        /** @brief Analog function output received. */
    void (*on_analog_function)(uint16_t address, uint8_t output_number, uint8_t value);

        /** @brief Speed restriction command received. */
    void (*on_speed_restriction)(uint16_t address, bool enabled, uint8_t speed_limit);

        /** @brief Decoder entered fail-safe mode (no valid packet timeout). */
    void (*on_failsafe_entered)(void);

        /** @brief Decoder exited fail-safe mode (valid packet received). */
    void (*on_failsafe_exited)(void);

        /** @brief Fire a service mode ACK pulse. NULL = no ACK hardware. */
    void (*fire_ack_pulse)(void);

#endif /* DCC_COMPILE_DECODER */

} dcc_config_t;

// =============================================================================
// Lifecycle & ISR Entry Points
// =============================================================================

    /**
     * @brief Initialize the DCC library with user configuration.
     * @param config Pointer to user-populated configuration struct.
     */
extern void DccConfig_initialize(const dcc_config_t *config);

    /**
     * @brief Main loop processing. Call repeatedly from your main loop.
     *
     * @details Runs the scheduler, drains RailCom buffers, processes decoded
     * packets, fires application callbacks. All callbacks fire from this context.
     */
extern void DccConfig_run(void);

#ifdef DCC_COMPILE_COMMAND_STATION

    // =========================================================================
    // Main track ISR and power control
    // =========================================================================

    /**
     * @brief Main track timer ISR entry point.
     * @details Call from the main track timer compare-match ISR. Drives the
     *  bit encoder for the main track output. Must complete in < 30 us.
     */
extern void DccConfig_main_track_isr(void);

    /**
     * @brief Service track timer ISR entry point.
     * @details Call from the service track timer compare-match ISR. Drives the
     *  bit encoder for the service track output and samples current sense for
     *  ACK detection. Must complete in < 30 us.
     */
extern void DccConfig_service_track_isr(void);

    /**
     * @brief Shared fixed-period timer ISR entry point.
     * @details Call from the 58us shared timer ISR. Drives the tick_isr for
     *  both main track and service track bit encoders. Also samples current
     *  sense on the service track for ACK detection.
     */
extern void DccConfig_shared_timer_isr(void);

    /**
     * @brief RailCom cutout one-shot timer ISR entry point.
     * @details Call from the RailCom cutout timer ISR. Drives the cutout
     *  state machine through DELAY -> CH1 -> CH2 -> IDLE.
     */
extern void DccConfig_railcom_cutout_timer_isr(void);

    /**
     * @brief 100ms timer tick. Call from a 100ms periodic timer or main loop.
     * @details Used for timeout checking and periodic housekeeping.
     */
extern void DccConfig_100ms_timer_tick(void);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

    /**
     * @brief Decoder bit edge detected. Call from input-capture ISR.
     * @param timestamp_usec Microsecond timestamp of the signal edge.
     *
     * @details The library classifies one/zero bits from edge timing internally.
     * User ISR only needs to read the timer and call this function on each edge.
     */
extern void DccConfig_decoder_edge(uint32_t timestamp_usec);

#endif /* DCC_COMPILE_DECODER */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DCC_CONFIG__ */
