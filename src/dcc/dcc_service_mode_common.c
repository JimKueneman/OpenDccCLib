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
 * @file dcc_service_mode_common.c
 * @brief Shared service mode infrastructure: ACK detection, reset packet
 * sequencing, and retry logic.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_service_mode_common.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#include <string.h>

// =============================================================================
// Internal types
// =============================================================================

    /** @brief Phase of the service mode operation sequence (stored in the context state field). */
typedef enum {

    DCC_SERVICE_COMMON_STATE_IDLE,          /**< No operation in progress */
    DCC_SERVICE_COMMON_STATE_RESET_PRE,     /**< Sending the DCC_SERVICE_MODE_RESET_PRE_COUNT leading reset packets */
    DCC_SERVICE_COMMON_STATE_COMMAND,       /**< Sending command packets while scanning for an ACK */
    DCC_SERVICE_COMMON_STATE_RECOVERY,      /**< Writes only: sending reset packets while still scanning for an ACK */
    DCC_SERVICE_COMMON_STATE_RESET_POST     /**< Sending the DCC_SERVICE_MODE_RESET_POST_COUNT trailing reset packets, then reporting the result */

} service_common_state_enum;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Build and load a reset packet with service mode preamble.
     *
     * @details Three DCC_RESET_BYTE bytes with DCC_PREAMBLE_BITS_SERVICE preamble bits, sent once (repeat_count 0).
     *
     * @param context Pointer to the service mode common context.
     */
static void _load_reset_packet(dcc_service_mode_common_context_t *context) {

    dcc_packet_t reset_packet;
    memset(&reset_packet, 0, sizeof(reset_packet));

    reset_packet.data[0] = DCC_RESET_BYTE;
    reset_packet.data[1] = DCC_RESET_BYTE;
    reset_packet.data[2] = DCC_RESET_BYTE;
    reset_packet.byte_count = 3;
    reset_packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    reset_packet.repeat_count = 0;

    context->interface->load_packet(&reset_packet);

}

    /**
     * @brief Minimum length of an elevated-current span, in ack_sample ticks, for a valid ACK.
     *
     * @details One less than the tick count of USER_DEFINED_DCC_ACK_MIN_DURATION_US so a span
     * that is exactly the minimum duration still qualifies. With the typical config this is 85
     * samples (~4930 us).
     */
#define DCC_ACK_MIN_SAMPLES \
    ((USER_DEFINED_DCC_ACK_MIN_DURATION_US / DCC_ONE_BIT_HALF_PERIOD_US) - 1)

    /**
     * @brief Maximum consecutive high samples for a valid ACK.
     *
     * @details S-9.2.3 p.2 defines a Basic ACK as >= 60 mA sustained for
     * 6 ms +/- 1 ms (a two-sided 5-7 ms window). A high run that lasts
     * longer than the upper bound is treated by S-9.2.3 p.3 as an
     * over-current / fault condition, NOT an ACK. We therefore reject any
     * run exceeding this many samples. Unlike DCC_ACK_MIN_SAMPLES (which
     * subtracts 1), the MAX uses the raw quotient as the inclusive upper
     * bound. With the typical config: MIN ~= 85, MAX ~= 120.
     */
#define DCC_ACK_MAX_SAMPLES \
    (USER_DEFINED_DCC_ACK_MAX_DURATION_US / DCC_ONE_BIT_HALF_PERIOD_US)

    /**
     * @brief Default ACK dropout tolerance in microseconds (2 samples). Optional user setting; 0 gives strict consecutive-high detection.
     */
#ifndef USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116
#endif

    /**
     * @brief Dropout tolerance: bridge brief sub-threshold dips during an ACK.
     *
     * @details A real Basic ACK is the decoder pulsing a noisy load (motor):
     * commutation/PWM ripple can make the current-sense comparator chatter
     * below threshold for a sample or two mid-pulse. We bridge up to this many
     * CONSECUTIVE low samples into the run rather than ending it, so a noisy
     * 6 ms ACK is still measured as one ~6 ms span. A low run longer than this
     * is a real falling edge and closes the span.
     */
