/** @file dcc_user_config.h
 *  @brief Test configuration for OpenDccCLib
 *
 *  This is the test build configuration. Both roles and all service modes
 *  are enabled to maximize test coverage.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection -- both enabled for full test coverage
// =============================================================================

#define DCC_COMPILE_COMMAND_STATION
#define DCC_COMPILE_DECODER
#define DCC_COMPILE_ACCESSORY_DECODER

// =============================================================================
// Service Mode Selection -- all enabled for full test coverage
// =============================================================================

#define DCC_COMPILE_SERVICE_MODE_DIRECT
#define DCC_COMPILE_SERVICE_MODE_PAGED
#define DCC_COMPILE_SERVICE_MODE_REGISTER
#define DCC_COMPILE_SERVICE_MODE_ADDRESS

// =============================================================================
// Service Mode Task Selection -- all enabled for full test coverage
// =============================================================================

#define DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
#define DCC_COMPILE_SERVICE_MODE_TASK_PAGED
#define DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
#define DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
#define DCC_COMPILE_SERVICE_MODE_TASK_DETECT

// =============================================================================
// Command Station Buffer & Pool Sizes
// =============================================================================

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

// =============================================================================
// Decoder Configuration
// =============================================================================

/** @brief Max function number supported (F0-F28 = 29) */
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS   29

#endif /* __DCC_USER_CONFIG__ */
