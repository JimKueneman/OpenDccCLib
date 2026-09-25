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
 * @file dcc_railcom_command_station.c
 * @brief RailCom 4/8 decoding, cutout management, and receive buffer.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_railcom_command_station.h"
#include "dcc_railcom_utilities.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_COMMAND_STATION)

#include <string.h>

// =============================================================================
// Static helpers
// =============================================================================

    /**
     * @brief Append a decoded datagram to the circular buffer.
     *
     * @details When the buffer is full the oldest unread datagram is dropped
     * (tail advanced) to make room, so the newest reply is never lost.
     *
     * @param context Decoder context.
     * @param datagram Datagram to copy into the buffer.
     */
static void _buffer_push(dcc_railcom_command_station_context_t *context, const dcc_railcom_datagram_t *datagram) {

    if (context->buffer_count >= USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH) {

        context->buffer_tail = (context->buffer_tail + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
        context->buffer_count--;

    }

    memcpy(&context->buffer[context->buffer_head], datagram, sizeof(dcc_railcom_datagram_t));
    context->buffer_head = (context->buffer_head + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
    context->buffer_count++;

}

    /**
     * @brief Decode RailCom Channel 1 (first 2 bytes → 12 data bits).
     *
     * @details Needs at least DCC_RAILCOM_CH1_MAX_BYTES bytes; on a valid
     * decode the datagram is buffered and on_datagram fires with DCC_RAILCOM_CH1.
     * An invalid codeword drops the channel silently.
     *
     * @param context Decoder context.
     * @param raw_bytes Raw UART bytes from cutout.
     * @param raw_count Number of raw bytes received.
     */
static void _decode_channel_1(dcc_railcom_command_station_context_t *context, const uint8_t *raw_bytes, uint8_t raw_count) {

    dcc_railcom_datagram_t datagram;
    memset(&datagram, 0, sizeof(datagram));

    if (raw_count < DCC_RAILCOM_CH1_MAX_BYTES) {

        return;

    }

    if (!DccRailcomUtilities_decode_ch1(raw_bytes[0], raw_bytes[1], &datagram)) {

        return;

    }

    _buffer_push(context, &datagram);

    if (context->interface->on_datagram) {

        context->interface->on_datagram(context->cutout_address, DCC_RAILCOM_CH1, &datagram);

    }

}

    /**
     * @brief Decode RailCom Channel 2 (remaining bytes, up to 6 → up to 36 data bits).
     *
     * @details Skips the first DCC_RAILCOM_CH1_MAX_BYTES bytes; needs at least
     * one byte beyond them. On a valid decode the datagram is buffered and
     * on_datagram fires with DCC_RAILCOM_CH2. A failed decode drops the
     * channel silently.
     *
     * @param context Decoder context.
     * @param raw_bytes Raw UART bytes from cutout (Channel 1 bytes first).
     * @param raw_count Number of raw bytes received.
     */
static void _decode_channel_2(dcc_railcom_command_station_context_t *context, const uint8_t *raw_bytes, uint8_t raw_count) {

    dcc_railcom_datagram_t datagram;
    memset(&datagram, 0, sizeof(datagram));

    if (raw_count <= DCC_RAILCOM_CH1_MAX_BYTES) {

        return;

    }

    if (!DccRailcomUtilities_decode_ch2(&raw_bytes[DCC_RAILCOM_CH1_MAX_BYTES],
            (uint8_t)(raw_count - DCC_RAILCOM_CH1_MAX_BYTES), &datagram)) {

        return;

    }

    _buffer_push(context, &datagram);

    if (context->interface->on_datagram) {

        context->interface->on_datagram(context->cutout_address, DCC_RAILCOM_CH2, &datagram);

    }

}

    /**
     * @brief Drain the UART for one cutout and decode both channels.
     *
     * @details Reads until uart_read() returns false or 8 bytes
     * (DCC_RAILCOM_CH1_MAX_BYTES + DCC_RAILCOM_CH2_MAX_BYTES) are collected,
     * then hands the buffer to the Channel 1 and Channel 2 decoders. Nothing
     * is decoded when no bytes were received.
     *
     * @param context Decoder context.
     */
static void _process_cutout(dcc_railcom_command_station_context_t *context) {

    uint8_t raw_bytes[DCC_RAILCOM_CH1_MAX_BYTES + DCC_RAILCOM_CH2_MAX_BYTES];
    uint8_t raw_count = 0;

    while (raw_count < (DCC_RAILCOM_CH1_MAX_BYTES + DCC_RAILCOM_CH2_MAX_BYTES)) {

        if (!context->interface->uart_read(&raw_bytes[raw_count])) {

            break;

        }

        raw_count++;

    }

    if (raw_count == 0) {

        return;

    }

    _decode_channel_1(context, raw_bytes, raw_count);
    _decode_channel_2(context, raw_bytes, raw_count);

}

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the RailCom decoder module.
     *
     * @details Stores the interface and empties the datagram buffer; no cutout
     * is pending and the tag address is 0.
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance.
     * @param interface Pointer to populated interface_dcc_railcom_command_station_t struct.
     * @endverbatim
     */
void DccRailcomCommandStation_initialize(dcc_railcom_command_station_context_t *context, const interface_dcc_railcom_command_station_t *interface) {

    context->interface = interface;
    context->buffer_head = 0;
    context->buffer_tail = 0;
    context->buffer_count = 0;
    context->cutout_address = 0;
    context->cutout_pending = false;

}

    /**
     * @brief Main loop processing for the RailCom decoder.
     *
     * @details Returns at once if uart_read is NULL (RailCom disabled) or no
     * cutout is pending. Otherwise clears cutout_pending and processes the
     * cutout's bytes (see _process_cutout()).
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance.
     * @endverbatim
     */
void DccRailcomCommandStation_run(dcc_railcom_command_station_context_t *context) {

    if (!context->interface->uart_read) {

        return;

    }

    if (!context->cutout_pending) {

        return;

    }

    context->cutout_pending = false;
    _process_cutout(context);

}

    /**
     * @brief Begin a RailCom cutout window for a given address.
     *
     * @details Stores the tag address and sets cutout_pending; nothing is read
     * here, DccRailcomCommandStation_run() does the work.
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance.
     * @param address The DCC address associated with this cutout.
     * @endverbatim
     */
void DccRailcomCommandStation_begin_cutout(dcc_railcom_command_station_context_t *context, dcc_address_t address) {

    context->cutout_address = address;
    context->cutout_pending = true;

}

    /**
     * @brief End the current RailCom cutout window (no-op).
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance (unused).
     * @endverbatim
     */
void DccRailcomCommandStation_end_cutout(dcc_railcom_command_station_context_t *context) {

    /* Intentionally empty — processing happens in run() after cutout ends. */
    (void)context;

}

    /**
     * @brief Read the next decoded RailCom datagram from the buffer.
     *
     * @details Copies the oldest unread datagram out and advances the tail.
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance.
     * @param datagram Pointer to dcc_railcom_datagram_t to fill with decoded data.
     * @endverbatim
     *
     * @return true if a datagram was copied out, false if the buffer was empty.
     */
bool DccRailcomCommandStation_read(dcc_railcom_command_station_context_t *context, dcc_railcom_datagram_t *datagram) {

    if (context->buffer_count == 0) {

        return false;

    }

    memcpy(datagram, &context->buffer[context->buffer_tail], sizeof(dcc_railcom_datagram_t));
    context->buffer_tail = (context->buffer_tail + 1) % USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH;
    context->buffer_count--;

    return true;

}

    /**
     * @brief Return the number of decoded datagrams available in the buffer.
     *
     * @verbatim
     * @param context Pointer to dcc_railcom_command_station_context_t instance.
     * @endverbatim
     *
     * @return Number of datagrams waiting to be read.
     */
uint8_t DccRailcomCommandStation_available(const dcc_railcom_command_station_context_t *context) {

    return context->buffer_count;

}

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_COMMAND_STATION */
