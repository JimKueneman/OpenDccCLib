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
 * @date 13 Apr 2026
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
 * @verbatim
 * @param interface  Pointer to populated interface struct (wired by dcc_config.c).
 * @endverbatim
 */
void DccApplicationDecoderCv_initialize(const interface_dcc_application_decoder_cv_t *interface) {

    _interface = interface;

}

/**
 * @verbatim
 * @param cv_number  CV number (1-based per NMRA convention).
 * @param value      Pointer to receive the CV value.
 * @endverbatim
 * @return true if the read succeeded, false on error or NULL interface.
 */
bool DccApplicationDecoderCv_read(uint16_t cv_number, uint8_t *value) {

    if (!_interface) {

        return false;

    }

    return _interface->cv_read(cv_number, value);

}

/**
 * @verbatim
 * @param cv_number  CV number (1-based per NMRA convention).
 * @param value      Value to write.
 * @endverbatim
 * @return true if the write succeeded, false if locked or on error.
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
 * @return true if locked (CV 15 != CV 16), false if unlocked.
 */
bool DccApplicationDecoderCv_is_locked(void) {

    if (!_interface) {

        return false;

    }

    return _interface->is_locked();

}

#endif /* DCC_COMPILE_DECODER */
