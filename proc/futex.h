#ifndef _PROC_FUTEX_H_
#define _PROC_FUTEX_H_

#include "include/types.h"

/* Implementation inspired by: https://github.com/openbsd/src/blob/master/sys/kern/sys_futex.c */

#define FUTEX_SLEEPQUEUES_BITS 6
#define FUTEX_SLEEPQUEUES_SIZE (1U << FUTEX_SLEEPQUEUES_BITS)
#define FUTEX_SLEEPQUEUES_MASK (FUTEX_SLEEPQUEUES_SIZE - 1)

int proc_futexWait(u32 *address, u32 value, time_t timeout);
int proc_futexWakeup(u32 *address, u32 n_threads);

#endif