#define DCC_ACK_DROPOUT_SAMPLES \
    (USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US / DCC_ONE_BIT_HALF_PERIOD_US)


// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the service mode common module.
     *
     * @details Stores the interface and resets the state machine to idle with service mode
     * inactive. The per-operation fields not cleared here are set by DccServiceModeCommon_begin_operation().
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @param interface Pointer to populated interface_dcc_service_mode_common_t struct.
     * @endverbatim
     */
void DccServiceModeCommon_initialize(dcc_service_mode_common_context_t *context, const interface_dcc_service_mode_common_t *interface) {

    context->interface = interface;
    context->state = DCC_SERVICE_COMMON_STATE_IDLE;
    context->in_service_mode = false;
    context->step_callback = NULL;
    context->packet_count = 0;
    context->retry_count = 0;
    context->ack_detected = false;
    context->ack_high_count = 0;
    context->ack_overrun = false;
    context->packet_complete_flag = false;
    context->first_packet_sent = false;
    context->is_write_operation = false;
    context->recovery_packet_count = 0;

}

    /**
     * @brief ACK sampler: current at or above threshold.
     *
     * @details Algorithm:
     * -# If the span is not already flagged over-current, grow it by this sample plus any
     *    bridged low samples (we measure the SPAN of elevated current, not strict-high time --
     *    a noisy motor ACK dips below threshold while the current is still elevated)
     * -# If the span now exceeds DCC_ACK_MAX_SAMPLES, flag over-current (S-9.2.3 p.3: a fault, not an ACK)
     * -# Clear the low-run counter
     *
     * @param context Pointer to the service mode common context.
     */
static void DCC_ISR_FUNC(_ack_sample_elevated)(dcc_service_mode_common_context_t *context) {

    if (!context->ack_overrun) {

        context->ack_high_count += (uint16_t)(context->ack_low_run + 1u);

        if (context->ack_high_count > DCC_ACK_MAX_SAMPLES) {

            context->ack_overrun = true;

        }

    }

    context->ack_low_run = 0;

}

    /**
     * @brief ACK sampler: current below threshold while inside a span (candidate dropout).
     *
     * @details Algorithm:
     * -# Count the low sample
     * -# Up to DCC_ACK_DROPOUT_SAMPLES consecutive low samples are bridged; nothing else happens
     * -# A longer low run is a real falling edge: latch ack_detected if the span was not
     *    over-current and reached DCC_ACK_MIN_SAMPLES, then clear the span, the low run and
     *    the over-current flag ready for the next span
     *
     * @param context Pointer to the service mode common context.
     */
static void DCC_ISR_FUNC(_ack_sample_dropout)(dcc_service_mode_common_context_t *context) {

    context->ack_low_run++;

    if (context->ack_low_run > DCC_ACK_DROPOUT_SAMPLES) {

        /* Real falling edge: accept as an ACK only if the span stayed in the
         * two-sided 5-7 ms window (>= MIN and not over-current). */
        if (!context->ack_overrun && context->ack_high_count >= DCC_ACK_MIN_SAMPLES) {

            context->ack_detected = true;

        }

        context->ack_high_count = 0;
        context->ack_low_run = 0;
        context->ack_overrun = false;

    }

}

    /**
     * @brief Sample the current sense input from the timer ISR.
     *
     * @details Algorithm:
     * -# Return once an ACK has already been latched for this operation
     * -# Return unless the state is COMMAND or RECOVERY
     * -# Return while the ACK window is blanked (S-9.2.3 line 55: the first
     *    DCC_SERVICE_MODE_ACK_BLANK_PACKETS command packets mask the decoder mode-switch transient)
     * -# Reading >= USER_DEFINED_DCC_ACK_THRESHOLD_MA: extend the span (_ack_sample_elevated)
     * -# Reading below threshold inside a span: treat as a dropout (_ack_sample_dropout);
     *    below threshold with no span open: ignore
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @param sense_value Raw reading from current sense hardware.
     * @endverbatim
     */
