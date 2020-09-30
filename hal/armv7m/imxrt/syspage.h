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


typedef struct _syspage_map_t {
	u32 start;
	u32 end;
	u32 attr;

	u8 id;
	char name[8];
} syspage_map_t;


typedef struct syspage_program_t {
	u32 start;
	u32 end;

	u8 dmap;
	u8 imap;

	char cmdline[16];
} syspage_program_t;


typedef struct _syspage_t {
	struct {
		void *text;
		u32 textsz;

		void *data;
		u32 datasz;

		void *bss;
		u32 bsssz;
	} kernel;

	u32 syspagesz;

	char *arg;

	u32 progssz;
	syspage_program_t *progs;

	u32 mapssz;
	syspage_map_t *maps;
} syspage_t;


#pragma pack(pop)


/* Syspage */
extern syspage_t *syspage;


void _hal_syspageInit(void);


#endif

#endif
