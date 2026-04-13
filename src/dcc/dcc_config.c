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
 * @file dcc_config.c
 * @brief Wiring module: builds interface structs and initializes all modules.
 *
 * @details This is the only .c file that includes all module headers. It
 * creates two independent DCC output channels — main track (scheduler) and
 * service track (service mode) — each with its own bit encoder instance.
 * Both channels run concurrently in DccConfig_run(). Application-layer
 * modules (dcc_application_main_track, dcc_application_service_track) are
 * wired here and own the user-facing API.
 *
 * @author Jim Kueneman
 * @date 11 Apr 2026
 */

#include "dcc_config.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION
#include "dcc_bit_encoder.h"
#include "dcc_packet_encoder.h"
#include "dcc_scheduler.h"
#include "dcc_service_mode_common.h"
#include "dcc_railcom_decoder.h"
#include "dcc_railcom_cutout.h"
#include "dcc_application_main_track.h"
#include "dcc_application_service_track.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
#include "dcc_service_mode_direct.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
#include "dcc_service_mode_paged.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
#include "dcc_service_mode_register.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
#include "dcc_service_mode_address.h"
#endif

#ifdef DCC_COMPILE_DECODER
#include "dcc_bit_decoder.h"
#include "dcc_packet_decoder.h"
#include "dcc_cv_storage.h"
#include "dcc_railcom_encoder.h"
#endif

    /** @brief Stored pointer to the user's configuration struct */
static const dcc_config_t *_configuration_pointer = (void *)0;

#ifdef DCC_COMPILE_COMMAND_STATION

/* =========================================================================
 * Main track channel: encoder + scheduler + RailCom
 * ========================================================================= */

    /** @brief Main track bit encoder context */
static dcc_bit_encoder_context_t _main_encoder_context;

    /** @brief Main track bit encoder interface */
static interface_dcc_bit_encoder_t _main_encoder_interface;

    /** @brief Main track scheduler context */
static dcc_scheduler_context_t _main_scheduler_context;

    /** @brief Main track scheduler interface */
static interface_dcc_scheduler_t _main_scheduler_interface;

    /** @brief Main track RailCom decoder context */
static dcc_railcom_decoder_context_t _main_railcom_context;

    /** @brief Main track RailCom decoder interface */
static interface_dcc_railcom_decoder_t _main_railcom_interface;

    /** @brief Main track application layer interface */
static interface_dcc_application_main_track_t _main_application_interface;

/* =========================================================================
 * Service track channel: encoder + service mode common + sub-modules
 * ========================================================================= */

    /** @brief Service track bit encoder context */
static dcc_bit_encoder_context_t _service_encoder_context;

    /** @brief Service track bit encoder interface */
static interface_dcc_bit_encoder_t _service_encoder_interface;

    /** @brief Service track service mode common context */
static dcc_service_mode_common_context_t _service_common_context;

    /** @brief Service track service mode common interface */
static interface_dcc_service_mode_common_t _service_common_interface;

    /** @brief Service track application layer interface */
static interface_dcc_application_service_track_t _service_application_interface;

/* =========================================================================
 * RailCom cutout: one-shot timer state machine
 * ========================================================================= */

    /** @brief RailCom cutout context (main track only for now) */
static dcc_railcom_cutout_context_t _railcom_cutout_context;

    /** @brief RailCom cutout interface */
static interface_dcc_railcom_cutout_t _railcom_cutout_interface;

/* =========================================================================
 * Shared timer reference count
 * ========================================================================= */

    /** @brief Number of channels currently using the shared timer.
     *  Timer starts when count goes 0 -> 1, stops when 1 -> 0. */
static uint8_t _shared_timer_ref_count = 0;

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /** @brief Direct service mode context */
static dcc_service_mode_direct_context_t _service_direct_context;

    /** @brief Direct service mode interface */
static interface_dcc_service_mode_direct_t _service_direct_interface;

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /** @brief Paged service mode context */
static dcc_service_mode_paged_context_t _service_paged_context;

    /** @brief Paged service mode interface */
