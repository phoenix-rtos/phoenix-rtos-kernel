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

#ifndef _PH_HAL_IA32_CPU_H_
#define _PH_HAL_IA32_CPU_H_

#include "hal/types.h"

#define SIZE_PAGE 0x1000U


/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (2U * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif


/* Bitfields used to construct interrupt descriptors */
#define IGBITS_DPL0   0x00000000U
#define IGBITS_DPL3   0x00006000U
#define IGBITS_PRES   0x00008000U
#define IGBITS_SYSTEM 0x00000000U
#define IGBITS_IRQEXC 0x00000e00U
#define IGBITS_TRAP   0x00000f00U
#define IGBITS_TSS    0x00000500U


/* Bitfields used to construct segment descriptors */
#define DBITS_4KB 0x00800000U /* 4KB segment granularity */
#define DBITS_1B  0x00000000U /* 1B segment granularity */

#define DBITS_CODE32 0x00400000U /* 32-bit code segment */
#define DBITS_CODE16 0x00000000U /* 16-bit code segment */

#define DBITS_PRESENT    0x00008000U /* present segment */
#define DBITS_NOTPRESENT 0x00000000U /* segment not present in the physical memory*/

#define DBITS_DPL0 0x00000000U /* kernel privilege level segment */
#define DBITS_DPL3 0x00006000U /* user privilege level segment */

#define DBITS_SYSTEM 0x00000000U /* segment used by system */
#define DBITS_APP    0x00001000U /* segment used by application */

#define DBITS_CODE 0x00000800U /* code segment descriptor */
#define DBITS_DATA 0x00000000U /* data segment descriptor */

#define DBITS_EXPDOWN   0x00000400U /* data segment is expandable down */
#define DBITS_WRT       0x00000200U /* writing to data segment is permitted */
#define DBITS_ACCESIBLE 0x00000100U /* data segment is accessible */

#define DBITS_CONFORM 0x00000400U /* conforming code segment */
#define DBITS_READ    0x00000200U /* read from code segment is permitted */


/*
 * Predefined descriptor types
 */


/* Descriptor of Task State Segment - used in CPU context switching */
#define DESCR_TSS (DBITS_1B | DBITS_PRESENT | DBITS_DPL0 | DBITS_SYSTEM | 0x00000900U)

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
#define GDT_FREE_SEL_IDX 5U

#define CR0_TS_BIT       8U
#define FPU_CONTEXT_SIZE 108U /* sizeof(fpu_context_t) */

/* IO Ports */
/* Ports of (8259A) PIC (Programmable Interrupt Controller) */
#define PORT_PIC_MASTER_COMMAND ((u16)0x20)
#define PORT_PIC_MASTER_DATA    ((u16)0x21)
#define PORT_PIC_SLAVE_COMMAND  ((u16)0xa0)
#define PORT_PIC_SLAVE_DATA     ((u16)0xa1)
/* Ports of PIT (Programmable Interval Timer) */
#define PORT_PIT_DATA_CHANNEL0 ((u16)0x40)
#define PORT_PIT_COMMAND       ((u16)0x43)
/* Ports of 8042 PS/2 Controller */
#define PORT_PS2_DATA    ((u16)0x60)
#define PORT_PS2_COMMAND ((u16)0x64)

/* There are objects in memory that require O(MAX_CPU_COUNT^2) memory. */
#define MAX_CPU_COUNT 64

#define LAPIC_DEFAULT_ADDRESS 0xfee00000U

/* Local APIC offsets */
#define LAPIC_ID_REG          0x20U
#define LAPIC_VERSION_REG     0x30U
#define LAPIC_TASK_PRIO_REG   0x80U
#define LAPIC_ARBI_PRIO_REG   0x90U
#define LAPIC_PROC_PRIO_REG   0xa0U
#define LAPIC_EOI_REG         0xb0U
#define LAPIC_REMO_READ_REG   0xc0U
#define LAPIC_LOGI_DEST_REG   0xd0U
#define LAPIC_DEST_FORM_REG   0xe0U
#define LAPIC_SPUR_IRQ_REG    0xf0U
#define LAPIC_ISR_REG_0_31    0x100U
#define LAPIC_ISR_REG_32_63   0x110U
#define LAPIC_ISR_REG_64_95   0x120U
#define LAPIC_ISR_REG_96_127  0x130U
#define LAPIC_ISR_REG_128_159 0x140U
#define LAPIC_ISR_REG_160_191 0x150U
#define LAPIC_ISR_REG_192_223 0x160U
#define LAPIC_ISR_REG_224_255 0x170U
#define LAPIC_TMR_REG_0_31    0x180U
#define LAPIC_TMR_REG_32_63   0x190U
#define LAPIC_TMR_REG_64_95   0x1a0U
#define LAPIC_TMR_REG_96_127  0x1b0U
#define LAPIC_TMR_REG_128_159 0x1c0U
#define LAPIC_TMR_REG_160_191 0x1d0U
#define LAPIC_TMR_REG_192_223 0x1e0U
#define LAPIC_TMR_REG_224_255 0x1f0U
#define LAPIC_IRR_REG_0_31    0x200U
#define LAPIC_IRR_REG_32_63   0x210U
#define LAPIC_IRR_REG_64_95   0x220U
#define LAPIC_IRR_REG_96_127  0x230U
#define LAPIC_IRR_REG_128_159 0x240U
#define LAPIC_IRR_REG_160_191 0x250U
#define LAPIC_IRR_REG_192_223 0x260U
#define LAPIC_IRR_REG_224_255 0x270U
#define LAPIC_ERR_STAT_REG    0x280U
#define LAPIC_LVT_CMCI_REG    0x2f0U
#define LAPIC_ICR_REG_0_31    0x300U
#define LAPIC_ICR_REG_32_63   0x310U
#define LAPIC_LVT_TIMER_REG   0x320U
#define LAPIC_LVT_THERMO_REG  0x330U
#define LAPIC_LVT_PMC_REG     0x340U
#define LAPIC_LVT_LINT0_REG   0x350U
#define LAPIC_LVT_LINT1_REG   0x360U
#define LAPIC_LVT_ERR_REG     0x370U
#define LAPIC_LVT_TMR_IC_REG  0x380U
#define LAPIC_LVT_TMR_CC_REG  0x390U
#define LAPIC_LVT_TMR_DC_REG  0x3e0U


#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 10000


#define SIZE_STACK_ARG(sz) (((sz) + 3U) & ~0x3U)


/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "t used as type -  wrong interpretation" */
#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		if ((n) == 0U) { \
			(ustack) += 4; \
		} \
		(v) = *(t *)(ustack); \
		(ustack) += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)
