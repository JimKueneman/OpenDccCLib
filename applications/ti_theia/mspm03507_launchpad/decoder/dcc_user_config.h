/*
 * dcc_user_config.h -- Project-level feature flags for the DCC library.
 *
 * The DCC library is compiled for EITHER a decoder OR a command station
 * (or both, though that is unusual).  Uncomment the role you need below.
 *
 * This file is included by the library's internal headers, so every .c
 * file in the library sees these defines automatically.
 */

#ifndef __DCC_USER_CONFIG__
#define __DCC_USER_CONFIG__

// =============================================================================
// Role Selection
//
// Enable exactly ONE of these.  The library uses #ifdef to include or
// exclude large blocks of decoder-only or command-station-only code.
// =============================================================================

// #define DCC_COMPILE_COMMAND_STATION   /* not used in this demo */
#define DCC_COMPILE_DECODER
#define DCC_COMPILE_RAILCOM           // RailCom Tx (comment out to strip)

// =============================================================================
// Decoder Configuration
// =============================================================================

#ifdef DCC_COMPILE_DECODER

/* How many function outputs the decoder tracks (F0 through F28 = 29).
 * Increase this if your hardware has more outputs; the library allocates
 * a bool array of this size. */
#define USER_DEFINED_DCC_DECODER_MAX_FUNCTIONS  29

#endif /* DCC_COMPILE_DECODER */

#endif /* __DCC_USER_CONFIG__ */
