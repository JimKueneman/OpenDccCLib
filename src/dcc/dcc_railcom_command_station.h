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
 * @file dcc_railcom_command_station.h
 * @brief RailCom 4/8 decoding, cutout management, and receive buffer.
 *
 * @details Decodes RailCom bytes received during cutout windows. Manages
 * a circular buffer of decoded datagrams tagged with the DCC address of the
 * packet that preceded the cutout. Disabled at runtime if the interface's
 * uart_read is NULL (dcc_config.c leaves it NULL when the config has no
 * RailCom UART).
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#ifndef __DCC_RAILCOM_COMMAND_STATION__
#define __DCC_RAILCOM_COMMAND_STATION__

#include "dcc_types.h"
#include "dcc_defines.h"

#if defined(DCC_COMPILE_RAILCOM) && defined(DCC_COMPILE_COMMAND_STATION)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Read one byte from RailCom UART. Returns true if byte available. */
    bool (*uart_read)(uint8_t *byte);

        /**
         * @brief User callback: RailCom datagram decoded. NULL = no notification.
         *        Fired from DccRailcomCommandStation_run(), NOT ISR context.
         */
    void (*on_datagram)(uint16_t address, uint8_t channel, const dcc_railcom_datagram_t *datagram);

} interface_dcc_railcom_command_station_t;

    /** @brief Instance context for the RailCom decoder module.
     *
     *  @details Holds all per-instance state that was formerly file-scope static.
     *  Allocate one of these per DCC output channel that uses RailCom.
     */
typedef struct {

        /** @brief Injected dependencies; NULL until DccRailcomCommandStation_initialize(). */
    const interface_dcc_railcom_command_station_t *interface;

        /** @brief Circular buffer of decoded datagrams; the oldest is overwritten when full. */
    dcc_railcom_datagram_t buffer[USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH];

        /** @brief Index the next decoded datagram is written at. */
    uint8_t buffer_head;

        /** @brief Index of the oldest unread datagram. */
    uint8_t buffer_tail;

        /** @brief Number of unread datagrams in the buffer. */
    uint8_t buffer_count;

        /** @brief DCC address the next decoded reply is tagged with. */
    dcc_address_t cutout_address;

        /** @brief Set from ISR context by DccRailcomCommandStation_begin_cutout(); DccRailcomCommandStation_run() clears it and drains the UART. */
    volatile bool cutout_pending;

} dcc_railcom_command_station_context_t;

        /**
         * @brief Initialize the RailCom decoder module.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         * @param interface Pointer to populated @ref interface_dcc_railcom_command_station_t struct.
         */
    extern void DccRailcomCommandStation_initialize(dcc_railcom_command_station_context_t *context, const interface_dcc_railcom_command_station_t *interface);

        /**
         * @brief Main loop processing for the RailCom decoder.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         *
         * @details When a cutout is pending, drains the UART, decodes Channel 1
         * and Channel 2, pushes each valid datagram into the buffer and fires
         * on_datagram for it. Does nothing if uart_read is NULL or no cutout is
         * pending.
         */
    extern void DccRailcomCommandStation_run(dcc_railcom_command_station_context_t *context);

        /**
         * @brief Begin a RailCom cutout window for a given address.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         * @param address The DCC address associated with this cutout.
         *
         * @details Records the address the decoded reply will be tagged with and
         * marks a cutout pending; the UART is read and decoded by the next
         * DccRailcomCommandStation_run(). Safe from ISR context. dcc_config.c
         * calls it from the cutout-complete hook, so the bytes are already in
         * the UART when run() reads them.
         */
    extern void DccRailcomCommandStation_begin_cutout(dcc_railcom_command_station_context_t *context, dcc_address_t address);

        /**
         * @brief End the current RailCom cutout window.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         *
         * @details Currently a no-op kept for API symmetry; all processing happens
         * in DccRailcomCommandStation_run().
         */
    extern void DccRailcomCommandStation_end_cutout(dcc_railcom_command_station_context_t *context);

        /**
         * @brief Read the next decoded RailCom datagram from the buffer.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         * @param datagram Pointer to @ref dcc_railcom_datagram_t to fill with decoded data.
         * @return true if a datagram was available, false if buffer empty.
         */
    extern bool DccRailcomCommandStation_read(dcc_railcom_command_station_context_t *context, dcc_railcom_datagram_t *datagram);

        /**
         * @brief Return the number of decoded datagrams available in the buffer.
         * @param context Pointer to @ref dcc_railcom_command_station_context_t instance.
         * @return Number of datagrams waiting to be read.
         */
    extern uint8_t DccRailcomCommandStation_available(const dcc_railcom_command_station_context_t *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_RAILCOM && DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_RAILCOM_COMMAND_STATION__ */
