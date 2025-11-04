/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARMv7)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_HAL_H_
#define _HAL_HAL_H_

#include "cpu.h"
/* TODO: remove config file after cleaning up 'includes' directory */
#include "config.h"
#include "string.h"
#include "console.h"
#include "pmap.h"
#include "spinlock.h"
#include "interrupts.h"
#include "exceptions.h"
#include "timer.h"
#include "types.h"


typedef struct _hal_tls_t {
	ptr_t tls_base;
	ptr_t arm_m_tls;
	size_t tdata_sz;
	size_t tbss_sz;
	size_t tls_sz;
} hal_tls_t;


void *hal_syspageRelocate(void *data);


ptr_t hal_syspageAddr(void);


void hal_wdgReload(void);


int hal_platformctl(void *ptr);


int hal_started(void);


void _hal_start(void);


void _hal_init(void);


void hal_lockScheduler(void);


void _hal_cpuInit(void);


void _hal_platformInit(void);


#endif
