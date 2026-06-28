/** @file dcc_user_config.h
 *  @brief Compile-gate config: COMMAND STATION, RailCom OFF.
 *
 *  Part of the compile-gate matrix (test/CMakeLists.txt). Command-station role
 *  with DCC_COMPILE_RAILCOM gated OFF. Proves the command-station build links
 *  without any RailCom symbol (cutout / railcom decoder) — i.e. RailCom code is
 *  never referenced outside its DCC_COMPILE_RAILCOM guard.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// Role: command station only. RailCom OFF.
#define DCC_COMPILE_COMMAND_STATION

// Service modes (command-station / programming-track feature).
#define DCC_COMPILE_SERVICE_MODE_DIRECT
#define DCC_COMPILE_SERVICE_MODE_PAGED
#define DCC_COMPILE_SERVICE_MODE_REGISTER
#define DCC_COMPILE_SERVICE_MODE_ADDRESS

#define DCC_COMPILE_SERVICE_MODE_TASK_DIRECT
#define DCC_COMPILE_SERVICE_MODE_TASK_PAGED
#define DCC_COMPILE_SERVICE_MODE_TASK_REGISTER
#define DCC_COMPILE_SERVICE_MODE_TASK_ADDRESS
#define DCC_COMPILE_SERVICE_MODE_TASK_DETECT

// Buffer & pool sizes (shared across all gate configs).
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT      16
#define USER_DEFINED_DCC_PREAMBLE_BITS_OPS         16
#define USER_DEFINED_DCC_MAX_LOCOS                  8
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH       4
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES       3
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA          60
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US     5000
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US     7000
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS     29

#endif /* __DCC_USER_CONFIG__ */
