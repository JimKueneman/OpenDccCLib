/** @file dcc_user_config.h
 *  @brief User-editable project configuration for OpenDccCLib
 *
 *  REQUIRED: Copy this file to your project's include path and edit.
 *    Arduino:     place next to your .ino file (sketch dir is on include path)
 *    PlatformIO:  place in src/ directory
 *    STM32 Cube:  place in Core/Inc/ directory
 *    CMake/Make:  place anywhere, add -I flag pointing to its directory
 *
 *  ALL values in this file are MANDATORY for the enabled role(s).
 *  The library will not compile if any are missing.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection -- enable one or both
// =============================================================================

#define DCC_COMPILE_COMMAND_STATION
// #define DCC_COMPILE_DECODER
// #define DCC_COMPILE_ACCESSORY_DECODER

// =============================================================================
// Service Mode Selection (requires DCC_COMPILE_COMMAND_STATION)
// =============================================================================

#define DCC_COMPILE_SERVICE_MODE_DIRECT
#define DCC_COMPILE_SERVICE_MODE_PAGED
// #define DCC_COMPILE_SERVICE_MODE_REGISTER
// #define DCC_COMPILE_SERVICE_MODE_ADDRESS

// =============================================================================
// Command Station Buffer & Pool Sizes
// =============================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

/** @brief Max concurrent active packets in the scheduler */
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT    16

/** @brief Max locos with auto-refresh */
#define USER_DEFINED_DCC_MAX_LOCOS                8

/** @brief RailCom receive ring buffer depth */
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH     4

/** @brief Number of retries for service mode operations */
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES     3

/** @brief ACK detection threshold in milliamps */
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA        60

/** @brief ACK minimum pulse duration in microseconds */
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US   5000

/** @brief ACK maximum pulse duration in microseconds */
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US   7000

#endif /* DCC_COMPILE_COMMAND_STATION */

// =============================================================================
// Decoder Configuration
// =============================================================================

#ifdef DCC_COMPILE_DECODER

/** @brief Max function number supported (F0-F28 = 29) */
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS   29

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_USER_CONFIG__ */
