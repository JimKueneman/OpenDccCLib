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
 * @date 27 Jun 2026
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

#if !defined(DCC_COMPILE_COMMAND_STATION) && !defined(DCC_COMPILE_DECODER) && !defined(DCC_COMPILE_ACCESSORY_DECODER)
#warning "No role enabled. Define DCC_COMPILE_COMMAND_STATION, DCC_COMPILE_DECODER, and/or DCC_COMPILE_ACCESSORY_DECODER in dcc_user_config.h."
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

#if defined(DCC_COMPILE_RAILCOM)

    /**
     * @brief RailCom detector hardware drivers.
     *
     * @details Allocate one of these and point to it from dcc_output_hw_t::railcom
     * on any channel that has a RailCom detector. The detector analog front-end
     * converts the decoder's current-source signal into 250 kbaud UART data.
     */
typedef struct {

        /** @brief Tristate the H-bridge output stage for the RailCom cutout window.
         *  Called by the library at T_CS. Required if using RailCom. */
    void (*begin_railcom_cutout)(void);

        /** @brief Restore H-bridge to normal drive mode after the cutout window.
         *  Called by the library at T_CE. Required if using RailCom. */
    void (*end_railcom_cutout)(void);

        /** @brief Enable UART Rx for RailCom data reception.
         *  Called by the library at T_TS1 and T_TS2. Required if using RailCom. */
    void (*uart_rx_enable)(void);

        /** @brief Disable UART Rx after RailCom data reception.
         *  Called by the library at T_TC1 and T_CE. Required if using RailCom. */
    void (*uart_rx_disable)(void);

        /** @brief Read one byte from the RailCom 250 kbaud UART. Returns true if byte available. */
    bool (*uart_read)(uint8_t *byte);

        /** @brief RailCom datagram decoded. Fired from DccConfig_run(), NOT ISR. NULL = no notification. */
    void (*on_railcom_datagram_result)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram);

} dcc_railcom_hw_t;

#endif /* DCC_COMPILE_RAILCOM */

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
         *  with zero jitter. Timer ISR must call DccConfig_58us_timer_isr(). REQUIRED.
         */
    void (*timer_start)(uint16_t half_bit_period_usec);

        /** @brief Stop DCC bit timer. REQUIRED. */
    void (*timer_stop)(void);

        /** @brief Toggle DCC output GPIO pin (ISR context). Used by the
         *  fixed-period shared timer architecture (tick_isr). REQUIRED. */
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

#if defined(DCC_COMPILE_RAILCOM)
        /**
         * @brief RailCom detector hardware. NULL = no RailCom on this channel.
         *  In practice only the main track has a RailCom detector.
         */
    const dcc_railcom_hw_t *railcom;
#endif /* DCC_COMPILE_RAILCOM */

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
     *     .begin_railcom_cutout = &MyDriver_hbridge_disable,
     *     .end_railcom_cutout   = &MyDriver_hbridge_enable,
     *     .uart_rx_enable       = &MyDriver_uart_rx_enable,
     *     .uart_rx_disable      = &MyDriver_uart_rx_disable,
     *     .uart_read            = &MyDriver_railcom_uart_read,
     *     .on_railcom_datagram_result = &MyApp_on_railcom,
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
         *  Called with DCC_ONE_BIT_HALF_PERIOD_US (58us). REQUIRED. */
    void (*shared_timer_start)(uint16_t period_usec);

        /** @brief Stop the shared fixed-period DCC timer. */
    void (*shared_timer_stop)(void);

#if defined(DCC_COMPILE_RAILCOM)
        /** @brief Start the RailCom cutout one-shot timer with the given period
         *  in microseconds. NULL = no RailCom cutout timing. */
    void (*railcom_timer_start)(uint16_t period_usec);

        /** @brief Stop the RailCom cutout one-shot timer. */
    void (*railcom_timer_stop)(void);

        /** @brief Cutout DELAY duration before tristating the H-bridge at
         *  T_CS = 26us after the packet end bit. 0 = use spec default. */
    uint16_t railcom_cutout_start_delay_us;

        /** @brief Cutout SETTLING duration before enabling UART Rx at
         *  T_TS1 = 80us (cumulative). 0 = use spec default. */
    uint16_t railcom_uart_rx_delay_us;

        /** @brief Cutout CH1 window before disabling UART Rx at
         *  T_TC1 = 177us (cumulative). 0 = use spec default. */
    uint16_t railcom_ch1_window_us;

        /** @brief Cutout GAP between Ch1 and Ch2 before re-enabling UART Rx at
         *  T_TS2 = 193us (cumulative). 0 = use spec default. */
    uint16_t railcom_ch1_ch2_gap_us;

        /** @brief Cutout CH2 window before disabling UART Rx and restoring the
         *  H-bridge at T_CE = 454us (cumulative). 0 = use spec default. */
    uint16_t railcom_ch2_window_us;
