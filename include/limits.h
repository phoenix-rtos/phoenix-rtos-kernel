/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Limits
 *
 * Copyright 2021 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_LIMITS_H_
#define _PH_LIMITS_H_


#define HOST_NAME_MAX 255U


/* Limit on size of core image. */
#define RLIMIT_CORE 0
/* Limit on CPU time per process. */
#define RLIMIT_CPU 1
/* Limit on data segment size. */
#define RLIMIT_DATA 2
/* Limit on file size. */
#define RLIMIT_FSIZE 3
/* Limit on number of open files. */
#define RLIMIT_NOFILE 4
/* Limit on stack size. */
#define RLIMIT_STACK 5
/* Limit on address space size. */
#define RLIMIT_AS 6


#define RLIM_INFINITY ((rlim_t) - 1)

/* We can represent all limits.  */
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY


typedef unsigned int rlim_t;


struct rlimit {
	rlim_t rlim_cur;
	rlim_t rlim_max;
};


typedef struct {
	/* To be extended with target kinds for partition, thread, uid etc. in the future */
	enum { limit_target_process } kind;
	union {
		int pid;
	};
} limit_target_t;

#endif
