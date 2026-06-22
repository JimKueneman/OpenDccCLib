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
 * @date 07 Apr 2026
 */

#include "dcc_service_mode_common.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#include <string.h>

// =============================================================================
// Internal types
// =============================================================================

typedef enum {

    SERVICE_COMMON_STATE_IDLE,
    SERVICE_COMMON_STATE_RESET_PRE,
    SERVICE_COMMON_STATE_COMMAND,
    SERVICE_COMMON_STATE_RECOVERY,
    SERVICE_COMMON_STATE_RESET_POST

} service_common_state_enum;

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Build and load a reset packet with service mode preamble.
     */
static void _load_reset_packet(dcc_service_mode_common_context_t *context) {

    dcc_packet_t reset_packet;

    reset_packet.data[0] = DCC_RESET_BYTE;
    reset_packet.data[1] = DCC_RESET_BYTE;
    reset_packet.data[2] = DCC_RESET_BYTE;
    reset_packet.byte_count = 3;
    reset_packet.preamble_bits = DCC_PREAMBLE_BITS_SERVICE;
    reset_packet.repeat_count = 0;

    context->interface->load_packet(&reset_packet);

}

    /**
     * @brief Minimum consecutive high samples for a valid ACK.
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


// =============================================================================
// Public API
// =============================================================================

void DccServiceModeCommon_initialize(dcc_service_mode_common_context_t *context, const interface_dcc_service_mode_common_t *interface) {

    context->interface = interface;
    context->state = SERVICE_COMMON_STATE_IDLE;
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

void DccServiceModeCommon_ack_sample(dcc_service_mode_common_context_t *context, uint16_t sense_value) {

    if (context->ack_detected)
        return;

    if (context->state != SERVICE_COMMON_STATE_COMMAND &&
        context->state != SERVICE_COMMON_STATE_RECOVERY) {

        return;

    }

    if (sense_value >= USER_DEFINED_DCC_ACK_THRESHOLD_MA) {

        /* Still high: defer the decision. Count the run; if it grows past
         * the upper bound, flag it as an over-current run (S-9.2.3 p.3),
         * not an ACK, and stop counting this run. */
        if (!context->ack_overrun) {

            context->ack_high_count++;

            if (context->ack_high_count > DCC_ACK_MAX_SAMPLES) {

                context->ack_overrun = true;

            }

        }

    } else {

        /* Falling edge: the high run just ended. Accept it as an ACK only
         * if it stayed within the two-sided 5-7 ms window (S-9.2.3 p.2):
         * long enough (>= MIN) and not flagged as over-current (<= MAX). */
        if (!context->ack_overrun &&
            context->ack_high_count >= DCC_ACK_MIN_SAMPLES) {

            context->ack_detected = true;

        }

        context->ack_high_count = 0;
        context->ack_overrun = false;

    }

}

void DccServiceModeCommon_on_packet_complete(dcc_service_mode_common_context_t *context) {

    context->packet_complete_flag = true;

}

    /**
     * @brief Retry the current service mode operation or declare failure.
     */
static void _retry_or_fail(dcc_service_mode_common_context_t *context) {

    if (context->retry_count < USER_DEFINED_DCC_SERVICE_MODE_RETRIES) {

        context->retry_count++;
        context->state = SERVICE_COMMON_STATE_RESET_PRE;
        context->packet_count = 0;

    } else {

        context->result = DCC_SERVICE_MODE_NO_ACK;
        context->state = SERVICE_COMMON_STATE_RESET_POST;
        context->packet_count = 0;

    }

}

    /**
     * @brief Run the COMMAND state of the service mode state machine.
     */
