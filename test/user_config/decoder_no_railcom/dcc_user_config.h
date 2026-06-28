/** @file dcc_user_config.h
 *  @brief Compile-gate config: MOBILE DECODER, RailCom OFF.
 *
 *  Part of the compile-gate matrix (test/CMakeLists.txt). Mobile-decoder role
 *  with DCC_COMPILE_RAILCOM gated OFF — the minimal real loco decoder. Proves
 *  the decoder build links with no RailCom symbol (no railcom encoder), i.e.
 *  RailCom is never referenced outside its guard on the decoder side.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// Role: mobile decoder only. RailCom OFF.
#define DCC_COMPILE_DECODER

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

    /* Decoder received-packet FIFO depth (>= 2; one slot reserved). The
     * end-bit ISR enqueues; DccConfig_run drains and dispatches. */
#define USER_DEFINED_DCC_DECODER_PACKET_QUEUE_DEPTH      8

#endif /* __DCC_USER_CONFIG__ */
