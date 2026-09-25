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
 * @file dcc_service_mode_common.h
 * @brief Shared service mode infrastructure: ACK detection, reset packet
 * sequencing, and retry logic.
 *
 * @details Provides the low-level state machine that all service mode modules
 * (direct, paged, register, address) use to execute programming operations.
 * Each operation consists of: reset packets -> command packets -> ACK
 * detection -> post-reset packets. ACK is detected by ISR-rate sampling of
 * the current sense input via DccServiceModeCommon_ack_sample().
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_SERVICE_MODE_COMMON__
#define __DCC_SERVICE_MODE_COMMON__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Load a packet into the bit encoder for transmission. */
    void (*load_packet)(const dcc_packet_t *packet);

        /** @brief Check if bit encoder is idle (ready for next packet). */
    bool (*is_encoder_idle)(void);

} interface_dcc_service_mode_common_t;

    /** @brief Instance context for the service mode common module.
     *
     *  @details Holds all per-instance state that was formerly file-scope static.
     *  Allocate one of these per DCC output channel that needs service mode.
     */
typedef struct {

    const interface_dcc_service_mode_common_t *interface;   /**< Injected dependencies (packet loader, encoder idle check) */
    volatile uint8_t state;     /**< service_common_state_enum cast to uint8_t */
    bool in_service_mode;       /**< true between DccServiceModeCommon_enter() and DccServiceModeCommon_exit() */
    dcc_packet_t command_packet;    /**< Copy of the packet sent during the command phase */
    dcc_service_mode_step_callback_t step_callback;     /**< Fired once with result when the operation returns to idle */
    dcc_service_mode_result_enum result;    /**< Outcome reported to step_callback (SUCCESS or NO_ACK) */
    uint8_t packet_count;       /**< Packets sent so far in the current state */
    uint8_t retry_count;        /**< Full reset/command sequences retried so far (limit USER_DEFINED_DCC_SERVICE_MODE_RETRIES) */
    volatile bool ack_detected; /**< Latched by DccServiceModeCommon_ack_sample() once a valid ACK span closes */
    uint16_t ack_high_count;    /**< Length of the current elevated-current span, in ack_sample ticks */
    volatile bool ack_overrun;  /**< true once a high run exceeded DCC_ACK_MAX_SAMPLES (over-current, not an ACK) */
    volatile bool packet_complete_flag;     /**< Set from the ISR by DccServiceModeCommon_on_packet_complete(), consumed by DccServiceModeCommon_run() */
    bool first_packet_sent;     /**< true while a loaded packet is still being transmitted */
    bool is_write_operation;        /**< true = write (has recovery phase) */
    uint8_t recovery_packet_count;  /**< packets to send during recovery */
    uint8_t command_repeat_count;   /**< command packets to send (S-9.2.3 varies by mode) */
    bool ack_window_open;           /**< false until the ACK blanking window passes (S-9.2.3 line 55) */
    uint16_t ack_low_run;           /**< consecutive sub-threshold samples in the current run (dropout filter) */

} dcc_service_mode_common_context_t;

        /**
         * @brief Initialize the service mode common module.
         * @details Stores the interface and resets the state machine to idle with service mode inactive.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @param interface Pointer to populated @ref interface_dcc_service_mode_common_t struct.
         */
    extern void DccServiceModeCommon_initialize(dcc_service_mode_common_context_t *context, const interface_dcc_service_mode_common_t *interface);

        /**
         * @brief Main loop processing for service mode state machine.
         * @details Call repeatedly from the main loop. Does nothing unless service mode is active.
         * Waits for the previous packet to finish and the encoder to go idle, then advances the
         * reset -> command -> recovery -> post-reset sequence one packet per call and fires the
         * step callback when the operation returns to idle.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         */
    extern void DccServiceModeCommon_run(dcc_service_mode_common_context_t *context);

        /**
         * @brief Sample the current sense input from the timer ISR.
         * @details Call once per 58 us half-bit tick. Samples are ignored unless an operation is in
         * its command or recovery phase and the ACK blanking window (the first
         * DCC_SERVICE_MODE_ACK_BLANK_PACKETS command packets) has passed. A reading at or above
         * USER_DEFINED_DCC_ACK_THRESHOLD_MA extends the current elevated span; a low run longer than
         * the dropout tolerance closes the span, which counts as an ACK only if its length is within
         * the 5-7 ms window (USER_DEFINED_DCC_ACK_MIN_DURATION_US .. USER_DEFINED_DCC_ACK_MAX_DURATION_US).
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @param sense_value Raw reading from current sense hardware, in the same units as USER_DEFINED_DCC_ACK_THRESHOLD_MA.
         */
    extern void DccServiceModeCommon_ack_sample(dcc_service_mode_common_context_t *context, uint16_t sense_value);

        /**
         * @brief Notify that the encoder has finished transmitting a packet.
         * @details Called from ISR context (via on_packet_complete callback). Only sets a flag;
         * the state machine advances on the next DccServiceModeCommon_run() call.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         */
    extern void DccServiceModeCommon_on_packet_complete(dcc_service_mode_common_context_t *context);

        /**
         * @brief Check if the common module is idle (no operation in progress).
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @return true if idle and ready for a new operation.
         */
    extern bool DccServiceModeCommon_is_idle(const dcc_service_mode_common_context_t *context);

        /**
         * @brief Check if currently in service mode.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @return true if service mode is active.
         */
    extern bool DccServiceModeCommon_is_active(const dcc_service_mode_common_context_t *context);

        /**
         * @brief Enter service mode.
         * @details Marks service mode active so DccServiceModeCommon_run() will process operations.
         * Sends nothing on the track by itself.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @return Always true.
         */
    extern bool DccServiceModeCommon_enter(dcc_service_mode_common_context_t *context);

        /**
         * @brief Exit service mode.
         * @details Ignored while an operation is in progress; call again once
         * DccServiceModeCommon_is_idle() returns true.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         */
    extern void DccServiceModeCommon_exit(dcc_service_mode_common_context_t *context);

        /**
         * @brief Start a single service mode operation.
         * @details Copies the command packet and runs the S-9.2.3 sequence: reset packets, command
         * packets, recovery (reset) packets for writes only, then post-reset packets. A detected ACK
         * ends the command phase early. With no ACK the whole sequence is retried up to
         * USER_DEFINED_DCC_SERVICE_MODE_RETRIES times before on_step_complete reports
         * DCC_SERVICE_MODE_NO_ACK.
         * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
         * @param command_packet Pointer to @ref dcc_packet_t to send during the command phase.
         * @param on_step_complete Callback fired when the operation completes (@ref dcc_service_mode_step_callback_t).
         * @param is_write_operation true for write operations (adds the recovery phase), false for verify.
         * @param command_repeat Number of command packets to send (S-9.2.3 minimum varies by mode).
         * @param recovery_count Number of recovery packets to send after the command phase (writes only; ignored for verify).
         * @return true if the operation started, false if service mode is not active or an operation is already in progress.
         */
    extern bool DccServiceModeCommon_begin_operation(
                dcc_service_mode_common_context_t *context,
                const dcc_packet_t *command_packet,
                dcc_service_mode_step_callback_t on_step_complete,
                bool is_write_operation,
                uint8_t command_repeat,
                uint8_t recovery_count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_SERVICE_MODE_COMMON__ */