static void _run_command_state(dcc_service_mode_common_context_t *context) {

    if (context->ack_detected) {

        /* ACK confirmed — skip remaining command packets and
         * proceed directly to post-reset / recovery. */
        context->result = DCC_SERVICE_MODE_SUCCESS;
        context->state = SERVICE_COMMON_STATE_RESET_POST;
        context->packet_count = 0;

    } else if (context->packet_count < DCC_SERVICE_MODE_COMMAND_REPEAT) {

        context->interface->load_packet(&context->command_packet);
        context->first_packet_sent = true;
        context->packet_count++;

    } else if (context->is_write_operation) {

        /* Write operations enter recovery phase (S-9.2.3
         * Decoder-Recovery-Time) -- continue sending reset packets
         * while still scanning for ACK. */
        context->state = SERVICE_COMMON_STATE_RECOVERY;
        context->packet_count = 0;

    } else {

        /* Verify operations have no recovery phase -- retry or fail. */
        _retry_or_fail(context);

    }

}

    /**
     * @brief Run the RECOVERY state of the service mode state machine.
     */
static void _run_recovery_state(dcc_service_mode_common_context_t *context) {

    if (context->ack_detected) {

        context->result = DCC_SERVICE_MODE_SUCCESS;
        context->state = SERVICE_COMMON_STATE_RESET_POST;
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
     */
static void _run_reset_post_state(dcc_service_mode_common_context_t *context) {

    if (context->packet_count < DCC_SERVICE_MODE_RESET_POST_COUNT) {

        _load_reset_packet(context);
        context->first_packet_sent = true;
        context->packet_count++;

    } else {

        context->state = SERVICE_COMMON_STATE_IDLE;

        if (context->step_callback) {

            context->step_callback(context->result);

        }

    }

}

void DccServiceModeCommon_run(dcc_service_mode_common_context_t *context) {

    if (!context->in_service_mode)
        return;

    /* Wait for encoder to finish the previous packet before advancing. */
    if (context->first_packet_sent) {

        if (!context->packet_complete_flag) {

            return;

        }
        context->packet_complete_flag = false;
        context->first_packet_sent = false;

    }

    /* Belt-and-suspenders: verify encoder is actually idle */
    if (!context->interface->is_encoder_idle())
        return;

    if (context->state == SERVICE_COMMON_STATE_RESET_PRE) {

        if (context->packet_count < DCC_SERVICE_MODE_RESET_PRE_COUNT) {

            _load_reset_packet(context);
            context->first_packet_sent = true;
            context->packet_count++;

        } else {

            context->packet_count = 0;
            context->ack_detected = false;
            context->ack_high_count = 0;
            context->ack_overrun = false;
            context->state = SERVICE_COMMON_STATE_COMMAND;

        }

    } else if (context->state == SERVICE_COMMON_STATE_COMMAND) {

        _run_command_state(context);

    } else if (context->state == SERVICE_COMMON_STATE_RECOVERY) {

        _run_recovery_state(context);

    } else if (context->state == SERVICE_COMMON_STATE_RESET_POST) {

        _run_reset_post_state(context);

    }

}

bool DccServiceModeCommon_is_idle(const dcc_service_mode_common_context_t *context) {

    return (context->state == SERVICE_COMMON_STATE_IDLE);

}

bool DccServiceModeCommon_is_active(const dcc_service_mode_common_context_t *context) {

    return context->in_service_mode;

}

bool DccServiceModeCommon_enter(dcc_service_mode_common_context_t *context) {

    context->in_service_mode = true;
    return true;

}

void DccServiceModeCommon_exit(dcc_service_mode_common_context_t *context) {

    if (context->state != SERVICE_COMMON_STATE_IDLE)
        return;

    context->in_service_mode = false;

}

bool DccServiceModeCommon_begin_operation(dcc_service_mode_common_context_t *context, const dcc_packet_t *command_packet, dcc_service_mode_step_callback_t on_step_complete, bool is_write_operation, uint8_t recovery_count) {

    if (!context->in_service_mode)
        return false;

    if (context->state != SERVICE_COMMON_STATE_IDLE)
        return false;

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

    context->state = SERVICE_COMMON_STATE_RESET_PRE;

    return true;

}

#endif /* DCC_COMPILE_COMMAND_STATION */
