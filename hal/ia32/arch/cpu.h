/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IA32_CPU_H_
#define _HAL_IA32_CPU_H_

#include "types.h"

#define SIZE_PAGE 0x1000u


/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8 * 512)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8 * SIZE_PAGE)
#endif


/* Bitfields used to construct interrupt descriptors */
#define IGBITS_DPL0   0x00000000
#define IGBITS_DPL3   0x00006000
#define IGBITS_PRES   0x00008000
#define IGBITS_SYSTEM 0x00000000
#define IGBITS_IRQEXC 0x00000e00
#define IGBITS_TRAP   0x00000f00
#define IGBITS_TSS    0x00000500


/* Bitfields used to construct segment descriptors */
#define DBITS_4KB 0x00800000 /* 4KB segment granularity */
#define DBITS_1B  0x00000000 /* 1B segment granularity */

#define DBITS_CODE32 0x00400000 /* 32-bit code segment */
#define DBITS_CODE16 0x00000000 /* 16-bit code segment */

#define DBITS_PRESENT    0x00008000 /* present segment */
#define DBITS_NOTPRESENT 0x00000000 /* segment not present in the physical memory*/

#define DBITS_DPL0 0x00000000 /* kernel privilege level segment */
#define DBITS_DPL3 0x00006000 /* user privilege level segment */

#define DBITS_SYSTEM 0x00000000 /* segment used by system */
#define DBITS_APP    0x00001000 /* segment used by application */

#define DBITS_CODE 0x00000800 /* code segment descriptor */
#define DBITS_DATA 0x00000000 /* data segment descriptor */

#define DBITS_EXPDOWN   0x00000400 /* data segment is expandable down */
#define DBITS_WRT       0x00000200 /* writing to data segment is permitted */
#define DBITS_ACCESIBLE 0x00000100 /* data segment is accessible */

#define DBITS_CONFORM 0x00000400 /* conforming code segment */
#define DBITS_READ    0x00000200 /* read from code segment is permitted */


/*
 * Predefined descriptor types
 */


/* Descriptor of Task State Segment - used in CPU context switching */
#define DESCR_TSS (DBITS_1B | DBITS_PRESENT | DBITS_DPL0 | DBITS_SYSTEM | 0x00000900)

/* Descriptor of user task code segment */
#define DESCR_UCODE (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL3 | DBITS_APP | DBITS_CODE | DBITS_READ)

/* Descriptor of user task data segment */
#define DESCR_UDATA (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL3 | DBITS_APP | DBITS_DATA | DBITS_WRT)


/* Descriptor of user task code segment */
#define DESCR_KCODE (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL0 | DBITS_APP | DBITS_CODE | DBITS_READ)

/* Descriptor of user task data segment */
#define DESCR_KDATA (DBITS_4KB | DBITS_PRESENT | DBITS_DPL0 | DBITS_APP | DBITS_DATA | DBITS_WRT)

/* Descriptor of Thread Local Storage segment */
#define DESCR_TLS (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL3 | DBITS_APP | DBITS_DATA | DBITS_WRT)


/* Segment selectors */
#define SEL_KCODE 8
#define SEL_KDATA 16
#define SEL_UCODE 27
#define SEL_UDATA 35

/* The first index in GDT that can be used for TSS and TLS entries */
#define GDT_FREE_SEL_IDX 5

#define CR0_TS_BIT       8u
#define FPU_CONTEXT_SIZE 108u /* sizeof(fpu_context_t) */

/* IO Ports */
/* Ports of (8259A) PIC (Programmable Interrupt Controller) */
#define PORT_PIC_MASTER_COMMAND ((void *)0x20)
#define PORT_PIC_MASTER_DATA    ((void *)0x21)
#define PORT_PIC_SLAVE_COMMAND  ((void *)0xa0)
#define PORT_PIC_SLAVE_DATA     ((void *)0xa1)
/* Ports of PIT (Programmable Interval Timer) */
#define PORT_PIT_DATA_CHANNEL0 ((void *)0x40)
#define PORT_PIT_COMMAND       ((void *)0x43)
/* Ports of 8042 PS/2 Controller */
#define PORT_PS2_DATA    ((void *)0x60)
#define PORT_PS2_COMMAND ((void *)0x64)

/* There are objects in memory that require O(MAX_CPU_COUNT^2) memory. */
#define MAX_CPU_COUNT 64

#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 10000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= ((sizeof(t) + 3) & ~3);	\
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		if (n == 0) \
			ustack += 4; \
		v = *(t *)ustack; \
		ustack += ((sizeof(t) + 3) & ~3); \
	} while (0)



#pragma pack(push, 1)

/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct {
	u32 savesp;
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
	fpu_context_t fpuContext;
	u32 cr0Bits;
	u32 eip; /* eip, cs, eflags, esp, ss saved by CPU on interrupt */
	u32 cs;
	u32 eflags;
	u32 esp;
	u32 ss;
} cpu_context_t;


/* IA32 TSS */
typedef struct {
	u16 backlink, _backlink;
	u32 esp0;
	u16 ss0, _ss0;
	u32 esp1;
	u16 ss1, _ss1;
	u32 esp2;
	u16 ss2, _ss2;
	u32 cr3;
	u32 eip;
	u32 eflags;
	u32 eax;
	u32 ecx;
	u32 edx;
	u32 ebx;
	u32 esp;
	u32 ebp;
	u32 esi;
	u32 edi;
	u16 es, _es;
	u16 cs, _cs;
	u16 ss, _ss;
	u16 ds, _ds;
	u16 fs, _fs;
	u16 gs, _gs;
	u16 ldt, _ldt;
	u16 trfl;
	u16 iomap;
} tss_t;

#pragma pack(pop)


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile ("cli":);
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile ("sti":);
}


/* performance */


static inline void hal_cpuHalt(void)
{
	__asm__ volatile ("hlt":);
}


static inline void hal_cpuSetDevBusy(int s)
{
}


static inline void hal_cpuGetCycles(cycles_t *cb)
{
	/* clang-format off */
	__asm__ volatile ("rdtsc" : "=A" (*cb));
	/* clang-format on */
}


/* bit operations */


static inline unsigned int hal_cpuGetLastBit(unsigned long v)
{
	int lb;

	/* clang-format off */
	__asm__ volatile (
		"bsrl %1, %0\n\t"
		"jnz 1f\n\t"
		"xorl %0, %0\n"
		"1:"
	: "=r" (lb)
	: "rm" (v)
	: );
	/* clang-format on */

	return lb;
}


static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	int fb;

	/* clang-format off */
	__asm__ volatile (
		"bsfl %1, %0\n\t"
		"jnz 1f\n\t"
		"xorl %0, %0\n"
		"1:"
	: "=r" (fb)
	: "rm" (v)
	: );
	/* clang-format on */

	return fb;
}


/* context management */

unsigned int hal_cpuGetTlsIndex(void);

static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
}


static inline void hal_cpuSetGot(void *got)
{
}


static inline void *hal_cpuGetGot(void)
{
	return NULL;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next + sizeof(u32);
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
	ctx->eax = retval;
}


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->esp;
}


static inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ((ctx->cs & 3) == 0);
}

/* Atomic operations */

static inline u32 hal_cpuAtomAdd(volatile u32 *dest, u32 val)
{
	/* clang-format off */
	__asm__ volatile (
		"lock xaddl %[val], %[dest]\n\t"
	: [val] "+r" (val), [dest] "+m" (*dest)
	:
	: "memory");
	/* clang-format on */
	return val;
}


#endif

#endif
