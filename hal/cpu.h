/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CPU_H_
#define _HAL_CPU_H_

#include <arch/cpu.h>
#include "spinlock.h"


/* interrupts */


extern void hal_cpuDisableInterrupts(void);


extern void hal_cpuEnableInterrupts(void);


/* performance */


extern void hal_cpuLowPower(time_t us);


extern void hal_cpuSetDevBusy(int s);


extern void hal_cpuHalt(void);


extern void hal_cpuGetCycles(cycles_t *cb);


/* bit operations */

extern unsigned int hal_cpuGetLastBit(const u32 v);


extern unsigned int hal_cpuGetFirstBit(const u32 v);


/* context management */


extern void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got);


extern void hal_cpuSetGot(void *got);


extern void *hal_cpuGetGot(void);


extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


extern int hal_cpuReschedule(struct _spinlock_t *spinlock, u32 *scp);


extern void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next);


extern void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval);


extern u32 hal_cpuGetPC(void);


extern void _hal_cpuSetKernelStack(void *kstack);


extern void *hal_cpuGetSP(cpu_context_t *ctx);


extern void *hal_cpuGetUserSP(cpu_context_t *ctx);


extern int hal_cpuSupervisorMode(cpu_context_t *ctx);


extern int hal_cpuPushSignal(void *kstack, void (*handler)(void), int n);


extern void hal_longjmp(cpu_context_t *ctx);


extern void hal_jmp(void *f, void *kstack, void *stack, int argc);


/* core management */


extern unsigned int hal_cpuGetID(void);


extern unsigned int hal_cpuGetCount(void);


extern char *hal_cpuInfo(char *info);


extern char *hal_cpuFeatures(char *features, unsigned int len);


extern void cpu_sendIPI(unsigned int cpu, unsigned int intr);


#endif
