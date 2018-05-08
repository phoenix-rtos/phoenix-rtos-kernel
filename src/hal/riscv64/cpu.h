/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CPU_H_
#define _HAL_CPU_H_


/* Size of thread kernel stack */
#define SIZE_KSTACK 4 * 512


#define NULL 0


#ifndef __ASSEMBLY__


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= sizeof(t); \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		if (n == 0) \
			ustack += 4; \
		v = *(t *)ustack; \
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

typedef u64 addr_t;
typedef u64 cycles_t;

typedef u64 usec_t;
typedef s64 offs_t;

typedef unsigned long size_t;
typedef unsigned long long time_t;

/* Object identifier - contains server port and object id */
typedef u64 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;


#pragma pack(1)

/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct {
	u64 pc;
	u64 savesp;
	u64 esp;
} cpu_context_t;

#pragma pack(8)


/* platform specific syscall */


extern int hal_platformctl(void *ptr);


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
}


static inline void hal_cpuEnableInterrupts(void)
{
}


/* performance */


static inline time_t hal_cpuLowPower(time_t ms)
{
	return 0;
}


static inline void hal_cpuHalt(void)
{
}


static inline void hal_cpuGetCycles(void *cb)
{
	return;
}


/* memory management */


static inline void hal_cpuFlushTLB(void *vaddr)
{
	__asm__ ("sfence.vma"::);
}


static inline void *hal_cpuGetFaultAddr(void)
{
	return (void *)NULL;
}


static inline void hal_cpuSwitchSpace(addr_t cr3)
{
	return;
}


/* bit operations */


static inline unsigned int hal_cpuGetLastBit(u64 v)
{
	int lb = 63;

	if (!(v & 0xffffffff00000000L)) {
		lb -= 32;
		v = (v << 32);
	}

	if (!(v & 0xffff000000000000)) {
		lb -= 16;
		v = (v << 16);
	}

	if (!(v & 0xff00000000000000)) {
		lb -= 8;
		v = (v << 8);
	}

	if (!(v & 0xf000000000000000)) {
		lb -= 4;
		v = (v << 4);	
	}

	if (!(v & 0xc000000000000000)) {	
		lb -= 2;
		v = (v << 2);
	}

	if (!(v & 0x8000000000000000))
		lb -= 1;

	return lb;
}


static inline unsigned int hal_cpuGetFirstBit(u64 v)
{
	int fb = 0;

	if (!(v & 0xffffffffL)) {
		fb += 32;
		v = (v >> 32);
	}

	if (!(v & 0xffff)) {
		fb += 16;
		v = (v >> 16);
	}

	if (!(v & 0xff)) {
		fb += 8;
		v = (v >> 8);
	}

	if (!(v & 0xf)) {
		fb += 4;
		v = (v >> 4);
	}

	if (!(v & 0x3)) {
		fb += 2;
		v = (v >> 2);
	}

	if (!(v & 0x01))
		fb += 1;

	return fb;
}


static inline u32 hal_cpuSwapBits(const u32 v)
{
	u32 data = v;

	return data;
}


/* debug */


static inline void hal_cpuSetBreakpoint(void *addr)
{
	return;
}


/* context management */


static inline void hal_cpuSetGot(cpu_context_t *ctx, void *got)
{
}


/* Function creates new cpu context on top of given thread kernel stack */
extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


struct _spinlock_t;


extern int hal_cpuReschedule(struct _spinlock_t *lock);


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
//	curr->savesp = (u32)next + sizeof(u32);
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
//	ctx->eax = retval;
}


extern void _hal_cpuSetKernelStack(void *kstack);


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->esp;
}


static inline void hal_cpuGuard(cpu_context_t *ctx, void *addr)
{
}


static inline void hal_longjmp(cpu_context_t *ctx)
{
}


static inline void hal_jmp(void *f, void *kstack, void *stack, int argc)
{
}


/* core management */


static inline void hal_cpuid(u32 leaf, u32 index, u32 *ra, u32 *rb, u32 *rc, u32 *rd)
{
}


static inline unsigned int hal_cpuGetCount(void)
{
	return 1;
}


static inline unsigned int hal_cpuGetID(void)
{
	return 0;
}


extern void _hal_cpuInitCores(void);


extern char *hal_cpuInfo(char *info);


extern char *hal_cpuFeatures(char *features, unsigned int len);


static inline void hal_wdgReload(void)
{

}


extern void _hal_cpuInit(void);


#endif


#endif