static interface_dcc_service_mode_paged_t _service_paged_interface;

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /** @brief Register service mode context */
static dcc_service_mode_register_context_t _service_register_context;

    /** @brief Register service mode interface */
static interface_dcc_service_mode_register_t _service_register_interface;

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /** @brief Address-only service mode context */
static dcc_service_mode_address_context_t _service_address_context;

    /** @brief Address-only service mode interface */
static interface_dcc_service_mode_address_t _service_address_interface;

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#ifdef DCC_COMPILE_DECODER

    /** @brief Interface struct for the bit decoder module */
static interface_dcc_bit_decoder_t _bit_decoder_interface;

    /** @brief Interface struct for the packet decoder module */
static interface_dcc_packet_decoder_t _packet_decoder_interface;

    /** @brief Interface struct for the CV storage module */
static interface_dcc_cv_storage_t _cv_storage_interface;

    /** @brief Interface struct for the RailCom encoder module */
static interface_dcc_railcom_encoder_t _railcom_encoder_interface;

#endif /* DCC_COMPILE_DECODER */

#ifdef DCC_COMPILE_COMMAND_STATION

/* =========================================================================
 * Main track wrapper functions
 *
 * These capture the main track context pointers so interface structs
 * (which use fixed callback signatures) can reach the correct instance.
 * ========================================================================= */

static void _main_on_packet_complete(void) {

    DccScheduler_on_packet_complete(&_main_scheduler_context);

}

static void _main_load_packet(const dcc_packet_t *packet) {

    DccBitEncoder_load_packet(&_main_encoder_context, packet);

}

static bool _main_is_encoder_idle(void) {

    return DccBitEncoder_is_idle(&_main_encoder_context);

}

static void _main_encoder_start(void) {

    DccBitEncoder_start(&_main_encoder_context);

}

static void _main_encoder_stop(void) {

    DccBitEncoder_stop(&_main_encoder_context);

}

static bool _main_scheduler_insert(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {

    return DccScheduler_insert(&_main_scheduler_context, packet, address, tag, priority, auto_refresh);

}

static void _main_scheduler_remove_address(dcc_address_t address) {

    DccScheduler_remove_address(&_main_scheduler_context, address);

}

static void _main_scheduler_clear(void) {

    DccScheduler_clear(&_main_scheduler_context);

}

/* =========================================================================
 * Service track wrapper functions
 * ========================================================================= */

static void _service_on_packet_complete(void) {

    DccServiceModeCommon_on_packet_complete(&_service_common_context);

}

static void _service_load_packet(const dcc_packet_t *packet) {

    if (_configuration_pointer && _configuration_pointer->on_packet_sent) {

        _configuration_pointer->on_packet_sent(packet);

    }

    DccBitEncoder_load_packet(&_service_encoder_context, packet);

}

static bool _service_is_encoder_idle(void) {

    return DccBitEncoder_is_idle(&_service_encoder_context);

}

static void _service_encoder_start(void) {

    DccBitEncoder_start(&_service_encoder_context);

}

static void _service_encoder_stop(void) {

    DccBitEncoder_stop(&_service_encoder_context);

}

static bool _service_begin_operation(const dcc_packet_t *packet, dcc_service_mode_step_callback_t callback, bool is_write_operation, uint8_t recovery_count) {

    return DccServiceModeCommon_begin_operation(&_service_common_context, packet, callback, is_write_operation, recovery_count);

}

static bool _service_is_common_idle(void) {

    return DccServiceModeCommon_is_idle(&_service_common_context);

}

static bool _service_enter_service_mode(void) {

    return DccServiceModeCommon_enter(&_service_common_context);

}

static void _service_exit_service_mode(void) {

    DccServiceModeCommon_exit(&_service_common_context);

}

static bool _service_is_service_mode_active(void) {

    return DccServiceModeCommon_is_active(&_service_common_context);

}

/* =========================================================================
 * Shared timer reference-counted start/stop wrappers
 *
 * Both application layers call timer_start/timer_stop through these.
 * The actual hardware timer starts on the first acquire and stops on
 * the last release.
 * ========================================================================= */

static void _shared_timer_acquire(uint16_t period_usec) {

    _shared_timer_ref_count++;

    if (_shared_timer_ref_count == 1) {

        if (_configuration_pointer->shared_timer_start) {

            _configuration_pointer->shared_timer_start(period_usec);

        }

    }

}

static void _shared_timer_release(void) {

    if (_shared_timer_ref_count > 0) {

        _shared_timer_ref_count--;

    }

    if (_shared_timer_ref_count == 0) {

        if (_configuration_pointer->shared_timer_stop) {

            _configuration_pointer->shared_timer_stop();

        }

    }

}

/* =========================================================================
 * RailCom cutout bridge: on_cutout_complete sets the encoder's flag
 * ========================================================================= */

static void _railcom_cutout_begin_wrapper(void) {

    DccRailcomCutout_begin(&_railcom_cutout_context);

}

static void _railcom_on_cutout_complete(void) {

    _main_encoder_context.cutout_complete = true;

}

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

static bool _service_direct_write_byte(uint16_t cv_number, uint8_t value) {

    return DccServiceModeDirect_write_byte(&_service_direct_context, cv_number, value);

}

static bool _service_direct_verify_byte(uint16_t cv_number, uint8_t value) {

    return DccServiceModeDirect_verify_byte(&_service_direct_context, cv_number, value);

}

static bool _service_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    return DccServiceModeDirect_write_bit(&_service_direct_context, cv_number, bit_position, bit_value);

}