/* parasoft-end-suppress MISRAC2012-RULE_20_7*/


#pragma pack(push, 1)

typedef struct {
	u16 controlWord, _controlWord;
	u16 statusWord, _statusWord;
	u16 tagWord, _tagWord;
	u32 fip;
	u32 fips;
	u32 fdp;
	u16 fds, _fds;
	ld80 fpuContext[8];
} fpu_context_t;

/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct _cpu_context_t {
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

typedef struct {
	tss_t tss[MAX_CPU_COUNT];
	char stacks[MAX_CPU_COUNT][SIZE_KSTACK];
	u32 dr5;
	unsigned int ncpus;
	volatile unsigned int readyCount;
	unsigned int cpus[MAX_CPU_COUNT];
} hal_cpu_t;

extern hal_cpu_t hal_cpu;
void hal_cpuSendIPI(unsigned int cpu, unsigned int intrAndFlags);


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile("cli");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile("sti");
}


/* performance */


static inline void hal_cpuHalt(void)
{
	__asm__ volatile("hlt");
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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline unsigned int hal_cpuGetLastBit(unsigned long v)
{
	unsigned int lb;

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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned int fb;

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


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next + sizeof(u32);
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->eax = (u32)retval;
}


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->esp;
}

/* Atomic operations */

/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
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


static inline void hal_cpuid(u32 leaf, u32 index, u32 *ra, u32 *rb, u32 *rc, u32 *rd)
{
	/* clang-format off */
	__asm__ volatile (
		"cpuid"
	: "=a" (*ra), "=b" (*rb), "=c" (*rc), "=d" (*rd)
	: "a" (leaf), "c" (index)
	: "memory");
	/* clang-format on */
}


static inline void hal_cpuReloadTlsSegment(void)
{
	/* clang-format off */
	__asm__ volatile (
		"pushw %%gs\n\t"
		"popw %%gs"
	:::);
	/* clang-format on */
}


#endif

#endif
