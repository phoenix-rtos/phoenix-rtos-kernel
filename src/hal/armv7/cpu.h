/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CPU_H_
#define _HAL_CPU_H_

#define NULL 0

#define SIZE_PAGE       0x200

#define SIZE_KSTACK     (2 * 512)
#define SIZE_USTACK     (2 * SIZE_PAGE)

#define RET_HANDLER_MSP 0xfffffff1
#define RET_THREAD_MSP  0xfffffff9
#define RET_THREAD_PSP  0xfffffffd

#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		hal_memcpy(&(v), ustack, sizeof(t)); \
		ustack += (sizeof(t) + 3) & ~0x3; \
	} while (0)


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u32 cycles_t;

typedef s64 offs_t;

typedef unsigned int size_t;
typedef unsigned long long time_t;

typedef u32 ptr_t;

/* Object identifier - contains server port and object id */
typedef u32 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;

/* TODO - add FPU context for iMXRT */
typedef struct _cpu_context_t {
	u32 savesp;

	/* Saved by ISR */
	u32 psp;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 r11;
	u32 irq_ret;

	/* Saved by hardware */
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 lr;
	u32 pc;
	u32 psr;
} cpu_context_t;


extern volatile cpu_context_t *_cpu_nctx;


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile ("cpsid if");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile ("cpsie if");
}


/* performance */


extern time_t hal_cpuLowPower(time_t ms);


extern void hal_cpuSetDevBusy(int s);


static inline void hal_cpuHalt(void)
{
	__asm__ volatile ("\
		wfi; \
		nop; ");
}


extern void hal_cpuGetCycles(cycles_t *cb);


/* bit operations */


static inline unsigned int hal_cpuGetLastBit(const u32 v)
{
	int pos;

	__asm__ volatile ("clz %0, %1" : "=r" (pos) : "r" (v));

	return 31 - pos;
}


static inline unsigned int hal_cpuGetFirstBit(const u32 v)
{
	unsigned pos;

	__asm__ volatile ("\
		rbit %0, %1; \
		clz  %0, %0;" : "=r" (pos) : "r" (v));

	return (pos);
}


/* context management */


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	ctx->r9 = (u32)got;
}


static inline void hal_cpuSetGot(void *got)
{
	__asm__ volatile ("mov r9, %0" :: "r" (got));
}


static inline void *hal_cpuGetGot(void)
{
	void *got;

	__asm__ volatile ("mov %0, r9" : "=r" (got));

	return got;
}


extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


struct _spinlock_t;


extern int hal_cpuReschedule(struct _spinlock_t *spinlock);


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
	ctx->r0 = retval;
}


static inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return 0;
}


static inline u32 hal_cpuGetPC(void)
{
	void *pc;

	__asm__ volatile ("mov %0, pc" : "=r" (pc));

	return (u32)pc;
}


static inline void hal_cpuDataBarrier(void)
{
	__asm__ volatile ("dmb");
}


static inline void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile ("dsb");
}


static inline void hal_cpuInstrBarrier(void)
{
	__asm__ volatile ("isb");
}



static inline void _hal_cpuSetKernelStack(void *kstack)
{
}


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->psp;
}


static inline int hal_cpuPushSignal(cpu_context_t *ctx, void (*handler)(void), int sig)
{
	return 0;
}


static inline void hal_longjmp(cpu_context_t *ctx)
{
	__asm__ volatile
	(" \
		cpsid if; \
		str %1, [%0]; \
		bl _hal_invokePendSV; \
		cpsie if; \
	1:	b 1b"
	:
	: "r" (&_cpu_nctx), "r" (ctx)
	: "memory");
}


static inline void hal_jmp(void *f, void *kstack, void *stack, int argc)
{
	if (stack == NULL) {
		__asm__ volatile
		(" \
			mov sp, %1; \
			subs %0, #1; \
			bmi 1f; \
			pop {r0};\
			subs %0, #1; \
			bmi 1f; \
			pop {r1}; \
			subs %0, #1; \
			bmi 1f; \
			pop {r2}; \
			subs %0, #1; \
			bmi 1f; \
			pop {r3}; \
		1: \
			bx %2"
		: "+r" (argc)
		: "r" (kstack), "r" (f)
		: "r0", "r1", "r2", "r3", "sp");
	}
	else {
		__asm__ volatile
		(" \
			msr msp, %2; \
			subs %1, #1; \
			bmi 1f; \
			ldr r0, [%0], #4; \
			subs %1, #1; \
			bmi 1f; \
			ldr r1, [%0], #4; \
			subs %1, #1; \
			bmi 1f; \
			ldr r2, [%0], #4; \
			subs %1, #1; \
			bmi 1f; \
			ldr r3, [%0], #4; \
		1: \
			msr psp, %0; \
			mov r4, #3; \
			msr control, r4; \
			bx %3"
		: "+r"(stack), "+r" (argc)
		: "r" (kstack), "r" (f)
		: "r0", "r1", "r2", "r3", "r4", "sp");
	}
}


static inline void hal_cpuGuard(cpu_context_t *ctx, void *addr)
{
}



/* core management */


static inline unsigned int hal_cpuGetID(void)
{
	return 0;
}


static inline unsigned int hal_cpuGetCount(void)
{
	return 1;
}


extern void hal_cpuRestart(void);


extern char *hal_cpuInfo(char *info);


extern char *hal_cpuFeatures(char *features, unsigned int len);


extern void hal_wdgReload(void);


extern void _hal_cpuInit(void);

#endif

#endif