void DCC_ISR_FUNC(DccServiceModeCommon_ack_sample)(dcc_service_mode_common_context_t *context, uint16_t sense_value) {

    if (context->ack_detected) {

        return;

    }

    if (context->state != DCC_SERVICE_COMMON_STATE_COMMAND && context->state != DCC_SERVICE_COMMON_STATE_RECOVERY) {

        return;

    }

    /* S-9.2.3 line 55: ignore current until the ACK scan window has opened
     * (the first command packets are blanked to mask mode-switch transients). */
    if (!context->ack_window_open) {

        return;

    }

    if (sense_value >= USER_DEFINED_DCC_ACK_THRESHOLD_MA) {

        _ack_sample_elevated(context);

    } else if (context->ack_high_count > 0) {

        _ack_sample_dropout(context);

    }

}

    /**
     * @brief Notify that the encoder has finished transmitting a packet.
     *
     * @details ISR context. Only sets packet_complete_flag; DccServiceModeCommon_run() consumes it.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     */
void DCC_ISR_FUNC(DccServiceModeCommon_on_packet_complete)(dcc_service_mode_common_context_t *context) {

    context->packet_complete_flag = true;

}

    /**
     * @brief Retry the current service mode operation or declare failure.
     *
     * @details While retry_count is below USER_DEFINED_DCC_SERVICE_MODE_RETRIES, increments it
     * and restarts from RESET_PRE. Otherwise sets the result to DCC_SERVICE_MODE_NO_ACK and
     * moves to RESET_POST so the trailing reset packets are still sent before the callback fires.
     *
     * @param context Pointer to the service mode common context.
     */
static void _retry_or_fail(dcc_service_mode_common_context_t *context) {

    if (context->retry_count < USER_DEFINED_DCC_SERVICE_MODE_RETRIES) {

        context->retry_count++;
        context->state = DCC_SERVICE_COMMON_STATE_RESET_PRE;
        context->packet_count = 0;

    } else {

        context->result = DCC_SERVICE_MODE_NO_ACK;
        context->state = DCC_SERVICE_COMMON_STATE_RESET_POST;
        context->packet_count = 0;

    }

}

    /**
     * @brief Run the COMMAND state of the service mode state machine.
     *
     * @details Algorithm:
     * -# ACK latched: set result SUCCESS, skip the remaining command packets and go to RESET_POST
     * -# Fewer than command_repeat_count packets sent: load the command packet again; once more
     *    than DCC_SERVICE_MODE_ACK_BLANK_PACKETS have been loaded, open the ACK window
     *    (it stays open through the rest of COMMAND and all of RECOVERY)
     * -# Command packets exhausted on a write: go to RECOVERY
     * -# Command packets exhausted on a verify: retry or fail (no recovery phase)
     *
     * @param context Pointer to the service mode common context.
     */