static bool _service_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    return DccServiceModeDirect_verify_bit(&_service_direct_context, cv_number, bit_position, bit_value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

static bool _service_paged_write(uint16_t cv_number, uint8_t value) {

    return DccServiceModePaged_write(&_service_paged_context, cv_number, value);

}

static bool _service_paged_verify(uint16_t cv_number, uint8_t value) {

    return DccServiceModePaged_verify(&_service_paged_context, cv_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

static bool _service_register_write(uint8_t register_number, uint8_t value) {

    return DccServiceModeRegister_write(&_service_register_context, register_number, value);

}

static bool _service_register_verify(uint8_t register_number, uint8_t value) {

    return DccServiceModeRegister_verify(&_service_register_context, register_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

static bool _service_address_write(uint8_t address) {

    return DccServiceModeAddress_write(&_service_address_context, address);

}

static bool _service_address_verify(uint8_t address) {

    return DccServiceModeAddress_verify(&_service_address_context, address);

}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#endif /* DCC_COMPILE_COMMAND_STATION */

void DccConfig_initialize(const dcc_config_t *config) {

    _configuration_pointer = config;

    if (!config) {

        return;

    }

#ifdef DCC_COMPILE_COMMAND_STATION

    /* =================================================================
     * Main track channel: encoder + scheduler + RailCom
     * ================================================================= */

    _shared_timer_ref_count = 0;

    /* Wire main track bit encoder */
    _main_encoder_interface.timer_set_period = config->main_track.timer_set_period;
    _main_encoder_interface.pin_toggle = config->main_track.pin_toggle;
    _main_encoder_interface.railcom_cutout_begin = (void *)0;
    _main_encoder_interface.railcom_cutout_end = (void *)0;
    _main_encoder_interface.on_packet_complete = &_main_on_packet_complete;

    if (config->main_track.railcom) {

        if (config->railcom_timer_start) {

            /* New architecture: one-shot timer cutout module.
             * The tick ISR enters RAILCOM_CUTOUT state and calls begin().
             * The cutout module signals completion via cutout_complete flag.
             * railcom_cutout_end is not used — the cutout module handles it. */
            _main_encoder_interface.railcom_cutout_begin = &_railcom_cutout_begin_wrapper;
            _main_encoder_interface.railcom_cutout_end = (void *)0;

        } else {

            /* Old architecture: variable-period ISR with simplified cutout. */
            _main_encoder_interface.railcom_cutout_begin = config->main_track.railcom->cutout_begin;
            _main_encoder_interface.railcom_cutout_end = config->main_track.railcom->cutout_end;

        }

    }

    DccBitEncoder_initialize(&_main_encoder_context, &_main_encoder_interface);

    /* Wire main track scheduler */
    _main_scheduler_interface.load_packet = &_main_load_packet;
    _main_scheduler_interface.is_encoder_idle = &_main_is_encoder_idle;
    _main_scheduler_interface.build_idle_packet = &DccPacketEncoder_idle;
    _main_scheduler_interface.on_packet_sent = config->on_packet_sent;
    _main_scheduler_interface.on_idle = config->on_idle;

    DccScheduler_initialize(&_main_scheduler_context, &_main_scheduler_interface);

    /* Wire main track RailCom decoder */
    _main_railcom_interface.uart_read = (void *)0;
    _main_railcom_interface.on_datagram = (void *)0;

    if (config->main_track.railcom) {

        _main_railcom_interface.uart_read = config->main_track.railcom->uart_read;
        _main_railcom_interface.on_datagram = config->main_track.railcom->on_datagram;

    }

    DccRailcomDecoder_initialize(&_main_railcom_context, &_main_railcom_interface);

    /* Wire RailCom cutout module (one-shot timer state machine) */
    _railcom_cutout_interface.timer_one_shot_start = config->railcom_timer_start;
    _railcom_cutout_interface.timer_one_shot_stop = config->railcom_timer_stop;
    _railcom_cutout_interface.hbridge_disable = (void *)0;
    _railcom_cutout_interface.hbridge_enable = (void *)0;
    _railcom_cutout_interface.uart_ch1_enable = (void *)0;
    _railcom_cutout_interface.uart_ch1_disable = (void *)0;
    _railcom_cutout_interface.uart_ch2_enable = (void *)0;
    _railcom_cutout_interface.uart_ch2_disable = (void *)0;
    _railcom_cutout_interface.on_cutout_complete = &_railcom_on_cutout_complete;

    if (config->main_track.railcom) {

        _railcom_cutout_interface.hbridge_disable = config->main_track.railcom->hbridge_disable;
        _railcom_cutout_interface.hbridge_enable = config->main_track.railcom->hbridge_enable;
        _railcom_cutout_interface.uart_ch1_enable = config->main_track.railcom->uart_ch1_enable;
        _railcom_cutout_interface.uart_ch1_disable = config->main_track.railcom->uart_ch1_disable;
        _railcom_cutout_interface.uart_ch2_enable = config->main_track.railcom->uart_ch2_enable;
        _railcom_cutout_interface.uart_ch2_disable = config->main_track.railcom->uart_ch2_disable;

    }

    DccRailcomCutout_initialize(&_railcom_cutout_context, &_railcom_cutout_interface);

    /* Wire main track application layer.
     * If shared timer is configured, use ref-counted wrappers.
     * Otherwise fall back to per-channel timers. */
    if (config->shared_timer_start) {

        _main_application_interface.timer_start = &_shared_timer_acquire;
        _main_application_interface.timer_stop = &_shared_timer_release;

    } else {

        _main_application_interface.timer_start = config->main_track.timer_start;
        _main_application_interface.timer_stop = config->main_track.timer_stop;

    }
    _main_application_interface.track_power_set = config->main_track.track_power_set;
    _main_application_interface.encoder_start = &_main_encoder_start;
    _main_application_interface.encoder_stop = &_main_encoder_stop;
    _main_application_interface.scheduler_insert = &_main_scheduler_insert;
    _main_application_interface.scheduler_remove_address = &_main_scheduler_remove_address;
    _main_application_interface.scheduler_clear = &_main_scheduler_clear;

    DccApplicationMainTrack_initialize(&_main_application_interface);

    /* =================================================================
     * Service track channel: encoder + service mode
     * ================================================================= */

    /* Wire service track bit encoder */
    _service_encoder_interface.timer_set_period = config->service_track.timer_set_period;
    _service_encoder_interface.pin_toggle = config->service_track.pin_toggle;
    _service_encoder_interface.railcom_cutout_begin = (void *)0;
    _service_encoder_interface.railcom_cutout_end = (void *)0;
    _service_encoder_interface.on_packet_complete = &_service_on_packet_complete;

    DccBitEncoder_initialize(&_service_encoder_context, &_service_encoder_interface);

    /* Wire service mode common */
    _service_common_interface.load_packet = &_service_load_packet;
    _service_common_interface.is_encoder_idle = &_service_is_encoder_idle;

    DccServiceModeCommon_initialize(&_service_common_context, &_service_common_interface);

    /* Wire service track application layer.
     * If shared timer is configured, use ref-counted wrappers.
     * Otherwise fall back to per-channel timers. */
    if (config->shared_timer_start) {

        _service_application_interface.timer_start = &_shared_timer_acquire;
        _service_application_interface.timer_stop = &_shared_timer_release;

    } else {

        _service_application_interface.timer_start = config->service_track.timer_start;
        _service_application_interface.timer_stop = config->service_track.timer_stop;

    }
    _service_application_interface.track_power_set = config->service_track.track_power_set;
    _service_application_interface.encoder_start = &_service_encoder_start;
    _service_application_interface.encoder_stop = &_service_encoder_stop;
    _service_application_interface.enter_service_mode = &_service_enter_service_mode;
    _service_application_interface.exit_service_mode = &_service_exit_service_mode;
    _service_application_interface.is_service_mode_active = &_service_is_service_mode_active;

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
    _service_application_interface.direct_write_byte = &_service_direct_write_byte;
    _service_application_interface.direct_verify_byte = &_service_direct_verify_byte;
    _service_application_interface.direct_write_bit = &_service_direct_write_bit;
    _service_application_interface.direct_verify_bit = &_service_direct_verify_bit;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
    _service_application_interface.paged_write = &_service_paged_write;
    _service_application_interface.paged_verify = &_service_paged_verify;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
    _service_application_interface.register_write = &_service_register_write;
    _service_application_interface.register_verify = &_service_register_verify;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
    _service_application_interface.address_write = &_service_address_write;
    _service_application_interface.address_verify = &_service_address_verify;
#endif

    DccApplicationServiceTrack_initialize(&_service_application_interface);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /* Wire direct service mode */
    _service_direct_interface.begin_operation = &_service_begin_operation;
    _service_direct_interface.is_common_idle = &_service_is_common_idle;
    _service_direct_interface.on_complete = config->on_service_mode_complete;

    DccServiceModeDirect_initialize(&_service_direct_context, &_service_direct_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /* Wire paged service mode */
    _service_paged_interface.begin_operation = &_service_begin_operation;
    _service_paged_interface.is_common_idle = &_service_is_common_idle;
    _service_paged_interface.on_complete = config->on_service_mode_complete;

    DccServiceModePaged_initialize(&_service_paged_context, &_service_paged_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /* Wire register service mode */
    _service_register_interface.begin_operation = &_service_begin_operation;
    _service_register_interface.is_common_idle = &_service_is_common_idle;
    _service_register_interface.on_complete = config->on_service_mode_complete;

    DccServiceModeRegister_initialize(&_service_register_context, &_service_register_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /* Wire address-only service mode */
    _service_address_interface.begin_operation = &_service_begin_operation;
    _service_address_interface.is_common_idle = &_service_is_common_idle;
    _service_address_interface.on_complete = config->on_service_mode_complete;

    DccServiceModeAddress_initialize(&_service_address_context, &_service_address_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

#ifdef DCC_COMPILE_DECODER

    /* Wire CV storage interface */
    _cv_storage_interface.cv_read = config->cv_read;
    _cv_storage_interface.cv_write = config->cv_write;

    DccCvStorage_initialize(&_cv_storage_interface);

    /* Wire packet decoder interface — CV access routed through cv_storage */
    _packet_decoder_interface.cv_read = &DccCvStorage_read;
    _packet_decoder_interface.cv_write = &DccCvStorage_write;
    _packet_decoder_interface.on_speed_command = config->on_speed_command;
    _packet_decoder_interface.on_emergency_stop = config->on_emergency_stop;
    _packet_decoder_interface.on_function_command = config->on_function_command;
    _packet_decoder_interface.on_accessory_basic_command = config->on_accessory_basic_command;
    _packet_decoder_interface.on_accessory_extended_command = config->on_accessory_extended_command;
    _packet_decoder_interface.on_cv_write = config->on_cv_write;
    _packet_decoder_interface.on_cv_verify = config->on_cv_verify;
    _packet_decoder_interface.on_cv_bit = config->on_cv_bit;
    _packet_decoder_interface.on_consist_command = config->on_consist_command;
    _packet_decoder_interface.on_binary_state_short = config->on_binary_state_short;
    _packet_decoder_interface.on_binary_state_long = config->on_binary_state_long;
    _packet_decoder_interface.on_analog_function = config->on_analog_function;
    _packet_decoder_interface.on_speed_restriction = config->on_speed_restriction;
    _packet_decoder_interface.fire_ack_pulse = config->fire_ack_pulse;
    DccPacketDecoder_initialize(&_packet_decoder_interface);

    /* Wire bit decoder interface — complete packets go to packet decoder */
    _bit_decoder_interface.on_packet_received = &DccPacketDecoder_process_packet;

    DccBitDecoder_initialize(&_bit_decoder_interface);

    /* Wire RailCom encoder interface */
    _railcom_encoder_interface.uart_write = config->railcom_uart_write;

    DccRailcomEncoder_initialize(&_railcom_encoder_interface);

#endif /* DCC_COMPILE_DECODER */

}

void DccConfig_run(void) {

    if (!_configuration_pointer) {

        return;

    }

#ifdef DCC_COMPILE_COMMAND_STATION

    /* Main track: always runs the scheduler */
    DccScheduler_run(&_main_scheduler_context);

    /* Service track: runs service mode state machine when active */
    if (DccServiceModeCommon_is_active(&_service_common_context)) {

        DccServiceModeCommon_run(&_service_common_context);

    }

    /* Main track RailCom: drain decoded datagrams */
    DccRailcomDecoder_run(&_main_railcom_context);

#endif /* DCC_COMPILE_COMMAND_STATION */

}

#ifdef DCC_COMPILE_COMMAND_STATION

/* =========================================================================
 * ISR entry points (remain in dcc_config — called from hardware ISR)
 * ========================================================================= */

void DccConfig_main_track_isr(void) {

    DccBitEncoder_half_bit_isr(&_main_encoder_context);

}

void DccConfig_service_track_isr(void) {

    DccBitEncoder_half_bit_isr(&_service_encoder_context);

    /* Sample current sense every half-bit period for ACK detection.
     * Works with ADC (returns milliamps) or comparator (returns 0/non-zero).
     * The threshold comparison happens inside service mode common. */
    if (_configuration_pointer->service_track.current_sense_read) {

        DccServiceModeCommon_ack_sample(&_service_common_context, _configuration_pointer->service_track.current_sense_read());

    }

}

void DccConfig_shared_timer_isr(void) {

    /* Deterministic pin toggles — fire both channels immediately on
     * ISR entry using the look-ahead flags computed on the previous tick.
     * This eliminates jitter caused by variable state machine execution. */
    if (_main_encoder_context.toggle_next && _main_encoder_context.interface->pin_toggle) {

        _main_encoder_context.interface->pin_toggle();

    }

    if (_service_encoder_context.toggle_next && _service_encoder_context.interface->pin_toggle) {

        _service_encoder_context.interface->pin_toggle();

    }

    /* Now do the heavy state machine work and compute toggle_next
     * for the next tick. */
    DccBitEncoder_tick_isr(&_main_encoder_context);
    DccBitEncoder_tick_isr(&_service_encoder_context);

    /* Sample current sense every tick for service mode ACK detection. */
    if (_configuration_pointer->service_track.current_sense_read) {

        DccServiceModeCommon_ack_sample(&_service_common_context, _configuration_pointer->service_track.current_sense_read());

    }

}

void DccConfig_railcom_cutout_timer_isr(void) {

    DccRailcomCutout_timer_isr(&_railcom_cutout_context);

}

void DccConfig_100ms_timer_tick(void) {

    /* Timeout checking and periodic housekeeping. */

}

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

void DccConfig_decoder_edge(uint32_t timestamp_usec) {

    DccBitDecoder_edge(timestamp_usec);

}

#endif /* DCC_COMPILE_DECODER */
