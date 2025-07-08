#ifndef _PROC_FUTEX_H_
#define _PROC_FUTEX_H_

#include "include/types.h"

/* Implementation inspired by: https://github.com/openbsd/src/blob/master/sys/kern/sys_futex.c */

#define FUTEX_SLEEPQUEUES_BITS  6
#define FUTEX_SLEEPQUEUES_SIZE  (1U << FUTEX_SLEEPQUEUES_BITS)
#define FUTEX_SLEEPQUEUES_MASK  (FUTEX_SLEEPQUEUES_SIZE - 1)

struct _thread_t;

typedef struct _futex_t {
    struct _futex_t *prev, *next;
    addr_t address;

    struct _thread_t *waiting_thread;
} futex_t;

typedef struct {
    futex_t *futex_list;
} futex_sleepqueue_t;

int futex_wait(unsigned int *address, unsigned int value, time_t timeout);
int futex_wakeup(unsigned int *address, unsigned int n_threads);

#endif
