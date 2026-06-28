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
 * @file dcc_types.h
 * @brief Core type definitions, structures, and configuration constants for the
 * OpenDccCLib library.
 *
 * @details Defines all fundamental data types used across the library: address
 * types, speed modes, packet buffers, scheduler slots, and service mode results.
 * Includes dcc_user_config.h and validates all USER_DEFINED_DCC_* constants.
 * All memory is statically allocated at compile time.
 *
 * @author Jim Kueneman
 * @date 28 Jun 2026
 */

#ifndef __DCC_TYPES__
#define __DCC_TYPES__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// =============================================================================
// Include user configuration
// =============================================================================

#ifdef __has_include
  #if __has_include("dcc_user_config.h")
  #include "dcc_user_config.h"
  #elif __has_include("../../dcc_user_config.h")
  #include "../../dcc_user_config.h"
  #elif __has_include("../../../dcc_user_config.h")
  #include "../../../dcc_user_config.h"
  #elif __has_include("../../../../dcc_user_config.h")
  #include "../../../../dcc_user_config.h"
  #else
  #error "dcc_user_config.h not found. Copy from templates/typical/ to your project include path."
  #endif
#else
  #include "dcc_user_config.h"
#endif

// RailCom is an add-on to a role, not a role itself: it only compiles alongside a
// command-station, decoder, or accessory-decoder build. Catch a lone definition early.
#if defined(DCC_COMPILE_RAILCOM) && \
    !defined(DCC_COMPILE_COMMAND_STATION) && \
    !defined(DCC_COMPILE_DECODER) && \
    !defined(DCC_COMPILE_ACCESSORY_DECODER)
  #error "DCC_COMPILE_RAILCOM requires DCC_COMPILE_COMMAND_STATION, DCC_COMPILE_DECODER, or DCC_COMPILE_ACCESSORY_DECODER"
#endif

// =============================================================================
// Validate user-configurable constants (Command Station)
// =============================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

#ifndef USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT
#error "USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT must be defined in dcc_user_config.h"
#endif
#if USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT < 1
#error "USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT must be >= 1"
#endif

#ifndef USER_DEFINED_DCC_MAX_LOCOS
#error "USER_DEFINED_DCC_MAX_LOCOS must be defined in dcc_user_config.h"
#endif
#if USER_DEFINED_DCC_MAX_LOCOS < 1
#error "USER_DEFINED_DCC_MAX_LOCOS must be >= 1"
#endif

#ifndef USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH
#error "USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH must be defined in dcc_user_config.h"
#endif
#if USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH < 1
#error "USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH must be >= 1"
#endif

#ifndef USER_DEFINED_DCC_SERVICE_MODE_RETRIES
#error "USER_DEFINED_DCC_SERVICE_MODE_RETRIES must be defined in dcc_user_config.h"
#endif

#ifndef USER_DEFINED_DCC_ACK_THRESHOLD_MA
#error "USER_DEFINED_DCC_ACK_THRESHOLD_MA must be defined in dcc_user_config.h"
#endif

#ifndef USER_DEFINED_DCC_ACK_MIN_DURATION_US
#error "USER_DEFINED_DCC_ACK_MIN_DURATION_US must be defined in dcc_user_config.h"
#endif

#ifndef USER_DEFINED_DCC_ACK_MAX_DURATION_US
#error "USER_DEFINED_DCC_ACK_MAX_DURATION_US must be defined in dcc_user_config.h"
#endif

#endif /* DCC_COMPILE_COMMAND_STATION */

// =============================================================================
// Validate user-configurable constants (Decoder)
// =============================================================================

#ifdef DCC_COMPILE_DECODER

#ifndef USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS
#error "USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS must be defined in dcc_user_config.h"
#endif
#if USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS < 1
#error "USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS must be >= 1"
#endif

#ifndef USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH
#error "USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH must be defined in dcc_user_config.h"
#endif
#if USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH < 2
#error "USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH must be >= 2 (one slot is reserved)"
#endif

#endif /* DCC_COMPILE_DECODER */

// =============================================================================
// Address types
// =============================================================================

    /** @brief DCC address (0-10239) */
typedef uint16_t dcc_address_t;

    /** @brief DCC address type classification */
