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
 * @date 25 Sep 2026
 */

#include "dcc_config.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_COMMAND_STATION

#include "dcc_bit_encoder.h"
#include "dcc_application_command_station_packet.h"
#include "dcc_scheduler.h"
#include "dcc_service_mode_common.h"
#if defined(DCC_COMPILE_RAILCOM)
#include "dcc_railcom_command_station.h"
#include "dcc_railcom_cutout.h"
#endif
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

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
#include "dcc_service_mode_task_direct.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
#include "dcc_service_mode_task_paged.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
#include "dcc_service_mode_task_register.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
#include "dcc_service_mode_task_address.h"
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
#include "dcc_service_mode_task_detect.h"
#endif

#ifdef DCC_COMPILE_DECODER
#include "dcc_bit_decoder.h"
#include "dcc_packet_decoder.h"
#include "dcc_cv_storage.h"
#include "dcc_application_decoder_cv.h"
#if defined(DCC_COMPILE_RAILCOM)
#include "dcc_railcom_decoder.h"
#endif
#include "dcc_failsafe.h"
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

#if defined(DCC_COMPILE_RAILCOM)
    /** @brief Main track RailCom decoder context */
static dcc_railcom_command_station_context_t _main_railcom_context;

    /** @brief Main track RailCom decoder interface */
static interface_dcc_railcom_command_station_t _main_railcom_interface;
#endif /* DCC_COMPILE_RAILCOM */

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

#if defined(DCC_COMPILE_RAILCOM)
    /** @brief RailCom cutout context (main track only for now) */
static dcc_railcom_cutout_context_t _railcom_cutout_context;

    /** @brief RailCom cutout interface */
static interface_dcc_railcom_cutout_t _railcom_cutout_interface;
#endif /* DCC_COMPILE_RAILCOM */

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

/* =========================================================================
 * Service mode task orchestrators (singletons — interface struct only,
 * the task module owns its own static context).
 * ========================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    /** @brief Direct-mode task orchestrator interface */
static interface_dcc_service_mode_task_direct_t _task_direct_interface;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    /** @brief Paged-mode task orchestrator interface */
static interface_dcc_service_mode_task_paged_t _task_paged_interface;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    /** @brief Register-mode task orchestrator interface */
static interface_dcc_service_mode_task_register_t _task_register_interface;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    /** @brief Address-only task orchestrator interface */
static interface_dcc_service_mode_task_address_t _task_address_interface;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    /** @brief Mode-detect task orchestrator interface */
static interface_dcc_service_mode_task_detect_t _task_detect_interface;
#endif

#ifdef DCC_COMPILE_DECODER

    /** @brief Interface struct for the bit decoder module */
static interface_dcc_bit_decoder_t _bit_decoder_interface;

    /** @brief Interface struct for the packet decoder module */
static interface_dcc_packet_decoder_t _packet_decoder_interface;

    /** @brief Interface struct for the CV storage module */
static interface_dcc_cv_storage_t _cv_storage_interface;

    /** @brief Interface struct for the decoder CV application layer */
static interface_dcc_application_decoder_cv_t _decoder_cv_application_interface;

#if defined(DCC_COMPILE_RAILCOM)
    /** @brief Interface struct for the RailCom encoder module */
static interface_dcc_railcom_decoder_t _railcom_encoder_interface;
#endif /* DCC_COMPILE_RAILCOM */

    /** @brief Interface struct for the packet-timeout fail-safe module */
static interface_dcc_failsafe_t _failsafe_interface;

    /** @brief ACK pulse currently in progress */
static bool _ack_pulse_active = false;

    /** @brief Timestamp when ACK pulse started (microseconds) */
static uint32_t _ack_pulse_start_usec = 0;

    /**
     * @brief start_ack_pulse as the packet decoder sees it: turn the ACK load on
     *  through the app's hook and start the 6 ms clock that DccConfig_run() polls
     *  to turn it off again (S-9.2.3 sec 3, 6 ms +/- 1 ms). Installed only when the
     *  app supplies start_ack_pulse, so a NULL hook still short-circuits in the
     *  decoder and the timer never arms.
     *
     *  A start while a pulse is already active is ignored, not restarted: the
     *  command station repeats a matching verify, and a packet that completes in
     *  under 6 ms would otherwise stretch the pulse across every repeat into one
     *  long pulse that the command station rejects as over-current. The ACK is a
     *  fixed 6 ms pulse from the first acknowledged packet.
     */
static void _start_ack_pulse_wrapper(void) {

    if (_ack_pulse_active) {

        return;

    }

    _ack_pulse_start_usec = _configuration_pointer->get_timestamp_usec();
    _ack_pulse_active = true;
    _configuration_pointer->start_ack_pulse();

}

    /** @brief Poll the ACK pulse and stop it once the 6 ms window has elapsed (S-9.2.3 sec 3). */
static void _run_ack_pulse(void) {

    uint32_t elapsed;

    if (!_ack_pulse_active) {

        return;

    }

    elapsed = _configuration_pointer->get_timestamp_usec() - _ack_pulse_start_usec;

    if (elapsed >= DCC_ACK_PULSE_DURATION_US) {

        _ack_pulse_active = false;

        if (_configuration_pointer->stop_ack_pulse) {

            _configuration_pointer->stop_ack_pulse();

        }

    }

}

#if defined(DCC_COMPILE_RAILCOM)
    /**
     * @brief Bit decoder on_packet_received dispatch for a RailCom decoder. Runs at the
     *  packet end-bit edge: fire the time-critical RailCom Tx first (it bit-bangs the
     *  cutout, blocking), then queue the packet for deferred instruction dispatch.
     *
     * @param data Raw packet bytes including the XOR byte.
     * @param byte_count Number of valid bytes in data.
     */
