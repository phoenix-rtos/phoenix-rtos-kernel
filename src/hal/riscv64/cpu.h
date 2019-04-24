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


#define SIZE_PAGE 0x1000


/* Size of thread kernel stack */
#define SIZE_KSTACK (8 * 512)
#define SIZE_USTACK (8 * SIZE_PAGE)


#define NULL 0


#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 1000


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

typedef u64 ptr_t;

/* Object identifier - contains server port and object id */
typedef u64 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;


#pragma pack(push, 1)

/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct {
	u64 pc;
	u64 gp;    /* x3 */

	u64 t0;    /* x5 */
	u64 t1;    /* x6 */
	u64 t2;    /* x7 */

	u64 s0;    /* x8 */
	u64 s1;    /* x9 */
	u64 a0;    /* x10 */
	u64 a1;    /* x11 */

	u64 a2;    /* x12 */
	u64 a3;    /* x13 */
	u64 a4;    /* x14 */
	u64 a5;    /* x15 */

	u64 a6;    /* x16 */
	u64 a7;    /* x17 */
	u64 s2;    /* x18 */
	u64 s3;    /* x19 */

	u64 s4;    /* x20 */
	u64 s5;    /* x21 */
	u64 s6;    /* x22 */
	u64 s7;    /* x23 */

	u64 s8;    /* x24 */
	u64 s9;    /* x25 */
	u64 s10;   /* x26 */
	u64 s11;   /* x27 */

	u64 t3;    /* x28 */
	u64 t4;    /* x29 */
	u64 t5;    /* x30 */
	u64 t6;    /* x31 */

	u64 ksp;
	u64 sstatus;
	u64 sepc;
	u64 sbadaddr;
	u64 scause;
	u64 sscratch;

	u64 tp;
	u64 sp;

} cpu_context_t;

#pragma pack(pop)


/* CSR routines */


#define SR_SIE	0x00000002UL /* Supervisor Interrupt Enable */
#define SR_SPIE	0x00000020UL /* Previous Supervisor IE */
#define SR_SPP	0x00000100UL /* Previously Supervisor */
#define SR_SUM	0x00040000UL /* Supervisor may access User Memory */

#define SIE_STIE 0x00000020UL


#define csr_set(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrs " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
	__v;							\
})


#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " #csr ", %0"		\
			      : : "rK" (__v)			\
			      : "memory");			\
})


#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " #csr			\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})


/* platform specific syscall */


extern int hal_platformctl(void *ptr);


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ ("csrc sstatus, 2");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ ("csrs sstatus, 2");
}


/* performance */


static inline time_t hal_cpuLowPower(time_t ms)
{
	return 0;
}


static inline void hal_cpuHalt(void)
{
	__asm__ ("wfi");
}


static inline void hal_cpuGetCycles(void *cb)
{
	return;
}


static inline cycles_t hal_cpuGetCycles2(void)
{
	register cycles_t n;

	__asm__ __volatile__ (
		"rdtime %0"
		: "=r" (n));
	return n;
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


static inline void hal_cpuSwitchSpace(addr_t pdir)
{
	__asm__ ("sfence.vma; csrw sptbr, %0"::"r" (pdir));

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


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
}


/* Function creates new cpu context on top of given thread kernel stack */
extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


struct _spinlock_t;


extern int hal_cpuReschedule(struct _spinlock_t *lock);


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->ksp = (u64)next;
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
	return (void *)ctx->sp;
}


static inline void hal_cpuGuard(cpu_context_t *ctx, void *addr)
{
}


extern void hal_longjmp(cpu_context_t *ctx);


static inline void hal_jmp(void *f, void *kstack, void *stack, int argc)
{
}


/* core management */


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