typedef enum {

    DCC_ADDRESS_SHORT,              /**< 7-bit, 1-127 */
    DCC_ADDRESS_LONG,               /**< 14-bit, 128-10239 */
    DCC_ADDRESS_BROADCAST,          /**< address 0 */
    DCC_ADDRESS_IDLE,               /**< address 255 */
    DCC_ADDRESS_ACCESSORY,          /**< 9-bit basic accessory */
    DCC_ADDRESS_ACCESSORY_EXTENDED  /**< 11-bit extended accessory (signal) */

} dcc_address_type_enum;

// =============================================================================
// Speed types
// =============================================================================

    /** @brief DCC speed step modes */
typedef enum {

    DCC_SPEED_MODE_14,   /**< 14 speed steps */
    DCC_SPEED_MODE_28,   /**< 28 speed steps */
    DCC_SPEED_MODE_128   /**< 128 speed steps (advanced operations) */

} dcc_speed_mode_enum;

// =============================================================================
// Packet buffer
// =============================================================================

    /** @brief Maximum bytes in a DCC packet (2 addr + 3 instruction + 1 XOR) */
#define DCC_PACKET_MAX_BYTES 6

    /** @brief A single DCC packet ready for transmission */
typedef struct {

    uint8_t data[DCC_PACKET_MAX_BYTES]; /**< Raw packet bytes including XOR */
    uint8_t byte_count;                 /**< Number of valid bytes in data[] */
    uint8_t preamble_bits;              /**< Preamble one-bits (14 ops, 20 service) */
    uint8_t repeat_count;               /**< Times to send before dropping */

} dcc_packet_t;

// =============================================================================
// Time / Date types
// =============================================================================

    /** @brief Day of week for the model Time command (S-9.2.1 §2.3.6.2) */
typedef enum {

    DCC_DAY_OF_WEEK_MONDAY,         /**< 0 = Monday */
    DCC_DAY_OF_WEEK_TUESDAY,        /**< 1 */
    DCC_DAY_OF_WEEK_WEDNESDAY,      /**< 2 */
    DCC_DAY_OF_WEEK_THURSDAY,       /**< 3 */
    DCC_DAY_OF_WEEK_FRIDAY,         /**< 4 */
    DCC_DAY_OF_WEEK_SATURDAY,       /**< 5 */
    DCC_DAY_OF_WEEK_SUNDAY,         /**< 6 = Sunday */
    DCC_DAY_OF_WEEK_NOT_SUPPORTED   /**< 7 = not supported */

} dcc_day_of_week_enum;

// =============================================================================
// Scheduler types (Command Station)
// =============================================================================

    /** @brief Packet priority levels for the scheduler */
typedef enum {

    DCC_PRIORITY_ESTOP,     /**< Emergency stop -- highest priority */
    DCC_PRIORITY_SPEED,     /**< Speed commands */
    DCC_PRIORITY_FUNCTION,  /**< Function commands */
    DCC_PRIORITY_ACCESSORY, /**< Accessory commands */
    DCC_PRIORITY_CV,        /**< CV programming (ops-mode) */
    DCC_PRIORITY_IDLE       /**< Idle packets -- lowest priority */

} dcc_priority_enum;

    /** @brief Scheduler slot tag values for duplicate combining */
typedef enum {

    DCC_TAG_SPEED,          /**< Speed command */
    DCC_TAG_FUNC_GROUP_1,   /**< Function group 1 (FL, F1-F4) */
    DCC_TAG_FUNC_GROUP_2A,  /**< Function group 2a (F5-F8) */
    DCC_TAG_FUNC_GROUP_2B,  /**< Function group 2b (F9-F12) */
    DCC_TAG_FUNC_F13_F20,   /**< Feature expansion F13-F20 */
    DCC_TAG_FUNC_F21_F28,   /**< Feature expansion F21-F28 */
    DCC_TAG_FUNC_F29_F36,   /**< Feature expansion F29-F36 */
    DCC_TAG_FUNC_F37_F44,   /**< Feature expansion F37-F44 */
    DCC_TAG_FUNC_F45_F52,   /**< Feature expansion F45-F52 */
    DCC_TAG_FUNC_F53_F60,   /**< Feature expansion F53-F60 */
    DCC_TAG_FUNC_F61_F68,   /**< Feature expansion F61-F68 */
    DCC_TAG_ACCESSORY,      /**< Accessory command */
    DCC_TAG_CV,             /**< CV programming */
    DCC_TAG_CONSIST,        /**< Consist address */
    DCC_TAG_BINARY_STATE,   /**< Binary state control */
    DCC_TAG_ANALOG_FUNC     /**< Analog function control */

} dcc_tag_enum;

    /** @brief A single scheduler slot holding a packet and its metadata */
