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
 * @date 27 Jun 2026
 */

#include "dcc_cv_storage.h"

#ifdef DCC_COMPILE_DECODER

#include <string.h>   /* memset */

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

/* Resolve the indexed-window page pointer (CV31 high, CV32 low). */
static bool _index_page(uint8_t *page_hi, uint8_t *page_lo) {

    return _interface->cv_read(DCC_CV_INDEX_HIGH, page_hi) &&
           _interface->cv_read(DCC_CV_INDEX_LOW, page_lo);

}

/* Decode a CV29 byte into the named feature flags. */
static void _decode_cv29(uint8_t value, dcc_cv29_flags_t *flags) {

    flags->direction_reversed      = (value & DCC_CV29_DIRECTION_BIT)        != 0;
    flags->speed_steps_28_128      = (value & DCC_CV29_SPEED_STEPS_BIT)      != 0;
    flags->power_source_conversion = (value & DCC_CV29_ANALOG_ENABLE_BIT)    != 0;
    flags->railcom_enabled         = (value & DCC_CV29_RAILCOM_ENABLE_BIT)   != 0;
    flags->speed_table_enabled     = (value & DCC_CV29_SPEED_TABLE_BIT)      != 0;
    flags->extended_address        = (value & DCC_CV29_EXTENDED_ADDRESS_BIT) != 0;
    flags->accessory_decoder       = (value & DCC_CV29_ACCESSORY_BIT)        != 0;

}

/* Re-encode the named feature flags into a CV29 byte (reserved bit 6 stays 0). */
static uint8_t _encode_cv29(const dcc_cv29_flags_t *flags) {

    uint8_t value = 0;

    value |= flags->direction_reversed      ? DCC_CV29_DIRECTION_BIT        : 0;
    value |= flags->speed_steps_28_128      ? DCC_CV29_SPEED_STEPS_BIT      : 0;
    value |= flags->power_source_conversion ? DCC_CV29_ANALOG_ENABLE_BIT    : 0;
    value |= flags->railcom_enabled         ? DCC_CV29_RAILCOM_ENABLE_BIT   : 0;
    value |= flags->speed_table_enabled     ? DCC_CV29_SPEED_TABLE_BIT      : 0;
    value |= flags->extended_address        ? DCC_CV29_EXTENDED_ADDRESS_BIT : 0;
    value |= flags->accessory_decoder       ? DCC_CV29_ACCESSORY_BIT        : 0;

    return value;

}

bool DccCvStorage_read(uint16_t cv_number, uint8_t *value) {

    if (!_interface->cv_read) {

        return false;

    }

    /* Indexed window (CV257-512): a 256-byte porthole into the page selected by
     * CV31/CV32.  Route to cv_read_indexed; no hook / unresolved page -> false. */
    if (cv_number >= DCC_CV_INDEXED_FIRST && cv_number <= DCC_CV_INDEXED_LAST) {

        uint8_t page_hi, page_lo;

        if (!_interface->cv_read_indexed || !_index_page(&page_hi, &page_lo)) {

            return false;

        }

        return _interface->cv_read_indexed(page_hi, page_lo,
            (uint8_t)(cv_number - DCC_CV_INDEXED_FIRST), value);

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

    /* CV 8 is the read-only Manufacturer ID (S-9.2.2): its value is unchangeable.
     * A write command to it is a reset trigger, not a value write -- invoke
     * factory_reset (if provided) and do NOT forward the write. */
    if (cv_number == DCC_CV_MANUFACTURER_ID && value == 8) {

        if (_interface->factory_reset) {

            _interface->factory_reset();

        }

        return true;

    }

    /* Check decoder lock */
    if (DccCvStorage_is_locked()) {

        return false;

    }

    /* Indexed window (CV257-512): route through CV31/CV32 to cv_write_indexed. */
    if (cv_number >= DCC_CV_INDEXED_FIRST && cv_number <= DCC_CV_INDEXED_LAST) {

        uint8_t page_hi, page_lo;

        if (!_interface->cv_write_indexed || !_index_page(&page_hi, &page_lo)) {

            return false;

        }

        return _interface->cv_write_indexed(page_hi, page_lo,
            (uint8_t)(cv_number - DCC_CV_INDEXED_FIRST), value);

    }

    /* CV29 configuration register: force the reserved bit (the library's one universal
     * responsibility), then hand the decoded config to the app so it can clear any feature
     * it does not implement.  Per S-9.2.2 an unsupported feature bit must not be settable;
     * only the app knows what it supports, so it -- not the library -- filters the bits.
     * What the app leaves set is re-encoded and stored. */
    if (cv_number == DCC_CV_CONFIG) {

        dcc_cv29_flags_t flags;
        memset(&flags, 0, sizeof(flags));

        value &= (uint8_t)~DCC_CV29_RESERVED_BIT;       /* bit 6 reserved -> 0 */
        _decode_cv29(value, &flags);

        if (_interface->cv29_apply_supported_features) {

            _interface->cv29_apply_supported_features(&flags);
            value = _encode_cv29(&flags);

        }

        return _interface->cv_write(cv_number, value);

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