static void _on_packet_received_dispatch(const uint8_t *data, uint8_t byte_count) {

    DccRailcomDecoder_transmit(data, byte_count);
    DccPacketDecoder_enqueue(data, byte_count);

}
#endif /* DCC_COMPILE_RAILCOM */

    /**
     * @brief Application CV write: storage rules first, then refresh the packet
     *  decoder's address cache if an address CV changed.
     *
     * @details Routes the write through DccCvStorage_write() so the decoder lock,
     * the CV29 feature filter and the CV8 factory reset all apply, then tells the
     * packet decoder which CV changed so an address CV refreshes its match cache.
     *
     * @param cv_number CV number (1-based).
     * @param value Value to store.
     *
     * @return true if the storage write succeeded, false if it was refused (locked or hardware failure).
     */
static bool _decoder_cv_application_write(uint16_t cv_number, uint8_t value) {

    if (!DccCvStorage_write(cv_number, value)) {

        return false;

    }

    DccPacketDecoder_on_cv_written(cv_number);

    return true;

}

#endif /* DCC_COMPILE_DECODER */

#ifdef DCC_COMPILE_COMMAND_STATION

#if defined(DCC_COMPILE_RAILCOM)
/* =========================================================================
 * RailCom address tracking: which DCC address a cutout's decoded bytes
 * should be tagged with. Declared here, ahead of the wrapper functions
 * below that populate it, because it is captured at packet-load/-complete
 * time, not at cutout time -- see the RailCom cutout bridge comment further
 * down for why two stages, not one.
 * ========================================================================= */

    /** @brief Address of the packet most recently handed to the main track encoder */
static dcc_address_t _main_railcom_loaded_address = 0;

    /** @brief Promoted from _main_railcom_loaded_address once that packet's transmission finishes; tags the cutout that follows */
static dcc_address_t _main_railcom_completed_address = 0;

    /**
     * @brief Extract the locomotive address a main track packet is addressed to, for RailCom tagging.
     *
     * @details Short (1-127) and long/extended (0xC0-0xE7 + a second byte, S-9.2.1)
     * address forms only -- covers real locomotive traffic, which is what
     * RailCom POM/ADR replies are tagged against. Broadcast, accessory,
     * idle, and reserved leading bytes fall through to 0 (untagged) --
     * the address is only a label for decoded datagrams, not used for
     * routing. Accessory RailCom replies therefore come back untagged;
     * not handled here since nothing in this library decodes accessory
     * RailCom today.
     *
     * @param packet Packet about to be handed to the encoder; may be NULL.
     *
     * @return Decoded short or long address, or 0 when the packet is NULL, empty, or not addressed to a locomotive.
     */
static dcc_address_t _decode_main_packet_address(const dcc_packet_t *packet) {

    if (!packet || packet->byte_count == 0) {

        return 0;

    }

    uint8_t first_byte = packet->data[0];

    if (first_byte >= 0xC0 && first_byte <= 0xE7 && packet->byte_count >= 2) {

        return (dcc_address_t)(((first_byte & 0x3F) << 8) | packet->data[1]);

    }

    if (first_byte >= 1 && first_byte <= 127) {

        return (dcc_address_t)first_byte;

    }

    return 0;

}
#endif /* DCC_COMPILE_RAILCOM */

/* =========================================================================
 * Main track wrapper functions
 *
 * These capture the main track context pointers so interface structs
 * (which use fixed callback signatures) can reach the correct instance.
 * ========================================================================= */

    /**
     * @brief Main track encoder on_packet_complete hook: promote the RailCom tag address, then notify the scheduler.
     *
     * @details Runs from the bit encoder ISR at the end of a packet. With RailCom
     * compiled in it first freezes the finished packet's address for the cutout that
     * follows (see the two-stage capture note further down), then hands the completion
     * to DccScheduler_on_packet_complete() so the next packet can be dispatched.
     */
static void _main_on_packet_complete(void) {

#if defined(DCC_COMPILE_RAILCOM)
    /* The just-finished packet's address is frozen here, before the cutout
     * it triggers can complete and before the next packet can possibly be
     * loaded (DccScheduler_run() will not call load_packet() again until
     * the packet_complete_flag this sets has been consumed) -- see the
     * RailCom cutout bridge comment below for why this two-stage capture
     * replaces recording the address in on_packet_sent. */
    _main_railcom_completed_address = _main_railcom_loaded_address;
#endif

    DccScheduler_on_packet_complete(&_main_scheduler_context);

}

    /**
     * @brief Main track scheduler load_packet hook: record the RailCom tag address, then hand the packet to the encoder.
     *
     * @param packet Packet the scheduler dispatched for transmission.
     */
static void _main_load_packet(const dcc_packet_t *packet) {

#if defined(DCC_COMPILE_RAILCOM)
    _main_railcom_loaded_address = _decode_main_packet_address(packet);
#endif

    DccBitEncoder_load_packet(&_main_encoder_context, packet);

}

    /**
     * @brief Main track scheduler is_encoder_idle hook.
     *
     * @return true if the main track bit encoder has no packet in flight.
     */
static bool _main_is_encoder_idle(void) {

    return DccBitEncoder_is_idle(&_main_encoder_context);

}

    /** @brief Main track application encoder_start hook: start the main track bit encoder. */
static void _main_encoder_start(void) {

    DccBitEncoder_start(&_main_encoder_context);

}

    /** @brief Main track application encoder_stop hook: stop the main track bit encoder. */
