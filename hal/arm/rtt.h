/*
 * Phoenix-RTOS
 *
 * SEGGER's Real Time Transfer - simplified driver
 *
 * Copyright 2023-2024 Phoenix Systems
 * Author: Gerard Swiderski, Daniel Sawka
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef HAL_ARM_RTT_H_
#define HAL_ARM_RTT_H_


typedef enum {
	rtt_dir_up,   /* tx: target -> host */
	rtt_dir_down, /* rx: host -> target */
} rtt_dir_t;


typedef enum {
	rtt_mode_skip = 0,     /* write if the whole message can be written at once; discard the message otherwise */
	rtt_mode_trim = 1,     /* write anything if possible; discard the remaining unwritten data otherwise */
	rtt_mode_blocking = 2, /* wait until writable */
} rtt_mode_t;


/* Initialize rtt internal structures */
int _hal_rttInit(void);


/* Non-blocking write to channel */
int _hal_rttWrite(unsigned int chan, const void *buf, unsigned int count);


/* Check for available space in tx */
int _hal_rttTxAvail(unsigned int chan);


/* Reset fifo pointers */
int _hal_rttReset(unsigned int chan, rtt_dir_t dir);


/* Check whether driver has been already initialized */
int _hal_rttInitialized(void);


#endif /* end of HAL_ARM_RTT_H_ */
