// dcc_user_config.h
//
// This is the FIRST file to customize when porting OpenDccCLib to a new project.
// It controls which library features are compiled and sets buffer/pool sizes.
//
// Every project that uses OpenDccCLib must provide this file. The library headers
// include it via "dcc_user_config.h", so it must be on your compiler's include path.

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection
//
// Enable exactly one role (or both if your device is a combined station+decoder).
// DCC_COMPILE_COMMAND_STATION  -- builds the packet scheduler, bit encoder,
//                                 service mode, and RailCom support.
// DCC_COMPILE_DECODER          -- builds the bit decoder, CV handling, and
//                                 packet parser for receiving DCC commands.
// =============================================================================

#define DCC_COMPILE_COMMAND_STATION
// #define DCC_COMPILE_DECODER

// =============================================================================
// Service Mode Selection (requires DCC_COMPILE_COMMAND_STATION)
//
// Each flag compiles in one NMRA service-mode programming method. Disable any
// you do not need to save flash. All four are independent of each other.
//   DIRECT   -- CV direct byte/bit read/write  (most common, recommended)
//   PAGED    -- paged mode for older decoders
//   REGISTER -- register mode for very old decoders
//   ADDRESS  -- short-address-only programming (legacy)
// =============================================================================

#define DCC_COMPILE_SERVICE_MODE_DIRECT
#define DCC_COMPILE_SERVICE_MODE_PAGED
#define DCC_COMPILE_SERVICE_MODE_REGISTER
#define DCC_COMPILE_SERVICE_MODE_ADDRESS

// =============================================================================
// Service Mode Task Selection (requires DCC_COMPILE_COMMAND_STATION)
// =============================================================================

#define DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
#define DCC_COMPILE_SERVICE_MODE_TASK_PAGED
#define DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
#define DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
#define DCC_COMPILE_SERVICE_MODE_TASK_DETECT

// =============================================================================
// Command Station Buffer & Pool Sizes
//
// Tune these for your MCU's available RAM. Larger values support more locos
// and more concurrent operations but use more memory.
// =============================================================================

#ifdef DCC_COMPILE_COMMAND_STATION

// Max concurrent packets the scheduler can hold. Each slot is ~20 bytes.
// 24 handles 10 locos with speed+function refresh plus a few one-shot packets.
// Increase if you see scheduler-full errors; decrease to save RAM.
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT    24

// Max locos with automatic speed/function refresh. Each loco uses one
// scheduler slot permanently. Must be <= SCHEDULER_SLOT_COUNT minus headroom
// for one-shot packets (accessory, CV, etc.).
#define USER_DEFINED_DCC_MAX_LOCOS               10

// Depth of the RailCom receive ring buffer. Only matters if you wire
// railcom_uart_read in dcc_config_t. 4 is usually enough.
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH     4

// Number of retries for service mode read/write/verify before giving up.
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES     3

// Service mode ACK detection: the decoder pulls >= this many milliamps
// for between ACK_MIN and ACK_MAX microseconds to signal success.
// NMRA S-9.2.3 recommends 60 mA, 5-7 ms. Adjust if your current sense
// circuit has different scaling.
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA        60
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US   5000
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US   7000

// Bridge brief current-sense dropouts during an ACK (noisy motor loads /
// comparator chatter). 0 = strict consecutive-high. Keep it a small fraction of
// the 6 ms ACK (single-digit samples at the 58 us sample period) or it defeats
// the width discrimination. ~116 us = 2 samples.
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116

#endif /* DCC_COMPILE_COMMAND_STATION */

#endif /* __DCC_USER_CONFIG__ */
