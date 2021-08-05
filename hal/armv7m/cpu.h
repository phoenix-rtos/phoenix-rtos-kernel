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


#if defined(CPU_STM32L152XD) || defined(CPU_STM32L152XE) || defined(CPU_STM32L4X6)
#define CPU_STM32
#endif

#if defined(CPU_IMXRT105X) || defined(CPU_IMXRT106X) || defined(CPU_IMXRT117X)
#define CPU_IMXRT
#endif


#define NULL 0

#define SIZE_PAGE       0x200

#ifndef SIZE_USTACK
#define SIZE_USTACK     (3 * SIZE_PAGE)
#endif

#ifndef SIZE_KSTACK
#define SIZE_KSTACK     (2 * 512)
#endif

#ifdef CPU_IMXRT
#define RET_HANDLER_MSP 0xffffffe1
#define RET_THREAD_MSP  0xffffffe9
#define RET_THREAD_PSP  0xffffffed
#define HWCTXSIZE       (8 + 18)
#define USERCONTROL     0x7
#else
#define RET_HANDLER_MSP 0xfffffff1
#define RET_THREAD_MSP  0xfffffff9
#define RET_THREAD_PSP  0xfffffffd
#define HWCTXSIZE       8
#define USERCONTROL     0x3
#endif

#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((ptr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
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


typedef struct _cpu_context_t {
	u32 savesp;
	u32 fpuctx;

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

#ifdef CPU_IMXRT
	u32 s16;
	u32 s17;
	u32 s18;
	u32 s19;
	u32 s20;
	u32 s21;
	u32 s22;
	u32 s23;
	u32 s24;
	u32 s25;
	u32 s26;
	u32 s27;
	u32 s28;
	u32 s29;
	u32 s30;
	u32 s31;
#endif

	/* Saved by hardware */
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 lr;
	u32 pc;
	u32 psr;

#ifdef CPU_IMXRT
	u32 s0;
	u32 s1;
	u32 s2;
	u32 s3;
	u32 s4;
	u32 s5;
	u32 s6;
	u32 s7;
	u32 s8;
	u32 s9;
	u32 s10;
	u32 s11;
	u32 s12;
	u32 s13;
	u32 s14;
	u32 s15;
	u32 fpscr;
	u32 pad1;
#endif
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


extern void hal_cpuLowPower(time_t ms);


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


extern int hal_cpuReschedule(struct _spinlock_t *spinlock, u32 *scp);


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


void hal_jmp(void *f, void *kstack, void *stack, int argc);


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


static inline void cpu_sendIPI(unsigned int cpu, unsigned int intr)
{

}


#endif

#endif
