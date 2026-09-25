/**
 * @mainpage OpenDccCLib Documentation
 *
 *
 * @section Welcome
 *
 * <div style="display: flex; align-items: center;">
 *   <img src="Diesel.png" alt="Image" align="left" style="margin-right: 15px; width: 128px;">
 *   <span>
 *     <h2>OpenDccCLib - create DCC command station and decoder firmware in C easily.</h2>
 *   </span>
 *
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <h4>The OpenDccCLib library handles DCC packet construction, validation, decoding, service mode, and RailCom to allow the developer to focus on writing application specific code.</h4>
 *   </p>
 * </div>
 *
 * <hr>
 *
 * <h2 style="margin-left: 10px; margin-right: 10px;">Architecture</h2>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../ARCHITECTURE.pdf">Architecture</a> — As-built design: modules, feature flags, interface-struct dependency injection, execution contexts, and the scheduler / bit-encoder pipeline.
 *   </p>
 *
 * <hr>
 *
 * <h2 style="margin-left: 10px; margin-right: 10px;">Getting Started Guides</h2>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../QuickStartGuide_CommandStation.pdf">Quick Start Guide - Command Station</a> — Get a DCC command station on the track in minutes. Covers what you need, project setup, dcc_user_config.h, building and flashing, and the UART command interface.
 *   </p>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../QuickStartGuide_Decoder.pdf">Quick Start Guide - Decoder</a> — Get a DCC decoder receiving track packets. Covers what you need, project setup, dcc_user_config.h, building and flashing, how the decoder works, and customizing the callbacks.
 *   </p>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../DeveloperGuide_CommandStation.pdf">Developer Guide - Command Station</a> — In-depth walkthrough of the command-station side: project structure, ISR architecture, the drivers, bit encoder, scheduler, service mode programming, RailCom, callbacks, the application API, porting to a new MCU, unit testing, and troubleshooting.
 *   </p>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../DeveloperGuide_Decoder.pdf">Developer Guide - Decoder</a> — In-depth walkthrough of the decoder side: project structure, ISR architecture, the drivers, bit and packet decoders, CV storage and CV 29, service mode, RailCom replies, fail-safe, porting to a new MCU, unit testing, and troubleshooting.
 *   </p>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../OpenDccCLib_Brochure.pdf">Brochure</a> — Overview of DCC and OpenDccCLib: key features, protocol coverage, example platforms, architecture highlights, and test coverage.
 *   </p>
 *
 * <hr>
 *
 * <h2 style="margin-left: 10px; margin-right: 10px;">Compliance</h2>
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *     <a href="../../compliance/index.html">NMRA Compliance Dashboard</a> — Per-requirement status against the NMRA S-9.x standards, the tests that cover each one, and draft-versus-released provenance.
 *   </p>
 *
 * <hr>
 *
 * <h2 style="margin-left: 10px; margin-right: 10px;">License</h2>
 *
 *   <p style="margin-left: 50px; margin-right: 50px;">
 *       All rights reserved.
 *       Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *   </p>
 *   <ul style="margin-left: 50px; margin-right: 50px;">
 *       <li>
 *           Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *       </li>
 *       <li>
 *           Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *      </li>
 *  </ul>
 *  <p style="margin-left: 50px; margin-right: 50px;">
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *      AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *      IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *      ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *      LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *      CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *      SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *      ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *      POSSIBILITY OF SUCH DAMAGE.
 *   </p>
 *
 *
 */
