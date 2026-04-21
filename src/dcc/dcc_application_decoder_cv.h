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
 * @file dcc_application_decoder_cv.h
 * @brief Application-layer API for decoder CV access.
 *
 * @details Provides the user-facing functions for reading and writing CVs on a
 * decoder. Wraps the internal dcc_cv_storage module through an interface struct
 * wired by dcc_config.c during DccConfig_initialize(). Application code
 * includes this header instead of the internal CV storage header.
 *
 * @author Jim Kueneman
 * @date 13 Apr 2026
 */

#ifndef __DCC_APPLICATION_DECODER_CV__
#define __DCC_APPLICATION_DECODER_CV__

#include "dcc_types.h"

#ifdef DCC_COMPILE_DECODER

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /** @brief Interface struct — wired by dcc_config.c during initialization. */
typedef struct {

        /** @brief Read a CV value from persistent storage. */
    bool (*cv_read)(uint16_t cv_number, uint8_t *value);

        /** @brief Write a CV value to persistent storage (with lock enforcement). */
    bool (*cv_write)(uint16_t cv_number, uint8_t value);

        /** @brief Check if the decoder lock is engaged. */
    bool (*is_locked)(void);

} interface_dcc_application_decoder_cv_t;

    /**
     * @brief Initialize the decoder CV application module.
     * @param interface Pointer to populated
     *        @ref interface_dcc_application_decoder_cv_t (wired by dcc_config.c).
     */
extern void DccApplicationDecoderCv_initialize(const interface_dcc_application_decoder_cv_t *interface);

    /**
     * @brief Read a CV value.
     * @param cv_number CV number (1-based per NMRA convention).
     * @param value Pointer to receive the CV value.
     * @return true if the read succeeded, false on error or NULL interface.
     */
extern bool DccApplicationDecoderCv_read(uint16_t cv_number, uint8_t *value);

    /**
     * @brief Write a CV value with decoder lock enforcement.
     * @param cv_number CV number (1-based per NMRA convention).
     * @param value Value to write.
     * @return true if the write succeeded, false if locked or on error.
     */
extern bool DccApplicationDecoderCv_write(uint16_t cv_number, uint8_t value);

    /**
     * @brief Check if the decoder lock is engaged.
     * @return true if locked (CV 15 != CV 16), false if unlocked.
     */
extern bool DccApplicationDecoderCv_is_locked(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_APPLICATION_DECODER_CV__ */