#endif /* DCC_COMPILE_RAILCOM */

        /** @brief Main track output hardware. Runs the scheduler (normal DCC). */
    dcc_output_hw_t main_track;

        /** @brief Service track output hardware. Runs service mode (programming). */
    dcc_output_hw_t service_track;

    // =========================================================================
    // Command Station: OPTIONAL application callbacks (NULL = no notification)
    // =========================================================================

        /** @brief Packet fully transmitted on the wire. */
    void (*on_packet_sent)(const dcc_packet_t *packet);

#if defined(DCC_COMPILE_RAILCOM)
        /** @brief Accessory decoder SRQ detected in RailCom Ch1.
         *  The user should respond by sending a stop command
         *  (load_accessory_basic_stop or load_accessory_extended_stop)
         *  to collect the decoder's update. NULL = SRQ ignored. */
    void (*on_accessory_srq)(uint16_t address, bool is_extended);
#endif /* DCC_COMPILE_RAILCOM */

#endif /* DCC_COMPILE_COMMAND_STATION */

    // =========================================================================
    // Decoder: REQUIRED hardware drivers
    // =========================================================================

#ifdef DCC_COMPILE_DECODER

        /** @brief Read CV from persistent storage. Returns true on success. REQUIRED. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Write CV to persistent storage. Returns true on success. REQUIRED. */
    bool (*cv_write)(uint16_t cv_number, uint8_t value);

        /** @brief Reduce a CV29 write to the features this decoder actually supports. REQUIRED.
         *  On any CV29 write the library forces reserved bit 6, decodes the requested config,
         *  and calls this with a mutable @ref dcc_cv29_flags_t. Set to false every feature this
         *  product does NOT implement -- RailCom (railcom_enabled), analog conversion
         *  (power_source_conversion), speed table (speed_table_enabled), and so on. The library
         *  re-encodes whatever you leave set and stores that as CV29. Per S-9.2.2 an unsupported
         *  feature bit must never be settable, and only your app knows what it supports -- so
         *  clearing those bits is your responsibility, not the library's. */
    void (*cv29_apply_supported_features)(dcc_cv29_flags_t *flags);

        /** @brief Restore CVs to factory defaults. OPTIONAL (NULL = no reset).
         *  Invoked when a write command targets CV8 (read-only Manufacturer ID, S-9.2.2). */
    void (*factory_reset)(void);

        /** @brief Indexed CV access (CV257-512 window into the page selected by CV31/CV32).
         *  OPTIONAL (NULL = no indexed support). page = CV31:CV32, offset = cv# - 257. */
    bool (*cv_read_indexed)(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value);
    bool (*cv_write_indexed)(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t value);

    // =========================================================================
    // Decoder: NULL-optional hardware drivers
    // =========================================================================

#if defined(DCC_COMPILE_RAILCOM)
        /** @brief Set the RailCom Tx GPIO high or low. Library bit-bangs 4/8 encoded
         *  bytes at 4us intervals from ISR context. No UART hardware needed.
         *  NULL = no RailCom responses. */
    void (*railcom_tx_pin_set)(bool high);

        /** @brief Enable or disable the DCC edge-detect interrupt during RailCom cutout.
         *  Called with false at end bit before cutout, true after cutout ends. User may
         *  disable hardware interrupt or set a flag to skip. NULL = no blanking.
         *  Required if using RailCom. */
    void (*decoder_edge_irq_enable)(bool enabled);

        /** @brief App hook: a command addressed to this decoder was recognized before
         *  the XOR. Fill @p out for Channel 2 and return the reply status (DATA / ACK /
         *  BUSY / NACK / NONE). NULL = ADR only, no Channel 2. Must be fast/non-blocking. */
    dcc_railcom_reply_status_enum (*on_railcom_request)(const uint8_t *instruction,
            uint8_t instruction_count, dcc_railcom_response_t *out);
