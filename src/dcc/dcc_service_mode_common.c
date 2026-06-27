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

    /**
     * @brief Dropout tolerance: bridge brief sub-threshold dips during an ACK.
     *
     * @details A real Basic ACK is the decoder pulsing a noisy load (motor):
     * commutation/PWM ripple can make the current-sense comparator chatter
     * below threshold for a sample or two mid-pulse. We bridge up to this many
     * CONSECUTIVE low samples into the run rather than ending it, so a noisy
     * 6 ms ACK is still measured as one ~6 ms span. Set the user value to 0 for
     * strict consecutive-high detection (the original behavior). Optional --
     * defaults to ~116 us (2 samples) if the user config does not define it.
     */
#ifndef USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116
#endif
#define DCC_ACK_DROPOUT_SAMPLES \
    (USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US / DCC_ONE_BIT_HALF_PERIOD_US)


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

    if (context->ack_detected) {

        return;

    }

    if (context->state != SERVICE_COMMON_STATE_COMMAND &&
        context->state != SERVICE_COMMON_STATE_RECOVERY) {

        return;

    }

    /* S-9.2.3 line 55: ignore current until the ACK scan window has opened
     * (the first command packets are blanked to mask mode-switch transients). */
    if (!context->ack_window_open) {

        return;

    }

    if (sense_value >= USER_DEFINED_DCC_ACK_THRESHOLD_MA) {

        /* Elevated. Resuming after a brief dropout absorbs the bridged low
         * samples into the run (we measure the SPAN of elevated current, not
         * strict-high time -- a noisy motor ACK dips below threshold but the
         * current is still elevated). Flag over-current if the span grows past
         * the upper bound (S-9.2.3 p.3) -- that is a fault, not an ACK. */
        if (!context->ack_overrun) {

            context->ack_high_count += (uint16_t)(context->ack_low_run + 1u);

            if (context->ack_high_count > DCC_ACK_MAX_SAMPLES) {

                context->ack_overrun = true;

            }

        }

        context->ack_low_run = 0;

    } else if (context->ack_high_count > 0) {

        /* Below threshold while inside a run: a candidate dropout. Bridge up to
         * DCC_ACK_DROPOUT_SAMPLES consecutive low samples; only a longer low run
         * is a real falling edge. */
        context->ack_low_run++;

        if (context->ack_low_run > DCC_ACK_DROPOUT_SAMPLES) {

            /* Real falling edge: accept as an ACK only if the span stayed in the
             * two-sided 5-7 ms window (>= MIN and not over-current). */
            if (!context->ack_overrun &&
                context->ack_high_count >= DCC_ACK_MIN_SAMPLES) {

                context->ack_detected = true;

            }

            context->ack_high_count = 0;
            context->ack_low_run = 0;
            context->ack_overrun = false;

        }

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

    if (context->state != SERVICE_COMMON_STATE_IDLE) {

        return;

    }

    context->in_service_mode = false;

}

bool DccServiceModeCommon_begin_operation(dcc_service_mode_common_context_t *context, const dcc_packet_t *command_packet, dcc_service_mode_step_callback_t on_step_complete, bool is_write_operation, uint8_t command_repeat, uint8_t recovery_count) {

    if (!context->in_service_mode) {

        return false;

    }

    if (context->state != SERVICE_COMMON_STATE_IDLE) {

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

    context->state = SERVICE_COMMON_STATE_RESET_PRE;

    return true;

}

#endif /* DCC_COMPILE_COMMAND_STATION */