static void _main_encoder_stop(void) {

    DccBitEncoder_stop(&_main_encoder_context);

}


    /**
     * @brief Main track application scheduler_insert hook: forward to the main track scheduler instance.
     *
     * @param packet Packet to schedule.
     * @param address DCC address used as the duplicate-combining key.
     * @param tag Sub-key for duplicate combining.
     * @param priority Packet priority level.
     * @param auto_refresh true keeps the packet in the refresh cycle indefinitely.
     *
     * @return true if the packet was scheduled, false if no slot was free or a one-shot arrived with repeat_count 0.
     */
static bool _main_scheduler_insert(const dcc_packet_t *packet, dcc_address_t address, dcc_tag_enum tag, dcc_priority_enum priority, bool auto_refresh) {

    return DccScheduler_insert(&_main_scheduler_context, packet, address, tag, priority, auto_refresh);

}

    /**
     * @brief Main track application scheduler_remove_address hook: release every slot held for an address.
     *
     * @param address DCC address whose slots are released.
     */
static void _main_scheduler_remove_address(dcc_address_t address) {

    DccScheduler_remove_address(&_main_scheduler_context, address);

}

    /** @brief Main track application scheduler_clear hook: release every scheduler slot. */
static void _main_scheduler_clear(void) {

    DccScheduler_clear(&_main_scheduler_context);

}

/* =========================================================================
 * Service track wrapper functions
 * ========================================================================= */

    /** @brief Service track encoder on_packet_complete hook: advance the service mode common state machine. */
static void _service_on_packet_complete(void) {

    DccServiceModeCommon_on_packet_complete(&_service_common_context);

}

    /**
     * @brief Service track load_packet hook: fire the application's on_packet_sent, then hand the packet to the encoder.
     *
     * @details The callback fires at dispatch, before the packet is on the wire. The
     * same callback is wired straight into the main track scheduler, so the
     * application sees every packet from either track through one hook.
     *
     * @param packet Packet the service mode common module is sending next.
     */
static void _service_load_packet(const dcc_packet_t *packet) {

    if (_configuration_pointer && _configuration_pointer->on_packet_sent) {

        _configuration_pointer->on_packet_sent(packet);

    }

    DccBitEncoder_load_packet(&_service_encoder_context, packet);

}

    /**
     * @brief Service track is_encoder_idle hook.
     *
     * @return true if the service track bit encoder has no packet in flight.
     */
static bool _service_is_encoder_idle(void) {

    return DccBitEncoder_is_idle(&_service_encoder_context);

}

    /** @brief Service track application encoder_start hook: start the service track bit encoder. */
static void _service_encoder_start(void) {

    DccBitEncoder_start(&_service_encoder_context);

}

    /** @brief Service track application encoder_stop hook: stop the service track bit encoder. */
static void _service_encoder_stop(void) {

    DccBitEncoder_stop(&_service_encoder_context);

}

    /**
     * @brief Service mode primitive begin_operation hook: forward to the service track common instance.
     *
     * @param packet Command packet to send during the command phase.
     * @param callback Fired when the operation completes.
     * @param is_write_operation true for writes (longer recovery), false for verifies.
     * @param command_repeat Number of command packets to send.
     * @param recovery_count Number of recovery packets to send after the command phase.
     *
     * @return true if the operation started, false if busy or no current sense.
     */
static bool _service_begin_operation(const dcc_packet_t *packet, dcc_service_mode_step_callback_t callback, bool is_write_operation, uint8_t command_repeat, uint8_t recovery_count) {

    return DccServiceModeCommon_begin_operation(&_service_common_context, packet, callback, is_write_operation, command_repeat, recovery_count);

}

    /**
     * @brief Service mode is_common_idle / is_idle hook.
     *
     * @return true if the service track common module has no operation in progress.
     */
static bool _service_is_common_idle(void) {

    return DccServiceModeCommon_is_idle(&_service_common_context);

}

    /**
     * @brief Service track application enter_service_mode hook.
     *
     * @return true; entry cannot fail in this release.
     */
static bool _service_enter_service_mode(void) {

    return DccServiceModeCommon_enter(&_service_common_context);

}

    /** @brief Service track application exit_service_mode hook. Ignored while an operation is still in progress. */
static void _service_exit_service_mode(void) {

    DccServiceModeCommon_exit(&_service_common_context);

}

    /**
     * @brief Service track application is_service_mode_active hook.
     *
     * @return true while service mode has been entered and not yet exited.
     */
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

    /**
     * @brief Reference-counted shared timer start.
     *
     * @details Increments the channel count and starts the hardware timer only on
     * the 0 -> 1 transition; a later acquire from the other channel is counted but
     * does not restart the timer. No-op before DccConfig_initialize().
     *
     * @param period_usec Timer period in microseconds (both application layers pass DCC_ONE_BIT_HALF_PERIOD_US).
     */
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

    /**
     * @brief Reference-counted shared timer stop.
     *
     * @details Decrements the channel count (never below zero) and stops the
     * hardware timer only when it reaches zero, so the other channel keeps
     * running. No-op before DccConfig_initialize().
     */
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

