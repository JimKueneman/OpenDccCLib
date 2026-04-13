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
 * @file dcc_cv_storage.c
 * @brief CV read/write abstraction with decoder lock and factory reset.
 *
 * @author Jim Kueneman
 * @date 07 Apr 2026
 */

#include "dcc_cv_storage.h"

#ifdef DCC_COMPILE_DECODER

// =============================================================================
// Static state
// =============================================================================

static const interface_dcc_cv_storage_t *_interface;

// =============================================================================
// Public API
// =============================================================================

void DccCvStorage_initialize(const interface_dcc_cv_storage_t *interface) {

    _interface = interface;

}

bool DccCvStorage_read(uint16_t cv_number, uint8_t *value) {

    if (!_interface->cv_read) {

        return false;

    }

    return _interface->cv_read(cv_number, value);

}

bool DccCvStorage_write(uint16_t cv_number, uint8_t value) {

    if (!_interface->cv_write) {

        return false;

    }

    /* CV 15 and CV 16 are always writable (they control the lock) */
    if (cv_number == DCC_CV_DECODER_LOCK_1 ||
        cv_number == DCC_CV_DECODER_LOCK_2) {

        return _interface->cv_write(cv_number, value);

    }

    /* Factory reset: writing manufacturer ID (8) to CV 8 is always allowed */
    if (cv_number == DCC_CV_MANUFACTURER_ID && value == 8) {

        return _interface->cv_write(cv_number, value);

    }

    /* Check decoder lock */
    if (DccCvStorage_is_locked()) {

        return false;

    }

    return _interface->cv_write(cv_number, value);

}

bool DccCvStorage_is_locked(void) {

    uint8_t cv15_value;
    uint8_t cv16_value;

    if (!_interface->cv_read) {

        return false;

    }

    if (!_interface->cv_read(DCC_CV_DECODER_LOCK_1, &cv15_value)) {

        return false;

    }

    if (!_interface->cv_read(DCC_CV_DECODER_LOCK_2, &cv16_value)) {

        return false;

    }

    return (cv15_value != cv16_value);

}

#endif /* DCC_COMPILE_DECODER */