typedef struct {

    dcc_packet_t packet;        /**< The DCC packet */
    dcc_priority_enum priority; /**< Packet priority level */
    dcc_address_t address;      /**< DCC address for duplicate combining */
    dcc_tag_enum tag;           /**< Sub-key for duplicate combining */
    bool auto_refresh;          /**< true = keep in refresh cycle indefinitely */
    bool active;                /**< true = slot is in use */

} dcc_scheduler_slot_t;

// =============================================================================
// Service mode types
// =============================================================================

    /** @brief Service mode operation result */
typedef enum {

    DCC_SERVICE_MODE_SUCCESS,      /**< Operation completed successfully */
    DCC_SERVICE_MODE_NO_ACK,       /**< No acknowledgement from decoder */
    DCC_SERVICE_MODE_VERIFY_FAIL,  /**< Verify operation failed */
    DCC_SERVICE_MODE_BUSY,         /**< Service mode is already in progress */
    DCC_SERVICE_MODE_ERROR,        /**< General error (e.g., no current sense) */
    DCC_SERVICE_MODE_NOT_IN_SERVICE_MODE /**< Not in service mode */

} dcc_service_mode_result_t;

    /** @brief Callback for primitive service mode operation step completion. */
typedef void (*dcc_service_mode_step_callback_t)(dcc_service_mode_result_t result);

// =============================================================================
// Service mode task types (task orchestrator layer)
// =============================================================================

    /** @brief Decoder type for register mode CV mapping (mobile vs accessory). */
typedef enum {

    DCC_DECODER_TYPE_MOBILE,
    DCC_DECODER_TYPE_ACCESSORY,

} dcc_decoder_type_enum;

    /** @brief Service mode type identifier (result of decoder mode detection). */
typedef enum {

    DCC_SERVICE_MODE_TYPE_DIRECT,
    DCC_SERVICE_MODE_TYPE_PAGED,
    DCC_SERVICE_MODE_TYPE_REGISTER,
    DCC_SERVICE_MODE_TYPE_ADDRESS,
    DCC_SERVICE_MODE_TYPE_UNKNOWN,

} dcc_service_mode_type_enum;

    /** @brief Supported-mode flags returned by detect_mode (bitmask in a uint8_t).
     *         supported_modes == 0 means no service mode was detected. */
#define DCC_SERVICE_MODE_SUPPORTED_DIRECT   (1u << 0)
#define DCC_SERVICE_MODE_SUPPORTED_PAGED    (1u << 1)
#define DCC_SERVICE_MODE_SUPPORTED_REGISTER (1u << 2)
#define DCC_SERVICE_MODE_SUPPORTED_ADDRESS  (1u << 3)

    /** @brief Current phase of a task orchestrator operation. */
typedef enum {

    DCC_TASK_PHASE_READ,
    DCC_TASK_PHASE_WRITE,
    DCC_TASK_PHASE_VERIFY,

} dcc_task_phase_enum;

    /** @brief Callback: task CV operation complete. value = CV byte found or validated. */
typedef void (*dcc_service_mode_task_on_complete_callback_t)(dcc_service_mode_result_t result, uint8_t value);

    /** @brief Callback: detect_mode complete. supported_modes = bitmask of
     *         DCC_SERVICE_MODE_SUPPORTED_* flags (0 = none detected). */
typedef void (*dcc_service_mode_task_on_detect_callback_t)(dcc_service_mode_result_t result, uint8_t supported_modes);

    /** @brief Callback: progress notification during a multi-step task operation. */
typedef void (*dcc_service_mode_task_on_progress_callback_t)(dcc_task_phase_enum phase, uint8_t current_step, uint8_t estimated_steps);

// =============================================================================
// RailCom types
// =============================================================================

    /** @brief Maximum data bytes in a RailCom datagram */
#define DCC_RAILCOM_DATAGRAM_MAX_BYTES 6

    /** @brief RailCom channel identifier */
