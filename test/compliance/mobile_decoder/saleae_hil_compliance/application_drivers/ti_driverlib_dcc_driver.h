/*
 * ti_driverlib_dcc_driver.h -- Hardware driver for DCC on MSPM0G3507.
 *
 * PORTING GUIDE: To run on a different MCU, create a new driver file that
 * implements the same functions below using your MCU's HAL.  Then
 * update decoder.c to #include your file and wire the function pointers
 * in the dcc_config_t struct.
 *
 * This is the decoder-only version: it provides lock/unlock (interrupt
 * disable/enable) and a microsecond timestamp.  The command-station
 * version also needs timer start/stop/set_period and track power control.
 */

#ifndef __TI_DRIVERLIB_DCC_DRIVER__
#define __TI_DRIVERLIB_DCC_DRIVER__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start the free-running timestamp timer. */
extern void TI_DccDriver_initialize(void);

/* Disable all interrupts (used by the library to protect shared state). */
extern void TI_DccDriver_lock_shared_resources(void);

/* Re-enable interrupts. */
extern void TI_DccDriver_unlock_shared_resources(void);

/* Return the current time in microseconds from a free-running counter.
 * The counter wraps at ~4295 seconds (32-bit overflow). */
extern uint32_t TI_DccDriver_get_timestamp_usec(void);

/* Blocking microsecond delay for the RailCom Tx bit-bang, accurate at the 4 us
 * bit period via a 20 MHz (50 ns/tick) one-shot timer. */
extern void TI_DccDriver_railcom_delay_us(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif /* __TI_DRIVERLIB_DCC_DRIVER__ */
