/** @file dcc_user_config.h
 *  @brief Compile-gate config: ACCESSORY DECODER, RailCom OFF.
 *
 *  Part of the compile-gate matrix (test/CMakeLists.txt). Accessory-decoder role
 *  with DCC_COMPILE_RAILCOM gated OFF. Proves the accessory build links with no
 *  RailCom symbol present.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// Role: accessory decoder only. RailCom OFF.
#define DCC_COMPILE_ACCESSORY_DECODER

// Buffer & pool sizes (shared across all gate configs).
#define USER_DEFINED_DCC_SCHEDULER_SLOT_COUNT      16
#define USER_DEFINED_DCC_MAX_LOCOS                  8
#define USER_DEFINED_DCC_RAILCOM_BUFFER_DEPTH       4
#define USER_DEFINED_DCC_SERVICE_MODE_RETRIES       3
#define USER_DEFINED_DCC_ACK_THRESHOLD_MA          60
#define USER_DEFINED_DCC_ACK_MIN_DURATION_US     5000
#define USER_DEFINED_DCC_ACK_MAX_DURATION_US     7000
#define USER_DEFINED_DCC_ACK_DROPOUT_TOLERANCE_US 116
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS     29

#endif /* __DCC_USER_CONFIG__ */
