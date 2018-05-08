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

#define SIZE_PAGE       0x1000
#define SIZE_PDIR       0x4000
#define SIZE_CACHE_LINE 64

/* Size of thread kernel stack */
#define SIZE_KSTACK     4 * 512

#define NULL 0

#define USR_MODE        0x10
#define FIQ_MODE        0x11
#define IRQ_MODE        0x12
#define SVC_MODE        0x13	/* reset mode */
#define ABT_MODE        0x17
#define UND_MODE        0x1B
#define SYS_MODE        0x1F
#define MODE_MASK       0x1F
#define NO_ABORT        0x100	/* mask to disable Abort Exception */
#define NO_IRQ          0x80	/* mask to disable IRQ */
#define NO_FIQ          0x40	/* mask to disable FIQ */
#define	NO_INT          (NO_IRQ | NO_FIQ)	/* mask to disable IRQ and FIQ */
#define THUMB_STATE     0x20


#ifndef __ASSEMBLY__


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((addr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += sizeof(t); \
	} while (0)


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u32 cycles_t;

typedef u64 usec_t;
typedef s64 offs_t;

typedef unsigned int size_t;
typedef unsigned long long time_t;

typedef u64 id_t;

/* Object identifier - contains server port and object id */
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;


typedef struct _cpu_context_t {
	u32 savesp;
	u32 padding;

	/* Floating point coprocessor context */
	u32 fpsr;
	u32 freg[32 * 2];

	u32 psr;

	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;

	u32 fp;
	u32 ip;
	u32 sp;
	u32 lr;

	u32 pc;
} cpu_context_t;


/* platform specific syscall */


extern int hal_platformctl(void *ptr);


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile ("cpsid if");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile ("cpsie aif");
}


/* performance */


static inline time_t hal_cpuLowPower(time_t ms)
{
	return 0;
}


static inline void hal_cpuSetDevBusy(int s)
{
}


static inline void hal_cpuHalt(void)
{
	__asm__ volatile ("wfe");
}


extern void hal_cpuGetCycles(cycles_t *cb);


/* memory management */


extern void hal_cpuFlushDataCache(addr_t vaddr);


extern void hal_cpuInvalVA(addr_t vaddr);


extern void hal_cpuBranchInval(void);


extern void hal_cpuICacheInval(void);

/*
static inline void *hal_cpuGetFaultAddr(void)
{
	return NULL;
}
*/

extern addr_t hal_cpuGetUserTT(void);


extern void hal_cpuSetUserTT(addr_t tt);


extern void hal_cpuSetContextId(u32 id);


extern u32 hal_cpuGetContextId(void);

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


static inline void hal_cpuSetGot(cpu_context_t *ctx, void *got)
{
}


/* Function creates new cpu context on top of given thread kernel stack */
extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


struct _spinlock_t;


extern int hal_cpuReschedule(struct _spinlock_t *spinlock);


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next /*+ sizeof(u32)*/;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
	ctx->r0 = retval;
}


static inline u32 hal_cpuGetPC(void)
{
	void *pc;

	__asm__ volatile ("mov %0, pc" : "=r" (pc));

	return (u32)pc;
}


static inline void hal_cpuGuard(cpu_context_t *ctx, void *addr)
{

}


extern void _hal_cpuSetKernelStack(void *kstack);


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->sp;
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


extern void hal_longjmp(cpu_context_t *ctx);


extern void hal_jmp(void *f, void *kstack, void *stack, int argc);

/* core management */


extern u32 hal_cpuGetMIDR(void);


extern u32 hal_cpuGetPFR0(void);


extern u32 hal_cpuGetPFR1(void);


static inline unsigned int hal_cpuGetID(void)
{
	return 0;
}


static inline unsigned int hal_cpuGetCount(void)
{
	return 1;
}


extern void _hal_cpuInitCores(void);


extern char *hal_cpuInfo(char *info);


extern char *hal_cpuFeatures(char *features, unsigned int len);


static inline void hal_wdgReload(void)
{

}


extern void _hal_cpuInit(void);


extern void _hal_platformInit(void);


#endif

#endif