#endif /* DCC_COMPILE_RAILCOM */

    // =========================================================================
    // Decoder: Service mode hardware drivers
    // =========================================================================

        /** @brief Turn on the ACK current load. Library handles 6ms timing and
         *  calls stop_ack_pulse automatically from DccConfig_run().
         *  NULL = no ACK hardware. */
    void (*start_ack_pulse)(void);

        /** @brief Turn off the ACK current load. Called by the library after 6ms.
         *  NULL = no ACK hardware. */
    void (*stop_ack_pulse)(void);

    // =========================================================================
    // Decoder: OPTIONAL application callbacks (NULL = no notification)
    // =========================================================================

        /** @brief Speed command received. */
    void (*on_speed_command)(uint16_t address, uint8_t speed, bool direction, dcc_speed_mode_enum mode);

        /** @brief Emergency stop received for specific address. */
    void (*on_emergency_stop_command)(uint16_t address);

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

        /** @brief CV write command received on this decoder. */
    void (*on_cv_write_command)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV verify command received on this decoder. */
    void (*on_cv_verify_command)(uint16_t cv_number, uint8_t value, bool service_mode);

        /** @brief CV bit manipulation command received on this decoder. */
    void (*on_cv_bit_command)(uint16_t cv_number, uint8_t bit_position, bool bit_value, bool service_mode);

        /** @brief Consist control command received. */
    void (*on_consist_command)(uint16_t address, uint8_t consist_address, bool direction_normal);

        /** @brief Binary state short (1-127) command received. */
    void (*on_binary_state_short_command)(uint16_t address, uint8_t state_number, bool active);

        /** @brief Binary state long (1-32767) command received. */
    void (*on_binary_state_long_command)(uint16_t address, uint16_t state_number, bool active);

        /** @brief Analog function output command received. */
    void (*on_analog_function_command)(uint16_t address, uint8_t output_number, uint8_t value);

        /** @brief Decoder entered fail-safe mode (no valid packet timeout). */
    void (*on_failsafe_entered)(void);

        /** @brief Decoder exited fail-safe mode (valid packet received). */
    void (*on_failsafe_exited)(void);

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
     * Also handles 6ms ACK pulse timing when an ACK is in progress.
     */
extern void DccConfig_run(void);

#ifdef DCC_COMPILE_COMMAND_STATION

    // =========================================================================
    // Command Station ISR Entry Points
    // =========================================================================

    /**
     * @brief Shared fixed-period timer ISR entry point.
     * @details Call from the 58us shared timer ISR. Drives the tick_isr for
     *  both main track and service track bit encoders. Also samples current
     *  sense on the service track for ACK detection.
     */
extern void DccConfig_58us_timer_isr(void);

#if defined(DCC_COMPILE_RAILCOM)
    /**
     * @brief RailCom cutout one-shot timer ISR entry point.
     * @details Call from the RailCom one-shot timer ISR. Drives the cutout
     *  state machine through DELAY -> SETTLING -> CH1 -> GAP -> CH2 -> IDLE.
     */
extern void DccConfig_railcom_oneshot_timer_isr(void);

    /**
     * @brief Reconfigure the RailCom cutout per-state timing at runtime.
     * @details Overwrites the active cutout context's five period fields; a 0 in
     *  any field selects that field's spec default (DCC_RAILCOM_* in dcc_defines.h),
     *  matching DccConfig_initialize. State and interface are left untouched, so an
     *  in-flight cutout completes on its old timing and the new values take effect
     *  from the next cutout. Periods are in microseconds.
     */
extern void DccConfig_set_railcom_cutout_timing(uint16_t start_delay_us, uint16_t uart_rx_delay_us,
                                                uint16_t ch1_window_us, uint16_t ch1_ch2_gap_us,
                                                uint16_t ch2_window_us);

    /**
     * @brief Cancel an in-progress RailCom cutout, restoring the H-bridge.
     * @details Stops the cutout one-shot timer and, if the cutout was active
     *  (past DELAY), calls end_railcom_cutout to restore normal drive. No-op when
     *  idle. Safe to call from a same-or-lower-priority ISR than the cutout timer.
     */
extern void DccConfig_cancel_railcom_cutout(void);

    /**
     * @brief True while a RailCom cutout is in progress (state != IDLE).
     */
extern bool DccConfig_railcom_cutout_is_active(void);

#endif /* DCC_COMPILE_RAILCOM */

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
extern void DccConfig_decoder_edge_isr(uint32_t timestamp_usec);

#endif /* DCC_COMPILE_DECODER */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DCC_CONFIG__ */
