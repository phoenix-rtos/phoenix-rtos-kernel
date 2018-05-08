/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (via SBI)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "console.h"
#include "cpu.h"
#include "syspage.h"
#include "spinlock.h"


struct {
	spinlock_t lock;
} console_common;


u64 sbi_call(u64 n, u64 arg0, void *arg1, void *arg2)
{
	register u64 a0 asm ("a0") = (arg0);
	register void *a1 asm ("a1") = (arg1);
	register void *a2 asm ("a2") = (arg2);
	register void *a7 asm ("a7") = (void *)n;

	__asm__ volatile ("ecall" \
		: "+r" (a0) \
		: "r" (a1), "r" (a2), "r" (a7) \
		: "memory");

	return a0;
}


static void _console_print(const char *s)
{

	for (; *s; s++)
		sbi_call(1, *s, 0, 0);

#if 0
	u32 t;
	register void *a0 asm ("a0") = (uintptr_t)(arg0);

	__asm__ volatile (" \
		la s0, %0; \
	1: \
		lbu a0, (s0); \
		beqz a0, 1f; \
		li a7, 1; \
		ecall; \
		add s0, s0, 1; \
		j 1b; \
	1:"
	:
	: "r" (t)
	: "s0");
#endif
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD) {
		_console_print("\033[1m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else if (attr != ATTR_USER) {
		_console_print("\033[36m");
		_console_print(s);
		_console_print("\033[0m");
	}
	else
		_console_print(s);
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{

	return;
}
