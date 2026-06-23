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
 * modules (dcc_application_command_station_main_track,
 * dcc_application_command_station_service_track) are wired here and own the
 * user-facing API.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#include "dcc_config.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION
#include <string.h>

#include "dcc_bit_encoder.h"
#include "dcc_application_command_station_packet.h"
#include "dcc_scheduler.h"
#include "dcc_service_mode_common.h"
#include "dcc_railcom_decoder.h"
#include "dcc_railcom_cutout.h"
#include "dcc_application_command_station_main_track.h"
#include "dcc_application_command_station_service_track.h"
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
static interface_dcc_application_command_station_main_track_t _main_application_interface;

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
static interface_dcc_application_command_station_service_track_t _service_application_interface;

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

/* =========================================================================
 * NOP auto-scheduling state (accessory RailCom SRQ polling)
 * ========================================================================= */

    /** @brief 100ms tick counter for NOP auto-scheduling.
     *  NOP inserted every 50 ticks (5 seconds) per S-9.3.2 Section 6.3.3. */
static uint8_t _nop_tick_counter = 0;

    /** @brief NOP threshold address for SRQ collision arbitration.
     *  Starts at max (all decoders respond). Lowered on collision. */
static uint16_t _nop_threshold_address = 0x07FF;

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

    /** @brief ACK pulse currently in progress */
static bool _ack_pulse_active = false;

    /** @brief Timestamp when ACK pulse started (microseconds) */
static uint32_t _ack_pulse_start_usec = 0;

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

static void _main_set_railcom_enabled(bool enabled) {

    _main_encoder_context.railcom_enabled = enabled;

}

static bool _main_is_railcom_enabled(void) {

    return _main_encoder_context.railcom_enabled;

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

    if (!_configuration_pointer) {

        return;

    }

    _shared_timer_ref_count++;

    if (_shared_timer_ref_count == 1) {

        if (_configuration_pointer->shared_timer_start) {

            _configuration_pointer->shared_timer_start(period_usec);

        }

    }

}