#if defined(DCC_COMPILE_RAILCOM)
/* =========================================================================
 * RailCom cutout bridge: the bit encoder's end bit arms the cutout timer.
 * The encoder runs continuously -- the driver blanks its own output between
 * the begin/end hooks -- so on_cutout_complete is not needed for that
 * purpose. It is needed for a second one: telling the command station
 * library when a cutout's bytes are ready to be read.
 *
 * DccRailcomCommandStation_begin_cutout() tags the upcoming cutout's decoded
 * bytes with a DCC address and is the only thing that sets cutout_pending;
 * without it DccRailcomCommandStation_run() returns immediately and no
 * RailCom byte is ever decoded. It must be called when the cutout COMPLETES
 * (the CH2 -> IDLE transition, reported through on_cutout_complete), not when
 * it begins: at cutout begin neither channel's bytes have been captured yet
 * (Channel 1 opens ~80 us later, Channel 2 a few hundred us after that), so
 * a cutout_pending flag set that early lets the next
 * DccRailcomCommandStation_run() poll read stale bytes left in the UART from
 * an earlier cutout -- Channel 1 and Channel 2 datagrams then show up
 * intermittently mislabeled.
 *
 * The address to tag with is _main_railcom_completed_address (declared
 * above, ahead of _main_load_packet()/_main_on_packet_complete() which
 * populate it) -- captured in two stages, not read directly from whatever
 * packet was most recently sent. A single-stage capture (recording the
 * address when a packet is dispatched, in on_packet_sent) races the
 * scheduler: on_packet_sent and load_packet fire back to back for a NEW
 * packet as soon as the PREVIOUS one's transmission ends, which is also the
 * moment this cutout begins -- a fast enough main loop can dispatch that
 * next packet, and overwrite the recorded address, before this cutout
 * completes and reads it, mistagging the reply. The two-stage version does
 * not have that race: _main_load_packet() records the address a packet is
 * dispatched with (_loaded), and _main_on_packet_complete() promotes it to
 * _completed once that same packet's transmission actually finishes --
 * which is also the earliest point the scheduler will dispatch a new one,
 * so _completed cannot change again until well after this cutout (~450 us)
 * has read it and the next packet's own end bit arrives (comfortably longer
 * -- the 16-bit preamble alone is ~1.9 ms).
 * ========================================================================= */

    /** @brief Bit encoder railcom_cutout_begin hook: arm the cutout one-shot state machine at the packet end bit. */
static void _railcom_cutout_begin_wrapper(void) {

    DccRailcomCutout_begin(&_railcom_cutout_context);

}

    /** @brief Cutout on_cutout_complete hook: tell the command station RailCom module which address the captured bytes belong to. */
static void _railcom_cutout_complete_wrapper(void) {

    DccRailcomCommandStation_begin_cutout(&_main_railcom_context, _main_railcom_completed_address);

}
#endif /* DCC_COMPILE_RAILCOM */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /**
     * @brief Task-layer write_byte hook: forward to the direct-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param value Value to write.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_direct_write_byte(uint16_t cv_number, uint8_t value) {

    return DccServiceModeDirect_write_byte(&_service_direct_context, cv_number, value);

}

    /**
     * @brief Task-layer verify_byte hook: forward to the direct-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param value Value to compare against.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_direct_verify_byte(uint16_t cv_number, uint8_t value) {

    return DccServiceModeDirect_verify_byte(&_service_direct_context, cv_number, value);

}

    /**
     * @brief Task-layer write_bit hook: forward to the direct-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param bit_position Bit position (0-7).
     * @param bit_value Bit value to write.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_direct_write_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    return DccServiceModeDirect_write_bit(&_service_direct_context, cv_number, bit_position, bit_value);

}

    /**
     * @brief Task-layer verify_bit hook: forward to the direct-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param bit_position Bit position (0-7).
     * @param bit_value Bit value to compare against.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_direct_verify_bit(uint16_t cv_number, uint8_t bit_position, bool bit_value) {

    return DccServiceModeDirect_verify_bit(&_service_direct_context, cv_number, bit_position, bit_value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /**
     * @brief Task-layer paged_write hook: forward to the paged-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param value Value to write.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_paged_write(uint16_t cv_number, uint8_t value) {

    return DccServiceModePaged_write(&_service_paged_context, cv_number, value);

}

    /**
     * @brief Task-layer paged_verify hook: forward to the paged-mode primitive instance.
     *
     * @param cv_number CV number (1-based).
     * @param value Value to compare against.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_paged_verify(uint16_t cv_number, uint8_t value) {

    return DccServiceModePaged_verify(&_service_paged_context, cv_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /**
     * @brief Task-layer register_write hook: forward to the register-mode primitive instance.
     *
     * @param register_number Register number (1-8).
     * @param value Value to write.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_register_write(uint8_t register_number, uint8_t value) {

    return DccServiceModeRegister_write(&_service_register_context, register_number, value);

}

    /**
     * @brief Task-layer register_verify hook: forward to the register-mode primitive instance.
     *
     * @param register_number Register number (1-8).
     * @param value Value to compare against.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_register_verify(uint8_t register_number, uint8_t value) {

    return DccServiceModeRegister_verify(&_service_register_context, register_number, value);

}

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /**
     * @brief Task-layer address_write hook: forward to the address-only primitive instance.
     *
     * @param address Short address to write to CV1.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_address_write(uint8_t address) {

    return DccServiceModeAddress_write(&_service_address_context, address);

}

    /**
     * @brief Task-layer address_verify hook: forward to the address-only primitive instance.
     *
     * @param address Short address to compare against CV1.
     *
     * @return true if the operation started, false if the primitive was busy.
     */
static bool _service_address_verify(uint8_t address) {

    return DccServiceModeAddress_verify(&_service_address_context, address);

}

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

/* =========================================================================
 * Service mode task dispatcher
 *
 * Every primitive's on_complete slot is wired here. When a primitive finishes
 * its full operation, this forwards the result to each task orchestrator. Only
 * the orchestrator that is mid-operation acts on it; the others are IDLE and
 * treat the call as a no-op. (Just one service track, so at most one task runs.)
 * The ACK outcome travels inside `result` (SUCCESS = ACK), so no separate ACK
 * signal is needed.
 * ========================================================================= */

    /**
     * @brief Primitive on_complete hook: fan the result out to every compiled task orchestrator.
     *
     * @param result Outcome of the primitive operation (SUCCESS = ACK seen).
     */
