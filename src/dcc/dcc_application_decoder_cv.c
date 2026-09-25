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
 * @file dcc_application_decoder_cv.c
 * @brief Application-layer implementation for decoder CV access.
 *
 * @author Jim Kueneman
 * @date 25 Sep 2026
 */

#include "dcc_application_decoder_cv.h"

#ifdef DCC_COMPILE_DECODER

// =============================================================================
// Static state
// =============================================================================

    /** @brief Stored pointer to the interface struct wired by dcc_config.c */
static const interface_dcc_application_decoder_cv_t *_interface = (void *)0;

// =============================================================================
// Public API
// =============================================================================

    /**
     * @brief Initialize the decoder CV application module.
     *
     * @details Stores the interface pointer. Called by dcc_config.c during
     * DccConfig_initialize().
     *
     * @verbatim
     * @param interface  Pointer to populated interface struct (wired by dcc_config.c).
     * @endverbatim
     */
void DccApplicationDecoderCv_initialize(const interface_dcc_application_decoder_cv_t *interface) {

    _interface = interface;

}

    /**
     * @brief Read a CV value.
     *
     * @details Returns false when uninitialized, otherwise forwards to the cv_read
     * hook (DccCvStorage_read when wired by dcc_config.c).
     *
     * @verbatim
     * @param cv_number  CV number (1-based per NMRA convention).
     * @param value      Pointer to receive the CV value.
     * @endverbatim
     *
     * @return true if the read succeeded; false on error or when the module is not initialized.
     */
bool DccApplicationDecoderCv_read(uint16_t cv_number, uint8_t *value) {

    if (!_interface) {

        return false;

    }

    return _interface->cv_read(cv_number, value);

}

    /**
     * @brief Write a CV value with decoder lock enforcement.
     *
     * @details Algorithm:
     * -# Return false when uninitialized.
     * -# Return false when the is_locked hook reports the lock engaged (this happens
     *    before the storage layer's own CV 15/16 and CV 8 exceptions).
     * -# Forward to the cv_write hook; dcc_config.c wires it to a wrapper that applies
     *    the DccCvStorage_write rules and refreshes the packet decoder's address cache.
     *
     * @verbatim
     * @param cv_number  CV number (1-based per NMRA convention).
     * @param value      Value to write.
     * @endverbatim
     *
     * @return true if the write succeeded; false if locked, on error, or when the module is not initialized.
     */
bool DccApplicationDecoderCv_write(uint16_t cv_number, uint8_t value) {

    if (!_interface) {

        return false;

    }

    if (_interface->is_locked()) {

        return false;

    }

    return _interface->cv_write(cv_number, value);

}

    /**
     * @brief Check if the decoder lock is engaged.
     *
     * @details Returns false when uninitialized, otherwise forwards to the is_locked
     * hook (DccCvStorage_is_locked when wired).
     *
     * @return true if locked (CV 15 != CV 16); false if unlocked or when the module is not initialized.
     */
bool DccApplicationDecoderCv_is_locked(void) {

    if (!_interface) {

        return false;

    }

    return _interface->is_locked();

}

#endif /* DCC_COMPILE_DECODER */
