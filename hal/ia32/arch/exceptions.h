/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IA32_EXCEPTIONS_H_
#define _HAL_IA32_EXCEPTIONS_H_

#include "types.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 6
#define EXC_PAGEFAULT 14

#define SIZE_CTXDUMP 512 /* Size of dumped context */


#pragma pack(push, 1)

typedef struct {
	u32 dr0;
	u32 dr1;
	u32 dr2;
	u32 dr3;
	u32 dr4;
	u32 dr5;
	u32 edi;
	u32 esi;
	u32 ebp;
	u32 edx;
	u32 ecx;
	u32 ebx;
	u32 eax;
	u16 gs;
	u16 fs;
	u16 es;
	u16 ds;
	u32 err;
	u32 eip;
	u32 cs;
	u32 eflags;
	u32 esp;
	u32 ss;
} exc_context_t;

#pragma pack(pop)

#endif
