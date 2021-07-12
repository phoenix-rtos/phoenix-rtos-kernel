/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SYSPAGE_H_
#define _HAL_SYSPAGE_H_

#include "cpu.h"

#ifndef __ASSEMBLY__

#pragma pack(push, 1)

/* shareable       map is shareable between multiple bus masters. */
/* cacheable       map is cacheable, i.e. its value may be kept in cache. */
/* bufferable      map is bufferable, i.e. using write-back caching. Cacheable but non-bufferable regions use write-through policy */

enum { mAttrRead = 0x01, mAttrWrite = 0x02, maAtrrExec = 0x04, mAttrShareable = 0x08,
	   mAttrCacheable = 0x10, mAttrBufferable = 0x20, /* TODO: */ };


typedef struct {
	struct {
		u32 type;
		u32 allocCnt;
		struct {
			u32 rbar;
			u32 rasr;
		} table[16] __attribute__((aligned(8)));
		u32 map[16]; /* ((u32)-1) = map is not assigned */
	} mpu;
} syspage_hal_t;


typedef struct _syspage_map_t {
	addr_t start;
	addr_t end;
	u32 attr;

	u8 id;
	char name[8];
} syspage_map_t;


typedef struct syspage_program_t {
	addr_t start;
	addr_t end;

	u8 dmap;
	u8 imap;

	char cmdline[32];
} syspage_program_t;


typedef struct _syspage_t {
	syspage_hal_t hal;

	struct {
		addr_t text;
		size_t textsz;

		addr_t data;
		size_t datasz;

		addr_t bss;
		size_t bsssz;
	} kernel;

	size_t syspagesz;

	char *arg;

	size_t progssz;
	syspage_program_t *progs;

	size_t mapssz;
	syspage_map_t *maps;
} syspage_t;


#pragma pack(pop)


/* Syspage */
extern syspage_t *syspage;


void _hal_syspageInit(void);


#endif

#endif