static void _service_task_primitive_complete(dcc_service_mode_result_enum result) {

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    DccServiceModeTaskDirect_on_primitive_complete(result);
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    DccServiceModeTaskPaged_on_primitive_complete(result);
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    DccServiceModeTaskRegister_on_primitive_complete(result);
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    DccServiceModeTaskAddress_on_primitive_complete(result);
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    DccServiceModeTaskDetect_on_primitive_complete(result);
#endif

    (void)result;

}

#endif /* DCC_COMPILE_COMMAND_STATION */

    /**
     * @brief Initialize the DCC library with user configuration.
     *
     * @details Algorithm:
     * -# Store the configuration pointer; return if it is NULL
     * -# Command station: wire and initialize the main track bit encoder, scheduler,
     *    RailCom receiver and cutout state machine (RailCom builds), and the main
     *    track application layer
     * -# Command station: wire and initialize the service track bit encoder, service
     *    mode common module and service track application layer, pointing the
     *    programming API at the task orchestrators
     * -# Wire and initialize each compiled service mode primitive and task orchestrator
     * -# Decoder: wire and initialize CV storage, the CV application layer, the packet
     *    decoder, the packet-timeout fail-safe, the bit decoder and (RailCom builds)
     *    the RailCom transmitter; reset the ACK pulse state
     *
     * @verbatim
     * @param config Pointer to the user-populated dcc_config_t. Must remain valid for the lifetime of the application.
     * @endverbatim
     */
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
    _main_encoder_interface.pin_toggle = config->main_track.pin_toggle;
    _main_encoder_interface.railcom_cutout_begin = (void *)0;
    _main_encoder_interface.on_packet_complete = &_main_on_packet_complete;

#if defined(DCC_COMPILE_RAILCOM)
    if (config->main_track.railcom) {

        if (config->railcom_timer_start) {

            /* One-shot timer cutout module.
             * The tick ISR enters RAILCOM_CUTOUT state and calls begin().
             * The cutout module signals completion via cutout_complete flag
             * and handles the H-bridge resume itself. */
            _main_encoder_interface.railcom_cutout_begin = &_railcom_cutout_begin_wrapper;

        }

    }
#endif /* DCC_COMPILE_RAILCOM */

    DccBitEncoder_initialize(&_main_encoder_context, &_main_encoder_interface);

    /* Wire main track scheduler */
    _main_scheduler_interface.load_packet = &_main_load_packet;
    _main_scheduler_interface.is_encoder_idle = &_main_is_encoder_idle;
    _main_scheduler_interface.build_idle_packet = &DccApplicationCommandStationPacket_load_idle;
    /* RailCom address tracking (when compiled in) happens in _main_load_packet()/
     * _main_on_packet_complete() above, not here -- on_packet_sent stays a plain
     * pass-through to the application's own callback either way. */
    _main_scheduler_interface.on_packet_sent = config->on_packet_sent;

    DccScheduler_initialize(&_main_scheduler_context, &_main_scheduler_interface);

#if defined(DCC_COMPILE_RAILCOM)
    /* Wire main track RailCom decoder */
    _main_railcom_interface.uart_read = (void *)0;
    _main_railcom_interface.on_datagram = (void *)0;

    if (config->main_track.railcom) {

        _main_railcom_interface.uart_read = config->main_track.railcom->uart_read;
        _main_railcom_interface.on_datagram = config->main_track.railcom->on_railcom_datagram_result;

    }

    DccRailcomCommandStation_initialize(&_main_railcom_context, &_main_railcom_interface);

    /* Wire RailCom cutout module (one-shot timer state machine) */
    _railcom_cutout_interface.timer_one_shot_start = config->railcom_timer_start;
    _railcom_cutout_interface.timer_one_shot_stop = config->railcom_timer_stop;
    _railcom_cutout_interface.begin_railcom_cutout = (void *)0;
    _railcom_cutout_interface.end_railcom_cutout = (void *)0;
    _railcom_cutout_interface.uart_rx_enable = (void *)0;
    _railcom_cutout_interface.uart_rx_disable = (void *)0;
    _railcom_cutout_interface.on_cutout_complete = (void *)0;  /* only if main_track.railcom is configured, below */

    if (config->main_track.railcom) {

        _railcom_cutout_interface.begin_railcom_cutout = config->main_track.railcom->begin_railcom_cutout;
        _railcom_cutout_interface.end_railcom_cutout = config->main_track.railcom->end_railcom_cutout;
        _railcom_cutout_interface.uart_rx_enable = config->main_track.railcom->uart_rx_enable;
        _railcom_cutout_interface.uart_rx_disable = config->main_track.railcom->uart_rx_disable;
        _railcom_cutout_interface.on_cutout_complete = &_railcom_cutout_complete_wrapper;

    }

    /* Resolve each cutout timing: a non-zero config value overrides the spec
     * default, 0 selects the dcc_defines spec default. */
    uint16_t cutout_start_delay = config->railcom_cutout_start_delay_us ? config->railcom_cutout_start_delay_us : DCC_RAILCOM_CUTOUT_START_DELAY_US;
    uint16_t cutout_uart_rx_delay = config->railcom_uart_rx_delay_us ? config->railcom_uart_rx_delay_us : DCC_RAILCOM_UART_RX_DELAY_US;
    uint16_t cutout_ch1_window = config->railcom_ch1_window_us ? config->railcom_ch1_window_us : DCC_RAILCOM_CH1_WINDOW_US;
    uint16_t cutout_ch1_ch2_gap = config->railcom_ch1_ch2_gap_us ? config->railcom_ch1_ch2_gap_us : DCC_RAILCOM_CH1_CH2_GAP_US;
    uint16_t cutout_ch2_window = config->railcom_ch2_window_us ? config->railcom_ch2_window_us : DCC_RAILCOM_CH2_WINDOW_US;

    DccRailcomCutout_initialize(&_railcom_cutout_context, &_railcom_cutout_interface,
                                cutout_start_delay, cutout_uart_rx_delay, cutout_ch1_window,
                                cutout_ch1_ch2_gap, cutout_ch2_window);
