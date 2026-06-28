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
 * @file dcc_cv_storage.h
 * @brief CV read/write abstraction with decoder lock and factory reset.
 *
 * @details Wraps the user-provided CV read/write callbacks with decoder lock
 * checking (CV 15/16 must match for writes to succeed) and factory reset
 * detection (writing 8 to CV 8).
 *
 * @author Jim Kueneman
 * @date 27 Jun 2026
 */

#ifndef __DCC_CV_STORAGE__
#define __DCC_CV_STORAGE__

#include "dcc_types.h"
#include "dcc_defines.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct -- dependencies injected by dcc_config.c */
typedef struct {

        /** @brief Read CV from persistent storage. REQUIRED. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Write CV to persistent storage. REQUIRED. */
    bool (*cv_write)(uint16_t cv_number, uint8_t value);

        /** @brief Reduce a CV29 write to the features this decoder actually supports. REQUIRED.
         *  The library forces reserved bit 6, decodes the request, and calls this; the app
         *  leaves set only the features it implements, clearing the rest (RailCom, analog
         *  conversion, speed table, ...). The library re-encodes what remains and stores that.
         *  Per S-9.2.2 an unsupported feature bit must never be settable, and only the app
         *  knows its support. */
    void (*cv29_apply_supported_features)(dcc_cv29_flags_t *flags);

        /** @brief Restore configuration to factory defaults. OPTIONAL (NULL = none).
         *  Invoked when a write command targets CV8 (the read-only Manufacturer ID);
         *  per S-9.2.2 the value is unchangeable, so the write only triggers a reset. */
    void (*factory_reset)(void);

        /** @brief Read a byte from the indexed CV window (CV257-512). OPTIONAL (NULL = no
         *  indexed support). page = CV31:CV32; offset = cv_number - 257. */
    bool (*cv_read_indexed)(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t *value);

        /** @brief Write a byte to the indexed CV window (CV257-512). OPTIONAL (NULL or a
         *  false return = page not supported -> NACK). */
    bool (*cv_write_indexed)(uint8_t page_hi, uint8_t page_lo, uint8_t offset, uint8_t value);

} interface_dcc_cv_storage_t;

    /**
     * @brief Initialize the CV storage module.
     * @param interface Pointer to populated interface struct.
     */
extern void DccCvStorage_initialize(const interface_dcc_cv_storage_t *interface);

    /**
     * @brief Read a CV value.
     * @param cv_number CV number (1-based).
     * @param value Pointer to receive the value.
     * @return true if read succeeded.
     */
extern bool DccCvStorage_read(uint16_t cv_number, uint8_t *value);

    /**
     * @brief Write a CV value with decoder lock enforcement.
     * @param cv_number CV number (1-based).
     * @param value Value to write.
     * @return true if write succeeded (false if locked or hardware failure).
     *
     * @details Checks decoder lock (CV 15 must equal CV 16) before allowing
     * writes. CV 15 and CV 16 themselves are always writable. Writing the
     * manufacturer ID value (8) to CV 8 is allowed for factory reset.
     */
extern bool DccCvStorage_write(uint16_t cv_number, uint8_t value);

    /**
     * @brief Check if the decoder lock is engaged.
     * @return true if locked (CV 15 != CV 16), false if unlocked.
     */
extern bool DccCvStorage_is_locked(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_CV_STORAGE__ */
