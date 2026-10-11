#ifndef _HAB_API_H_
#define _HAB_API_H_

#include "hal/types.h"
#include "hab_rvt.h"

/*
 * Performs early initialization of HAB API support. Must be called exactly once at startup.
 * The memory range for loading images needs to be explicitly stated because it may vary on different IMX chips.
 * Note: both minLoadAddr and maxLoadAddr must be >= VADDR_MIN and < VADDR_USR_MAX.
 *
 * minLoadAddr - start of virtual memory range for loading images for verification
 * maxLoadAddr - past-the-end of virtual memory range for loading images for verification
 */
int _hab_api_preinit(u32 minLoadAddr, u32 maxLoadAddr);


/*
 * Report HAB status and (optionally) events that have been logged.
 * Events will always be output from event index 0 until the buffer is full or all events have been output.
 * Data for all events is concatenated together - header of each event contains its size,
 * allowing them to be unambiguously separated by caller.
 *
 * status - output pointer to store security status (HAB_SUCCESS, HAB_WARNING, HAB_FAILURE)
 * cfg - output pointer to store security configuration (one of HAB_CFG_*)
 * state - output pointer to store state of the security state machine (one of HAB_STATE_*)
 * eventsBuf - (optional) pointer to store events data
 * bufSize - (optional) [in] pointer to size of eventsBuf, [out] size of events written to the buffer
 *  Note: if either eventsBuf or bufSize is NULL, events data will not be output.
 * Returns 0 if operation succeeded, < 0 if it failed.
 */
int hab_api_reportAll(hab_status_t *status, hab_config_t *cfg, hab_state_t *state, u8 *eventsBuf, size_t *bufSize);


/* Verify data using built-in HAB functions
 *
 * data - pointer to data for verification
 * size - size of data
 * ivtOffset - offset within data where IVT is stored
 *
 * Returns 0 if verification succeeded, -EINVAL if passed buffer is invalid or doesn't contain a valid IVT,
 * -EPERM if verification failed.
 */
int hab_api_verify(const void *data, size_t size, size_t ivtOffset);


#endif /* _HAB_API_H_ */