#endif /* DCC_COMPILE_RAILCOM */

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

    /* Programming surface is the task layer; the façade delegates to the task
     * singletons. The function pointers match the task public signatures, so they
     * are assigned directly. */
#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    _service_application_interface.direct_read_cv = &DccServiceModeTaskDirect_read_cv;
    _service_application_interface.direct_write_cv = &DccServiceModeTaskDirect_write_cv;
    _service_application_interface.direct_read_bit = &DccServiceModeTaskDirect_read_bit;
    _service_application_interface.direct_write_bit = &DccServiceModeTaskDirect_write_bit;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    _service_application_interface.paged_read_cv = &DccServiceModeTaskPaged_read_cv;
    _service_application_interface.paged_write_cv = &DccServiceModeTaskPaged_write_cv;
    _service_application_interface.paged_read_bit = &DccServiceModeTaskPaged_read_bit;
    _service_application_interface.paged_write_bit = &DccServiceModeTaskPaged_write_bit;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    _service_application_interface.register_read_cv = &DccServiceModeTaskRegister_read_cv;
    _service_application_interface.register_write_cv = &DccServiceModeTaskRegister_write_cv;
    _service_application_interface.register_read_bit = &DccServiceModeTaskRegister_read_bit;
    _service_application_interface.register_write_bit = &DccServiceModeTaskRegister_write_bit;
    _service_application_interface.register_factory_reset = &DccServiceModeTaskRegister_factory_reset;
    _service_application_interface.register_verify_value = &DccServiceModeTaskRegister_verify_value;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    _service_application_interface.address_read = &DccServiceModeTaskAddress_read;
    _service_application_interface.address_write = &DccServiceModeTaskAddress_write;
    _service_application_interface.address_verify = &DccServiceModeTaskAddress_verify;
    _service_application_interface.address_read_bit = &DccServiceModeTaskAddress_read_bit;
    _service_application_interface.address_write_bit = &DccServiceModeTaskAddress_write_bit;