static void _run_command_state(dcc_service_mode_common_context_t *context) {

    if (context->ack_detected) {

        /* ACK confirmed — skip remaining command packets and
         * proceed directly to post-reset / recovery. */
        context->result = DCC_SERVICE_MODE_SUCCESS;
        context->state = DCC_SERVICE_COMMON_STATE_RESET_POST;
        context->packet_count = 0;

    } else if (context->packet_count < context->command_repeat_count) {

        context->interface->load_packet(&context->command_packet);
        context->first_packet_sent = true;
        context->packet_count++;

        /* S-9.2.3 line 55: open the ACK scan window once the early command
         * packets have passed (blank the decoder mode-switch transient). Stays
         * open through the rest of COMMAND and all of RECOVERY. */
        if (context->packet_count > DCC_SERVICE_MODE_ACK_BLANK_PACKETS) {

            context->ack_window_open = true;

        }

    } else if (context->is_write_operation) {

        /* Write operations enter recovery phase (S-9.2.3
         * Decoder-Recovery-Time) -- continue sending reset packets
         * while still scanning for ACK. */
        context->state = DCC_SERVICE_COMMON_STATE_RECOVERY;
        context->packet_count = 0;

    } else {

        /* Verify operations have no recovery phase -- retry or fail. */
        _retry_or_fail(context);

    }

}

    /**
     * @brief Run the RECOVERY state of the service mode state machine.
     *
     * @details Algorithm:
     * -# ACK latched: set result SUCCESS and go to RESET_POST
     * -# Fewer than recovery_packet_count reset packets sent: load another (S-9.2.3
     *    Decoder-Recovery-Time, ACK scanning continues)
     * -# Recovery exhausted with no ACK: retry or fail
     *
     * @param context Pointer to the service mode common context.
     */
static void _run_recovery_state(dcc_service_mode_common_context_t *context) {

    if (context->ack_detected) {

        context->result = DCC_SERVICE_MODE_SUCCESS;
        context->state = DCC_SERVICE_COMMON_STATE_RESET_POST;
        context->packet_count = 0;

    } else if (context->packet_count < context->recovery_packet_count) {

        _load_reset_packet(context);
        context->first_packet_sent = true;
        context->packet_count++;

    } else {

        /* Recovery exhausted with no ACK -- retry or fail. */
        _retry_or_fail(context);

    }

}

    /**
     * @brief Run the RESET_POST state of the service mode state machine.
     *
     * @details Sends DCC_SERVICE_MODE_RESET_POST_COUNT reset packets, then returns to IDLE and
     * fires the step callback (if set) with the stored result.
     *
     * @param context Pointer to the service mode common context.
     */
static void _run_reset_post_state(dcc_service_mode_common_context_t *context) {

    if (context->packet_count < DCC_SERVICE_MODE_RESET_POST_COUNT) {

        _load_reset_packet(context);
        context->first_packet_sent = true;
        context->packet_count++;

    } else {

        context->state = DCC_SERVICE_COMMON_STATE_IDLE;

        if (context->step_callback) {

            context->step_callback(context->result);

        }

    }

}

    /**
     * @brief Main loop processing for service mode state machine.
     *
     * @details Algorithm:
     * -# Return unless service mode is active
     * -# If a packet has been loaded, return until the ISR has flagged it complete, then clear both flags
     * -# Return unless the encoder reports idle (belt-and-suspenders)
     * -# RESET_PRE: load a reset packet until DCC_SERVICE_MODE_RESET_PRE_COUNT have been sent, then
     *    clear the ACK detector state and enter COMMAND
     * -# COMMAND, RECOVERY, RESET_POST: delegate to the per-state helper
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     */
void DccServiceModeCommon_run(dcc_service_mode_common_context_t *context) {

    if (!context->in_service_mode) {

        return;

    }

    /* Wait for encoder to finish the previous packet before advancing. */
    if (context->first_packet_sent) {

        if (!context->packet_complete_flag) {

            return;

        }
        context->packet_complete_flag = false;
        context->first_packet_sent = false;

    }

    /* Belt-and-suspenders: verify encoder is actually idle */
    if (!context->interface->is_encoder_idle()) {

        return;

    }

    if (context->state == DCC_SERVICE_COMMON_STATE_RESET_PRE) {

        if (context->packet_count < DCC_SERVICE_MODE_RESET_PRE_COUNT) {

            _load_reset_packet(context);
            context->first_packet_sent = true;
            context->packet_count++;

        } else {

            context->packet_count = 0;
            context->ack_detected = false;
            context->ack_high_count = 0;
            context->ack_overrun = false;
            context->state = DCC_SERVICE_COMMON_STATE_COMMAND;

        }

    } else if (context->state == DCC_SERVICE_COMMON_STATE_COMMAND) {

        _run_command_state(context);

    } else if (context->state == DCC_SERVICE_COMMON_STATE_RECOVERY) {

        _run_recovery_state(context);

    } else if (context->state == DCC_SERVICE_COMMON_STATE_RESET_POST) {

        _run_reset_post_state(context);

    }

}

    /**
     * @brief Check if the common module is idle (no operation in progress).
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     *
     * @return true if idle and ready for a new operation.
     */
