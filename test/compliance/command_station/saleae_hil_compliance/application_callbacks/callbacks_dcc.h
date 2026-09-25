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
 * @file callbacks_dcc.h
 * @brief Application callbacks that respond to DCC library events.
 *
 * @details These functions are wired into the dcc_config_t struct in saleae_hil_compliance.c.
 * The library calls them when specific events occur (packet dispatched, RailCom
 * datagram decoded, etc.). They run in main-loop context (from DccConfig_run()), NOT
 * from ISR context, so they are safe to use UART output or other slow I/O. The
 * two exceptions are the trigger/cancel helpers that the bench calls itself from the
 * UART parser and from the 58 us timer ISR; each one says which.
 *
 * HOW TO ADD NEW CALLBACKS:
 *   1. Write your function here with a signature matching the corresponding
 *      function pointer typedef in dcc_config.h.
 *   2. Declare it in this header inside the DCC_COMPILE_COMMAND_STATION guard.
 *   3. In saleae_hil_compliance.c, replace the NULL for that field with your
 *      function pointer.
 *
 * COMPILE GUARD: These callbacks are only compiled when
 * DCC_COMPILE_COMMAND_STATION is defined in dcc_user_config.h.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */
#ifndef __CALLBACKS_DCC__
#define __CALLBACKS_DCC__

#include "dcc_lib/dcc_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DCC_COMPILE_COMMAND_STATION

    /**
     * @brief Library on_packet_sent hook: drives the PB3 test trigger for the packet under test.
     *
     * @details The library fires on_packet_sent from DccConfig_run() when the packet is
     * DISPATCHED to the bit encoder (transmit start), not after it has finished on the
     * wire. When armed via CallbacksDcc_arm_trigger(), the first NON-idle packet raises
     * PB3 so the logic analyzer can hardware-trigger on that packet. It also feeds the
     * service-mode mock decoder and the mock-ACK width test.
     *
     * @param packet  Pointer to the @ref dcc_packet_t just handed to the encoder.
     */
extern void CallbacksDcc_on_packet_sent(const dcc_packet_t *packet);

    /**
     * @brief Arm the PB3 test trigger for the next non-idle packet.
     *
     * @details Clears PB3 low and arms it so the next non-idle packet dispatched
     * drives a single clean rising edge on PB3. Called from the UART "TRIG" command
     * just before the harness sends the command under test. Cancels a pending
     * "TRIG INSERT" arm.
     */
extern void CallbacksDcc_arm_trigger(void);

    /**
     * @brief Arm the PB3 test trigger for the next MAIN-TRACK INSERT instead of the next packet.
     *
     * @details PB3 rises the moment a UART command hands its packet to the scheduler,
     * so the bench can measure the scheduler command-to-wire latency (issue #5).
     * Driven by the UART "TRIG INSERT" command; disarmed by the insert (or by a
     * plain TRIG).
     */
extern void CallbacksDcc_arm_trigger_on_insert(void);

    /**
     * @brief Fires the insert trigger (PB3) when armed.
     *
     * @details Called by the UART command parser right after a successful main-track
     * insert (one-shot or auto-refresh). No-op unless armed by CallbacksDcc_arm_trigger_on_insert().
     */
extern void CallbacksDcc_on_main_track_insert(void);

    /**
     * @brief Enable the HIL mock decoder holding one CV value.
     *
     * @details The mock ACKs Direct verify commands for cv that match value (and
     * accepts writes to it) so the bench can exercise read-back and write+verify
     * end-to-end through the real ACK path. Driven by the UART "SVC MOCKCV" command.
     *
     * @param cv     1-based CV number the mock decoder holds.
     * @param value  Initial value of that CV.
     */
extern void CallbacksDcc_mock_decoder_set(uint16_t cv, uint8_t value);
    /** @brief Disable the HIL mock decoder so no verify command is ACKed (failure-path testing). */
extern void CallbacksDcc_mock_decoder_off(void);

    /**
     * @brief HIL boundary test: select whether the width-test mock fires early or in-window.
     *
     * @details When early is true the width-test mock fires its pulse on the FIRST
     * command packet (inside the ACK blanking window) so a test can confirm the
     * library masks it; false restores normal in-window firing.
     *
     * @param early  true = fire on the first (blanked) command packet, false = fire in-window.
     */
extern void CallbacksDcc_set_mock_ack_early(bool early);

    /**
     * @brief Arm a one-shot cancel of the next in-progress RailCom cutout (HIL only, S-9.3.2 CS-008).
     *
     * @details Requests a cancel of the next cutout that BEGINS after arming. The
     * 58 us bit-timer ISR must call CallbacksDcc_railcom_cancel_tick() each tick to
     * fire it mid-cutout, which drops the PB2 cutout strobe early. Disarms after firing.
     */
extern void CallbacksDcc_arm_railcom_cancel(void);
    /**
     * @brief 58 us ISR tick for the armed RailCom cutout cancel.
     *
     * @details Call from the shared DCC timer ISR every tick. Detects a cutout that
     * begins after arming and cancels it about two ticks in. No-op when not armed.
     */
extern void CallbacksDcc_railcom_cancel_tick(void);

#if defined(DCC_COMPILE_RAILCOM)
    /**
     * @brief Library on_railcom_datagram_result hook: report a decoded RailCom datagram (HIL loopback, S-9.3.2 CS-010..015).
     *
     * @details Fires from DccConfig_run() (main loop), so it prints straight to the
     * command UART as
     *   RC RESULT: addr=<dcc addr> ch=<1|2> id=<datagram id> n=<bytes> data=<hex..>
     * and bumps the result counter behind RC STATUS.
     *
     * @param address   DCC address the library tagged the datagram with.
     * @param channel   RailCom channel the datagram was decoded from (DCC_RAILCOM_CH1 or CH2).
     * @param datagram  Pointer to the decoded @ref dcc_railcom_datagram_t.
     */
extern void CallbacksDcc_on_railcom_datagram(uint16_t address, uint8_t channel,
                                             const dcc_railcom_datagram_t *datagram);
    /**
     * @brief Number of RC RESULT lines reported since the last reset (backs RC STATUS).
     *
     * @return Count of decoded RailCom datagrams reported.
     */
extern uint32_t CallbacksDcc_railcom_result_count(void);
    /** @brief Zero the RC RESULT counter (backs RC MOCK OFF). */
extern void CallbacksDcc_railcom_reset_result_count(void);
#endif /* DCC_COMPILE_RAILCOM */

#endif /* DCC_COMPILE_COMMAND_STATION */

#ifdef __cplusplus
}
#endif

#endif /* __CALLBACKS_DCC__ */