#endif

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    _service_application_interface.detect_mode = &DccServiceModeTaskDetect_detect_mode;
#endif

    DccApplicationCommandStationServiceTrack_initialize(&_service_application_interface);

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT

    /* Wire direct service mode */
    _service_direct_interface.begin_operation = &_service_begin_operation;
    _service_direct_interface.is_common_idle = &_service_is_common_idle;
    _service_direct_interface.on_complete = &_service_task_primitive_complete;

    DccServiceModeDirect_initialize(&_service_direct_context, &_service_direct_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_PAGED

    /* Wire paged service mode */
    _service_paged_interface.begin_operation = &_service_begin_operation;
    _service_paged_interface.is_common_idle = &_service_is_common_idle;
    _service_paged_interface.on_complete = &_service_task_primitive_complete;

    DccServiceModePaged_initialize(&_service_paged_context, &_service_paged_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER

    /* Wire register service mode */
    _service_register_interface.begin_operation = &_service_begin_operation;
    _service_register_interface.is_common_idle = &_service_is_common_idle;
    _service_register_interface.on_complete = &_service_task_primitive_complete;

    DccServiceModeRegister_initialize(&_service_register_context, &_service_register_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS

    /* Wire address-only service mode */
    _service_address_interface.begin_operation = &_service_begin_operation;
    _service_address_interface.is_common_idle = &_service_is_common_idle;
    _service_address_interface.on_complete = &_service_task_primitive_complete;

    DccServiceModeAddress_initialize(&_service_address_context, &_service_address_interface);

#endif /* DCC_COMPILE_SERVICE_MODE_ADDRESS */

    /* =================================================================
     * Service mode task orchestrators
     *
     * Each task drives the matching primitive(s) through the same wrapper
     * functions the application layer used to call directly. is_idle is wired
     * to the common-idle check; on_start_ack_scan is unused (ACK windowing is
     * done in the common module) and left NULL.
     * ================================================================= */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
    _task_direct_interface.verify_bit = &_service_direct_verify_bit;
    _task_direct_interface.verify_byte = &_service_direct_verify_byte;
    _task_direct_interface.write_byte = &_service_direct_write_byte;
    _task_direct_interface.write_bit = &_service_direct_write_bit;
    _task_direct_interface.is_idle = &_service_is_common_idle;
    _task_direct_interface.on_start_ack_scan = (void *)0;

    DccServiceModeTaskDirect_initialize(&_task_direct_interface);
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DIRECT */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_PAGED
    _task_paged_interface.paged_verify = &_service_paged_verify;
    _task_paged_interface.paged_write = &_service_paged_write;
    _task_paged_interface.is_idle = &_service_is_common_idle;
    _task_paged_interface.on_start_ack_scan = (void *)0;

    DccServiceModeTaskPaged_initialize(&_task_paged_interface);
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_PAGED */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
    _task_register_interface.register_verify = &_service_register_verify;
    _task_register_interface.register_write = &_service_register_write;
    _task_register_interface.is_idle = &_service_is_common_idle;
    _task_register_interface.on_start_ack_scan = (void *)0;

    DccServiceModeTaskRegister_initialize(&_task_register_interface);
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_REGISTER */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
    _task_address_interface.address_verify = &_service_address_verify;
    _task_address_interface.address_write = &_service_address_write;
    _task_address_interface.is_idle = &_service_is_common_idle;
    _task_address_interface.on_start_ack_scan = (void *)0;

    DccServiceModeTaskAddress_initialize(&_task_address_interface);
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS */

#ifdef DCC_COMPILE_SERVICE_MODE_TASK_DETECT
    /* Detect probes all four modes; it requires the matching primitives to be
     * compiled in. Unwired probes remain NULL (their stage would never run). */
    _task_detect_interface.is_idle = &_service_is_common_idle;
    _task_detect_interface.on_start_ack_scan = (void *)0;
#ifdef DCC_COMPILE_SERVICE_MODE_DIRECT
    _task_detect_interface.direct_verify_bit = &_service_direct_verify_bit;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_PAGED
    _task_detect_interface.paged_verify = &_service_paged_verify;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_REGISTER
    _task_detect_interface.register_verify = &_service_register_verify;
#endif
#ifdef DCC_COMPILE_SERVICE_MODE_ADDRESS
    _task_detect_interface.address_verify = &_service_address_verify;
#endif

    DccServiceModeTaskDetect_initialize(&_task_detect_interface);
#endif /* DCC_COMPILE_SERVICE_MODE_TASK_DETECT */

#ifdef DCC_COMPILE_DECODER

    /* Wire CV storage interface */
    _cv_storage_interface.cv_read = config->cv_read;
    _cv_storage_interface.cv_write = config->cv_write;
    _cv_storage_interface.factory_reset = config->factory_reset;
    _cv_storage_interface.cv_read_indexed = config->cv_read_indexed;
    _cv_storage_interface.cv_write_indexed = config->cv_write_indexed;
    _cv_storage_interface.cv29_apply_supported_features = config->cv29_apply_supported_features;

    DccCvStorage_initialize(&_cv_storage_interface);

    /* Wire decoder CV application layer -- routed through cv_storage so the
     * decoder lock, the CV 29 feature filter and the CV 8 reset apply, then
     * through the packet decoder so an address-CV write refreshes its cache. */
    _decoder_cv_application_interface.cv_read = &DccCvStorage_read;
    _decoder_cv_application_interface.cv_write = &_decoder_cv_application_write;
    _decoder_cv_application_interface.is_locked = &DccCvStorage_is_locked;

    DccApplicationDecoderCv_initialize(&_decoder_cv_application_interface);

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
    _packet_decoder_interface.start_ack_pulse = config->start_ack_pulse ? &_start_ack_pulse_wrapper : (void *)0;
    _packet_decoder_interface.on_addressed_packet = &DccFailsafe_note_valid_packet;
#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)
    _packet_decoder_interface.on_address_changed = &DccRailcomDecoder_set_address;
#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
    DccPacketDecoder_initialize(&_packet_decoder_interface);

    /* Wire packet-timeout fail-safe (S-9.2.4 §4). CV11 read through cv_storage;
     * the timer is polled in DccConfig_run and re-armed by on_addressed_packet. */
    _failsafe_interface.cv_read = &DccCvStorage_read;
    _failsafe_interface.get_timestamp_usec = config->get_timestamp_usec;
    _failsafe_interface.on_failsafe_entered = config->on_failsafe_entered;
    _failsafe_interface.on_failsafe_exited = config->on_failsafe_exited;
    DccFailsafe_initialize(&_failsafe_interface);

    /* Wire bit decoder interface — complete packets go to packet decoder. With RailCom,
     * route through a dispatch that fires the time-critical Tx (bit-bangs the cutout)
     * before queuing the packet for deferred instruction dispatch. */
#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)
    _bit_decoder_interface.on_packet_received = &_on_packet_received_dispatch;
#else
    _bit_decoder_interface.on_packet_received = &DccPacketDecoder_enqueue;
#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */
#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_DECODER)
    /* Per-byte feed to the RailCom Tx dispatch (recognize + answer before the XOR) */
    _bit_decoder_interface.on_byte_received = &DccRailcomDecoder_on_byte_received;
#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_DECODER */

    DccBitDecoder_initialize(&_bit_decoder_interface);

#if defined(DCC_COMPILE_RAILCOM)
    /* Wire RailCom Tx engine: bit-bang pin + the app's cycle-accurate delay, the
     * shared-resource lock (4us bit timing) and edge-IRQ mask (the decoder's injected
     * current self-triggers the edge ISR during the cutout). */
    _railcom_encoder_interface.tx_pin_set = config->railcom_tx_pin_set;
    _railcom_encoder_interface.delay_us = config->railcom_delay_us;
    _railcom_encoder_interface.lock_shared_resources = config->lock_shared_resources;
    _railcom_encoder_interface.unlock_shared_resources = config->unlock_shared_resources;
    _railcom_encoder_interface.on_railcom_request = config->on_railcom_request;

    DccRailcomDecoder_initialize(&_railcom_encoder_interface);
#endif /* DCC_COMPILE_RAILCOM */

    /* Initialize ACK pulse state */
    _ack_pulse_active = false;
    _ack_pulse_start_usec = 0;

#endif /* DCC_COMPILE_DECODER */

}

    /**
     * @brief Main loop processing.
     *
     * @details Algorithm:
     * -# Return if DccConfig_initialize() has not installed a configuration
     * -# Command station: run the main track scheduler
     * -# Command station: run the service track state machine while service mode is active
     * -# Command station, RailCom builds: drain the receiver and fire on_railcom_datagram_result
     * -# Decoder: dispatch packets queued by the end-bit ISR, firing the instruction callbacks
     * -# Decoder: poll the packet-timeout fail-safe
     * -# Decoder: poll the ACK pulse and stop it once 6 ms have elapsed
     */
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

#if defined(DCC_COMPILE_RAILCOM)
    /* Main track RailCom: drain decoded datagrams */
    DccRailcomCommandStation_run(&_main_railcom_context);
#endif /* DCC_COMPILE_RAILCOM */

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

    /* Dispatch packets queued by the end-bit ISR. Instruction callbacks run here in
     * the poll context (not the ISR), keeping the RailCom cutout window clear. */
    DccPacketDecoder_run();

    /* Packet-timeout fail-safe (S-9.2.4 §4): poll CV11 vs. elapsed time. */
    DccFailsafe_run();

    /* ACK pulse 6ms timing: poll timestamp and stop after 6ms elapsed */
    _run_ack_pulse();

#endif /* DCC_COMPILE_DECODER */

}

#ifdef DCC_COMPILE_COMMAND_STATION

/* =========================================================================
 * ISR entry points (remain in dcc_config — called from hardware ISR)
 * ========================================================================= */

    /**
     * @brief Shared fixed-period timer ISR entry point.
     *
     * @details Algorithm:
     * -# Toggle the main and service track output pins immediately, using the
     *    look-ahead flags computed on the previous tick, so the edges are not
     *    jittered by the state machine work that follows
     * -# Run the main track and service track bit encoder tick state machines,
     *    which compute the toggle flags for the next tick
     * -# Sample the service track current sense (when provided) for ACK detection
     */
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

#if defined(DCC_COMPILE_RAILCOM)
    /** @brief RailCom cutout one-shot timer ISR entry point: advance the cutout state machine. */
void DccConfig_railcom_oneshot_timer_isr(void) {

    DccRailcomCutout_timer_isr(&_railcom_cutout_context);

}

    /**
     * @brief Reconfigure the RailCom cutout per-state timing at runtime.
     *
     * @details Writes the five period fields of the cutout context directly rather
     * than re-initializing it, so the state machine is not reset: an in-flight
     * cutout finishes on its old timing and the new periods apply from the next
     * one. A 0 in any field selects that field's spec default.
     *
     * @verbatim
     * @param start_delay_us DELAY state length in microseconds (0 = DCC_RAILCOM_CUTOUT_START_DELAY_US).
     * @param uart_rx_delay_us SETTLING state length in microseconds (0 = DCC_RAILCOM_UART_RX_DELAY_US).
     * @param ch1_window_us Channel 1 window length in microseconds (0 = DCC_RAILCOM_CH1_WINDOW_US).
     * @param ch1_ch2_gap_us Gap between the channel windows in microseconds (0 = DCC_RAILCOM_CH1_CH2_GAP_US).
     * @param ch2_window_us Channel 2 window length in microseconds (0 = DCC_RAILCOM_CH2_WINDOW_US).
     * @endverbatim
     */
void DccConfig_set_railcom_cutout_timing(uint16_t start_delay_us, uint16_t uart_rx_delay_us, uint16_t ch1_window_us, uint16_t ch1_ch2_gap_us, uint16_t ch2_window_us) {

    /* 0 in any field selects that field's spec default, same as DccConfig_initialize.
     * Write the period fields directly (not via _initialize) so the cutout state
     * machine is NOT reset: an in-flight cutout finishes on its old timing and the
     * new periods apply from the next cutout. */
    _railcom_cutout_context.start_delay_us = start_delay_us ? start_delay_us : DCC_RAILCOM_CUTOUT_START_DELAY_US;
    _railcom_cutout_context.uart_rx_delay_us = uart_rx_delay_us ? uart_rx_delay_us : DCC_RAILCOM_UART_RX_DELAY_US;
    _railcom_cutout_context.ch1_window_us = ch1_window_us ? ch1_window_us : DCC_RAILCOM_CH1_WINDOW_US;
    _railcom_cutout_context.ch1_ch2_gap_us = ch1_ch2_gap_us ? ch1_ch2_gap_us : DCC_RAILCOM_CH1_CH2_GAP_US;
    _railcom_cutout_context.ch2_window_us = ch2_window_us ? ch2_window_us : DCC_RAILCOM_CH2_WINDOW_US;

}

    /** @brief Cancel an in-progress RailCom cutout, restoring the H-bridge. No-op when idle. */
void DccConfig_cancel_railcom_cutout(void) {

    DccRailcomCutout_cancel(&_railcom_cutout_context);

}

    /**
     * @brief Report whether a RailCom cutout is in progress.
     *
     * @return true while the cutout state machine is in any state other than IDLE.
     */
bool DccConfig_railcom_cutout_is_active(void) {

    return _railcom_cutout_context.state != DCC_RAILCOM_CUTOUT_IDLE;

}
#endif /* DCC_COMPILE_RAILCOM */

    /** @brief 100 ms housekeeping hook. Reserved; does nothing in this release. */
void DccConfig_100ms_timer_tick(void) {

    /* Reserved for periodic housekeeping. Nothing to do in this release. */

}

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef DCC_COMPILE_DECODER

    /**
     * @brief Decoder bit edge ISR entry point: forward the edge to the bit decoder.
     *
     * @verbatim
     * @param timestamp_usec Microsecond timestamp of the signal edge.
     * @endverbatim
     */
void DccConfig_decoder_edge_isr(uint32_t timestamp_usec) {

    DccBitDecoder_edge(timestamp_usec);

}

    /** @brief Re-read the address CVs into the packet decoder's match cache. */
void DccConfig_reload_address_cvs(void) {

    DccPacketDecoder_reload_address_cache();

}

#endif /* DCC_COMPILE_DECODER */
