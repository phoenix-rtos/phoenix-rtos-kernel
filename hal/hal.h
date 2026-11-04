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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_HAL_H_
#define _PH_HAL_HAL_H_

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
	size_t tls_memsz;
	size_t tls_align;
	size_t tls_sz;
} hal_tls_t;


#if HAL_TLS_VARIANT == 1

static inline size_t hal_tlsTcbSpace(const hal_tls_t *tls)
{
	size_t align = (tls->tls_align != 0U) ? tls->tls_align : 1U;
	return (HAL_TLS_TCBSZ + align - 1U) & ~(align - 1U);
}


static inline ptr_t hal_tlsTp(const hal_tls_t *tls)
{
	return tls->tls_base - hal_tlsTcbSpace(tls);
}

#elif HAL_TLS_VARIANT == 2

static inline size_t hal_tlsTcbSpace(const hal_tls_t *tls)
{
	(void)tls;
	return 0U;
}


static inline ptr_t hal_tlsTp(const hal_tls_t *tls)
{
	return tls->tls_base + tls->tls_memsz;
}

#else
#error "HAL_TLS_VARIANT must be 1 or 2"
#endif


void *hal_syspageRelocate(void *data);


ptr_t hal_syspageAddr(void);


void hal_wdgReload(void);


int hal_platformctl(void *ptr);


int hal_started(void);


void _hal_start(void);


void _hal_init(void);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "May be implemented in assembly" */
void hal_lockScheduler(void);


void _hal_cpuInit(void);


void _hal_platformInit(void);


#endif
