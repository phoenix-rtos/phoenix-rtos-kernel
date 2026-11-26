/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU context struct
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_ARCH_IA32_CONTEXT_H_
#define _PH_ARCH_IA32_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


#pragma pack(push, 1)

typedef struct {
	__u16 controlWord, _controlWord;
	__u16 statusWord, _statusWord;
	__u16 tagWord, _tagWord;
	__u32 fip;
	__u32 fips;
	__u32 fdp;
	__u16 fds, _fds;
	__u8 fpuContext[8][10]; /* 80-bit long doubles as bytes */
} fpu_context_t;


/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct _cpu_context_t {
	__u32 savesp;
	__u32 edi;
	__u32 esi;
	__u32 ebp;
	__u32 edx;
	__u32 ecx;
	__u32 ebx;
	__u32 eax;
	__u16 gs;
	__u16 fs;
	__u16 es;
	__u16 ds;
	fpu_context_t fpuContext;
	__u32 cr0Bits;
	__u32 eip; /* eip, cs, eflags, esp, ss saved by CPU on interrupt */
	__u32 cs;
	__u32 eflags;
	__u32 esp;
	__u32 ss;
} cpu_context_t;

#pragma pack(pop)


#endif


#endif