static void _shared_timer_release(void) {

    if (!_configuration_pointer) {

        return;

    }

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
    _nop_tick_counter = 0;
    _nop_threshold_address = 0x07FF;

    /* Wire main track bit encoder */
    _main_encoder_interface.pin_toggle = config->main_track.pin_toggle;
    _main_encoder_interface.railcom_cutout_begin = (void *)0;
    _main_encoder_interface.on_packet_complete = &_main_on_packet_complete;

    if (config->main_track.railcom) {

        if (config->railcom_timer_start) {

            /* One-shot timer cutout module.
             * The tick ISR enters RAILCOM_CUTOUT state and calls begin().
             * The cutout module signals completion via cutout_complete flag
             * and handles the H-bridge resume itself. */
            _main_encoder_interface.railcom_cutout_begin = &_railcom_cutout_begin_wrapper;

        }

    }

    DccBitEncoder_initialize(&_main_encoder_context, &_main_encoder_interface);

    /* Wire main track scheduler */
    _main_scheduler_interface.load_packet = &_main_load_packet;
    _main_scheduler_interface.is_encoder_idle = &_main_is_encoder_idle;
    _main_scheduler_interface.build_idle_packet = &DccApplicationCommandStationPacket_load_idle;
    _main_scheduler_interface.on_packet_sent = config->on_packet_sent;

    DccScheduler_initialize(&_main_scheduler_context, &_main_scheduler_interface);

    /* Wire main track RailCom decoder */
    _main_railcom_interface.uart_read = (void *)0;
    _main_railcom_interface.on_datagram = (void *)0;

    if (config->main_track.railcom) {

        _main_railcom_interface.uart_read = config->main_track.railcom->uart_read;
        _main_railcom_interface.on_datagram = config->main_track.railcom->on_railcom_datagram_result;

    }

    DccRailcomDecoder_initialize(&_main_railcom_context, &_main_railcom_interface);

    /* Wire RailCom cutout module (one-shot timer state machine) */
    _railcom_cutout_interface.timer_one_shot_start = config->railcom_timer_start;
    _railcom_cutout_interface.timer_one_shot_stop = config->railcom_timer_stop;
    _railcom_cutout_interface.begin_railcom_cutout = (void *)0;
    _railcom_cutout_interface.end_railcom_cutout = (void *)0;
    _railcom_cutout_interface.uart_rx_enable = (void *)0;
    _railcom_cutout_interface.uart_rx_disable = (void *)0;
    _railcom_cutout_interface.on_cutout_complete = &_railcom_on_cutout_complete;

    if (config->main_track.railcom) {

        _railcom_cutout_interface.begin_railcom_cutout = config->main_track.railcom->begin_railcom_cutout;
        _railcom_cutout_interface.end_railcom_cutout = config->main_track.railcom->end_railcom_cutout;
        _railcom_cutout_interface.uart_rx_enable = config->main_track.railcom->uart_rx_enable;
        _railcom_cutout_interface.uart_rx_disable = config->main_track.railcom->uart_rx_disable;

    }

    /* Resolve each cutout timing: a non-zero config value overrides the spec
     * default, 0 selects the dcc_defines spec default. */
    uint16_t cutout_start_delay = config->railcom_cutout_start_delay_us
        ? config->railcom_cutout_start_delay_us : DCC_RAILCOM_CUTOUT_START_DELAY_US;
    uint16_t cutout_uart_rx_delay = config->railcom_uart_rx_delay_us
        ? config->railcom_uart_rx_delay_us : DCC_RAILCOM_UART_RX_DELAY_US;
    uint16_t cutout_ch1_window = config->railcom_ch1_window_us
        ? config->railcom_ch1_window_us : DCC_RAILCOM_CH1_WINDOW_US;
    uint16_t cutout_ch1_ch2_gap = config->railcom_ch1_ch2_gap_us
        ? config->railcom_ch1_ch2_gap_us : DCC_RAILCOM_CH1_CH2_GAP_US;
    uint16_t cutout_ch2_window = config->railcom_ch2_window_us
        ? config->railcom_ch2_window_us : DCC_RAILCOM_CH2_WINDOW_US;

    DccRailcomCutout_initialize(&_railcom_cutout_context, &_railcom_cutout_interface,
                                cutout_start_delay, cutout_uart_rx_delay, cutout_ch1_window,
                                cutout_ch1_ch2_gap, cutout_ch2_window);

    /* Wire main track application layer.
     * Uses ref-counted shared timer wrappers. */
    _main_application_interface.timer_start = &_shared_timer_acquire;
    _main_application_interface.timer_stop = &_shared_timer_release;
    _main_application_interface.track_power_set = config->main_track.track_power_set;
    _main_application_interface.encoder_start = &_main_encoder_start;
    _main_application_interface.encoder_stop = &_main_encoder_stop;
    _main_application_interface.scheduler_insert = &_main_scheduler_insert;
    _main_application_interface.scheduler_remove_address = &_main_scheduler_remove_address;
    _main_application_interface.scheduler_clear = &_main_scheduler_clear;
    _main_application_interface.set_railcom_enabled = &_main_set_railcom_enabled;
    _main_application_interface.is_railcom_enabled = &_main_is_railcom_enabled;

    DccApplicationCommandStationMainTrack_initialize(&_main_application_interface);

    /* =================================================================
     * Service track channel: encoder + service mode
     * ================================================================= */

    /* Wire service track bit encoder */
    _service_encoder_interface.pin_toggle = config->service_track.pin_toggle;
    _service_encoder_interface.railcom_cutout_begin = (void *)0;
    _service_encoder_interface.on_packet_complete = &_service_on_packet_complete;

    DccBitEncoder_initialize(&_service_encoder_context, &_service_encoder_interface);

    /* Wire service mode common */
    _service_common_interface.load_packet = &_service_load_packet;
    _service_common_interface.is_encoder_idle = &_service_is_encoder_idle;

    DccServiceModeCommon_initialize(&_service_common_context, &_service_common_interface);

    /* Wire service track application layer.
     * Uses ref-counted shared timer wrappers. */
    _service_application_interface.timer_start = &_shared_timer_acquire;
    _service_application_interface.timer_stop = &_shared_timer_release;
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

    DccApplicationCommandStationServiceTrack_initialize(&_service_application_interface);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /* Wire direct service mode */
    _service_direct_interface.begin_operation = &_service_begin_operation;
    _service_direct_interface.is_common_idle = &_service_is_common_idle;
    _service_direct_interface.on_complete = config->on_service_mode_result;

    DccServiceModeDirect_initialize(&_service_direct_context, &_service_direct_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /* Wire paged service mode */
    _service_paged_interface.begin_operation = &_service_begin_operation;
    _service_paged_interface.is_common_idle = &_service_is_common_idle;
    _service_paged_interface.on_complete = config->on_service_mode_result;

    DccServiceModePaged_initialize(&_service_paged_context, &_service_paged_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /* Wire register service mode */
    _service_register_interface.begin_operation = &_service_begin_operation;
    _service_register_interface.is_common_idle = &_service_is_common_idle;
    _service_register_interface.on_complete = config->on_service_mode_result;

    DccServiceModeRegister_initialize(&_service_register_context, &_service_register_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /* Wire address-only service mode */
    _service_address_interface.begin_operation = &_service_begin_operation;
    _service_address_interface.is_common_idle = &_service_is_common_idle;
    _service_address_interface.on_complete = config->on_service_mode_result;

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
    _packet_decoder_interface.on_emergency_stop_command = config->on_emergency_stop_command;
    _packet_decoder_interface.on_function_command = config->on_function_command;
    _packet_decoder_interface.on_accessory_basic_command = config->on_accessory_basic_command;
    _packet_decoder_interface.on_accessory_extended_command = config->on_accessory_extended_command;
    _packet_decoder_interface.on_cv_write_command = config->on_cv_write_command;
    _packet_decoder_interface.on_cv_verify_command = config->on_cv_verify_command;
    _packet_decoder_interface.on_cv_bit_command = config->on_cv_bit_command;
    _packet_decoder_interface.on_consist_command = config->on_consist_command;
    _packet_decoder_interface.on_binary_state_short_command = config->on_binary_state_short_command;
    _packet_decoder_interface.on_binary_state_long_command = config->on_binary_state_long_command;
    _packet_decoder_interface.on_analog_function_command = config->on_analog_function_command;
    _packet_decoder_interface.start_ack_pulse = config->start_ack_pulse;
    DccPacketDecoder_initialize(&_packet_decoder_interface);

    /* Wire bit decoder interface — complete packets go to packet decoder */
    _bit_decoder_interface.on_packet_received = &DccPacketDecoder_process_packet;

    DccBitDecoder_initialize(&_bit_decoder_interface);

    /* Wire RailCom encoder interface */
    _railcom_encoder_interface.uart_write = (void *)0;

    if (config->railcom_tx_pin_set) {

        /* RailCom Tx is available — wire the bit-bang pin setter */
        _railcom_encoder_interface.uart_write = (void *)0;

    }

    DccRailcomEncoder_initialize(&_railcom_encoder_interface);

    /* Initialize ACK pulse state */
    _ack_pulse_active = false;
    _ack_pulse_start_usec = 0;

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

#ifdef DCC_COMPILE_DECODER

    /* ACK pulse 6ms timing: poll timestamp and stop after 6ms elapsed */
    if (_ack_pulse_active) {

        uint32_t elapsed = _configuration_pointer->get_timestamp_usec() - _ack_pulse_start_usec;

        if (elapsed >= DCC_ACK_PULSE_DURATION_US) {

            _ack_pulse_active = false;

            if (_configuration_pointer->stop_ack_pulse) {

                _configuration_pointer->stop_ack_pulse();

            }

        }

    }

#endif /* DCC_COMPILE_DECODER */

}

#ifdef DCC_COMPILE_COMMAND_STATION

/* =========================================================================
 * ISR entry points (remain in dcc_config — called from hardware ISR)
 * ========================================================================= */

void DccConfig_58us_timer_isr(void) {

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

void DccConfig_railcom_oneshot_timer_isr(void) {

    DccRailcomCutout_timer_isr(&_railcom_cutout_context);

}

void DccConfig_100ms_timer_tick(void) {

    /* NOP auto-scheduling: insert accessory NOP every 5 seconds when main
     * track is powered on and RailCom is enabled (S-9.3.2 Section 6.3.3).
     * The NOP triggers accessory decoders to send SRQ in Ch1 if they have
     * pending updates. */
    if (_configuration_pointer->main_track.railcom &&
        _configuration_pointer->on_accessory_srq) {

        _nop_tick_counter++;

        if (_nop_tick_counter >= 50) {

            _nop_tick_counter = 0;

            /* Build and schedule a NOP packet with the current threshold
             * address. The threshold starts at max and is lowered on SRQ
             * collision (garbled Ch1 data) to narrow the responder pool. */
            dcc_packet_t nop_packet;
            memset(&nop_packet, 0, sizeof(nop_packet));
            DccApplicationCommandStationPacket_load_accessory_basic_stop(
                &nop_packet, _nop_threshold_address, 0);
            nop_packet.repeat_count = 1;

            DccApplicationCommandStationMainTrack_send_packet(
                &nop_packet, _nop_threshold_address,
                DCC_TAG_ACCESSORY, DCC_PRIORITY_ACCESSORY);

        }

    }

}

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

void DccConfig_decoder_edge_isr(uint32_t timestamp_usec) {

    DccBitDecoder_edge(timestamp_usec);

}

#endif /* DCC_COMPILE_DECODER */
