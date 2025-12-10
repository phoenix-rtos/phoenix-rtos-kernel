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

#ifndef _PH_HAL_CPU_H_
#define _PH_HAL_CPU_H_

#define SIG_SRC_SCHED 0
#define SIG_SRC_SCALL 1

#include <arch/cpu.h>
#include "spinlock.h"


struct _hal_tls_t;


typedef ptr_t arg_t;


struct stackArg {
	const void *argp;
	size_t sz;
};

/* parasoft-begin-suppress MISRAC2012-RULE_1_5 MISRAC2012-RULE_8_8 "Implementations are arch-specific and often static-inlined in headers for performance reasons" */

/* interrupts */


void hal_cpuDisableInterrupts(void);


void hal_cpuEnableInterrupts(void);


/* performance */


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc);


int hal_cpuLowPowerAvail(void);


void hal_cpuSetDevBusy(int s);


void hal_cpuHalt(void);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
void hal_cpuGetCycles(cycles_t *cb);


/* bit operations */

unsigned int hal_cpuGetLastBit(unsigned long v);


unsigned int hal_cpuGetFirstBit(unsigned long v);


/* context management */


void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got);


void hal_cpuSetGot(void *got);


void *hal_cpuGetGot(void);


int hal_cpuCreateContext(cpu_context_t **nctx, startFn_t start, void *kstack, size_t kstacksz, void *ustack, void *arg, struct _hal_tls_t *tls);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
int hal_cpuReschedule(struct _spinlock_t *spinlock, spinlock_ctx_t *scp);


void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next);


void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
void _hal_cpuSetKernelStack(void *kstack);


void *hal_cpuGetSP(cpu_context_t *ctx);


void *hal_cpuGetUserSP(cpu_context_t *ctx);


int hal_cpuSupervisorMode(cpu_context_t *ctx);


/* oldmask: mask to be restored in sigreturn after handling the signal */
int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src);


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly code" */
void hal_jmp(void *f, void *kstack, void *ustack, size_t kargc, const arg_t *kargs);


/* core management */


unsigned int hal_cpuGetID(void);


unsigned int hal_cpuGetCount(void);


char *hal_cpuInfo(char *info);


char *hal_cpuFeatures(char *features, unsigned int len);


void hal_cpuBroadcastIPI(unsigned int intr);


__attribute__((noreturn)) void hal_cpuReboot(void);


void hal_cpuSmpSync(void);


/* thread local storage */


void hal_cpuTlsSet(struct _hal_tls_t *tls, cpu_context_t *ctx);


/* cache management */


void hal_cleanDCache(ptr_t start, size_t len);


/* stack management */


void hal_stackPutArgs(void **stackp, size_t argc, const struct stackArg *argv);

/* parasoft-end-suppress MISRAC2012-RULE_1_5 MISRAC2012-RULE_8_8 */
#endif
