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
 * @date 07 Apr 2026
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

    const interface_dcc_service_mode_common_t *interface;
    volatile uint8_t state;     /**< service_common_state_enum cast to uint8_t */
    bool in_service_mode;
    dcc_packet_t command_packet;
    dcc_service_mode_step_callback_t step_callback;
    dcc_service_mode_result_t result;
    uint8_t packet_count;
    uint8_t retry_count;
    volatile bool ack_detected;
    uint16_t ack_high_count;
    volatile bool packet_complete_flag;
    bool first_packet_sent;
    bool is_write_operation;        /**< true = write (has recovery phase) */
    uint8_t recovery_packet_count;  /**< packets to send during recovery */

} dcc_service_mode_common_context_t;

    /**
     * @brief Initialize the service mode common module.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     * @param interface Pointer to populated @ref interface_dcc_service_mode_common_t struct.
     */
    extern void DccServiceModeCommon_initialize(dcc_service_mode_common_context_t *context, const interface_dcc_service_mode_common_t *interface);

    /**
     * @brief Main loop processing for service mode state machine.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     */
extern void DccServiceModeCommon_run(dcc_service_mode_common_context_t *context);

    /**
     * @brief Sample the current sense input from the timer ISR.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     * @param sense_value Raw reading from current sense hardware.
     */
    extern void DccServiceModeCommon_ack_sample(dcc_service_mode_common_context_t *context, uint16_t sense_value);

    /**
     * @brief Notify that the encoder has finished transmitting a packet.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     * @details Called from ISR context (via on_packet_complete callback).
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
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     * @return true if entered successfully.
     */
extern bool DccServiceModeCommon_enter(dcc_service_mode_common_context_t *context);

    /**
     * @brief Exit service mode.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     */
extern void DccServiceModeCommon_exit(dcc_service_mode_common_context_t *context);

    /**
     * @brief Start a single service mode operation.
     * @param context Pointer to @ref dcc_service_mode_common_context_t instance.
     * @param command_packet Pointer to @ref dcc_packet_t to send during the command phase.
     * @param on_step_complete Callback fired when the operation completes (@ref dcc_service_mode_step_callback_t).
     * @param is_write_operation true for write operations (longer recovery), false for verify.
     * @param recovery_count Number of recovery packets to send after the command phase.
     * @return true if operation started, false if busy or no current sense.
     */
    extern bool DccServiceModeCommon_begin_operation(dcc_service_mode_common_context_t *context, const dcc_packet_t *command_packet, dcc_service_mode_step_callback_t on_step_complete, bool is_write_operation, uint8_t recovery_count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_SERVICE_MODE_COMMON__ */