typedef enum {

    DCC_RAILCOM_CH1,  /**< Channel 1 (2 bytes, address/mobility feedback) */
    DCC_RAILCOM_CH2   /**< Channel 2 (up to 6 bytes, CV readback, etc.) */

} dcc_railcom_channel_enum;

    /** @brief A decoded RailCom datagram */
typedef struct {

    uint8_t datagram_id;                        /**< Datagram ID (0-15) */
    uint8_t data[DCC_RAILCOM_DATAGRAM_MAX_BYTES]; /**< Decoded data bytes */
    uint8_t count;                              /**< Number of valid data bytes */
    bool valid;                                 /**< true if datagram decoded ok */

} dcc_railcom_datagram_t;

// =============================================================================
// Bit encoder state (Command Station)
// =============================================================================

    /** @brief Bit encoder ISR state machine states */
typedef enum {

    DCC_BIT_STATE_IDLE,           /**< Not transmitting */
    DCC_BIT_STATE_PREAMBLE,       /**< Sending preamble one-bits */
    DCC_BIT_STATE_START_BIT,      /**< Sending packet start zero-bit */
    DCC_BIT_STATE_DATA,           /**< Sending data bits of a byte */
    DCC_BIT_STATE_XOR,            /**< Sending XOR error detection byte */
    DCC_BIT_STATE_END_BIT         /**< Sending packet end one-bit */

} dcc_bit_state_enum;

// =============================================================================
// Decoder types
// =============================================================================

    /** @brief A decoded DCC packet with parsed fields */
typedef struct {

    dcc_address_t address;          /**< Decoded address */
    dcc_address_type_enum type;     /**< Address type */
    uint8_t instruction[3];         /**< Instruction bytes (after address) */
    uint8_t instruction_count;      /**< Number of valid instruction bytes */
    bool valid;                     /**< true if XOR check passed */

} dcc_decoded_packet_t;

// =============================================================================
// RailCom encoder types (Decoder / Accessory Decoder)
// =============================================================================

    /** @brief Decoded CV29 configuration flags, handed to the app after a CV29 write.
     *  The library decodes the register; the app acts only on the features it supports. */
typedef struct {

    bool direction_reversed;        /**< Bit 0: locomotive direction reversed */
    bool speed_steps_28_128;        /**< Bit 1: true = 28/128-step (FL in function group), false = 14-step */
    bool power_source_conversion;   /**< Bit 2: analog power-source conversion enabled */
    bool railcom_enabled;           /**< Bit 3: RailCom (bi-directional comms) enabled */
    bool speed_table_enabled;       /**< Bit 4: speed table (CV67-94) selected */
    bool extended_address;          /**< Bit 5: long (CV17/18) addressing */
    bool accessory_decoder;         /**< Bit 7: accessory (vs multifunction) decoder */

} dcc_cv29_flags_t;

    /** @brief RailCom datagram for encoder (decoder-side response) */
typedef struct {

    uint8_t datagram_id;                        /**< Datagram ID (0-15) */
    uint8_t data[DCC_RAILCOM_DATAGRAM_MAX_BYTES]; /**< Data bytes to encode */
    uint8_t count;                              /**< Number of valid data bytes */

} dcc_railcom_response_t;

    /** @brief Kind of app-handled RailCom Channel 2 request (POM is library-internal). */
typedef enum {

    DCC_RAILCOM_APP_REQUEST_SEARCH,   /**< EXT location search (ID3) -- app returns its location */
    DCC_RAILCOM_APP_REQUEST_FILL      /**< XF refuel (ID7) -- app returns tank/refuel data */

} dcc_railcom_app_request_type_enum;

    /** @brief Outcome of filling a RailCom Channel 2 reply. The library turns the status into
     *  the matching S-9.3.2 token; the application never encodes a token itself. */
typedef enum {

    DCC_RAILCOM_REPLY_DATA,   /**< Success: the response datagram holds the data to transmit */
    DCC_RAILCOM_REPLY_ACK,    /**< Success, no data: transmit the ACK token */
    DCC_RAILCOM_REPLY_BUSY,   /**< Cannot answer yet, retry later: transmit the BUSY token */
    DCC_RAILCOM_REPLY_NACK,   /**< Cannot comply: transmit the NACK token */
    DCC_RAILCOM_REPLY_NONE    /**< Nothing to send: leave Channel 2 empty */

} dcc_railcom_reply_status_enum;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DCC_TYPES__ */