bool DccServiceModeCommon_is_idle(const dcc_service_mode_common_context_t *context) {

    return (context->state == DCC_SERVICE_COMMON_STATE_IDLE);

}

    /**
     * @brief Check if currently in service mode.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     *
     * @return true if service mode is active.
     */
bool DccServiceModeCommon_is_active(const dcc_service_mode_common_context_t *context) {

    return context->in_service_mode;

}

    /**
     * @brief Enter service mode.
     *
     * @details Sets in_service_mode; sends nothing on the track by itself.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     *
     * @return Always true.
     */
bool DccServiceModeCommon_enter(dcc_service_mode_common_context_t *context) {

    context->in_service_mode = true;
    return true;

}

    /**
     * @brief Exit service mode.
     *
     * @details Ignored while the state is not IDLE, so an operation in progress always runs to completion.
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @endverbatim
     */
void DccServiceModeCommon_exit(dcc_service_mode_common_context_t *context) {

    if (context->state != DCC_SERVICE_COMMON_STATE_IDLE) {

        return;

    }

    context->in_service_mode = false;

}

    /**
     * @brief Start a single service mode operation.
     *
     * @details Algorithm:
     * -# Refuse (return false) when service mode is not active
     * -# Refuse (return false) when the state is not IDLE
     * -# Copy the command packet into the context and latch the callback, write flag, repeat and recovery counts
     * -# Reset the counters, the ACK detector (window closed, no span) and the result to NO_ACK
     * -# Enter RESET_PRE; DccServiceModeCommon_run() sends the packets from here on
     *
     * @verbatim
     * @param context Pointer to dcc_service_mode_common_context_t instance.
     * @param command_packet Pointer to dcc_packet_t to send during the command phase.
     * @param on_step_complete Callback fired when the operation completes (dcc_service_mode_step_callback_t).
     * @param is_write_operation true for write operations (adds the recovery phase), false for verify.
     * @param command_repeat Number of command packets to send.
     * @param recovery_count Number of recovery packets to send after the command phase (writes only).
     * @endverbatim
     *
     * @return true if the operation started, false if service mode is not active or an operation is already in progress.
     */
bool DccServiceModeCommon_begin_operation(
            dcc_service_mode_common_context_t *context,
            const dcc_packet_t *command_packet,
            dcc_service_mode_step_callback_t on_step_complete,
            bool is_write_operation,
            uint8_t command_repeat,
            uint8_t recovery_count) {

    if (!context->in_service_mode) {

        return false;

    }

    if (context->state != DCC_SERVICE_COMMON_STATE_IDLE) {

        return false;

    }

    memcpy(&context->command_packet, command_packet, sizeof(dcc_packet_t));
    context->step_callback = on_step_complete;
    context->result = DCC_SERVICE_MODE_NO_ACK;
    context->packet_count = 0;
    context->retry_count = 0;
    context->ack_detected = false;
    context->ack_high_count = 0;
    context->ack_overrun = false;
    context->packet_complete_flag = false;
    context->first_packet_sent = false;
    context->is_write_operation = is_write_operation;
    context->recovery_packet_count = recovery_count;
    context->command_repeat_count = command_repeat;
    context->ack_window_open = false;
    context->ack_low_run = 0;

    context->state = DCC_SERVICE_COMMON_STATE_RESET_PRE;

    return true;

}

#endif /* DCC_COMPILE_COMMAND_STATION */
