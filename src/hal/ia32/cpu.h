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

#ifndef _HAL_CPU_H_
#define _HAL_CPU_H_


#define SIZE_PAGE         0x1000


#define SIZE_KSTACK       (8 * 512)
#define SIZE_USTACK       (8 * SIZE_PAGE)


/* Bitfields used to construct interrupt descriptors */
#define IGBITS_DPL0       0x00000000
#define IGBITS_DPL3       0x00006000
#define IGBITS_PRES       0x00008000
#define IGBITS_SYSTEM     0x00000000
#define IGBITS_IRQEXC     0x00000e00
#define IGBITS_TRAP       0x00000f00
#define IGBITS_TSS        0x00000500


/* Bitfields used to construct segment descriptors */
#define DBITS_4KB         0x00800000    /* 4KB segment granularity */
#define DBITS_1B          0x00000000    /* 1B segment granularity */

#define DBITS_CODE32      0x00400000    /* 32-bit code segment */
#define DBITS_CODE16      0x00000000    /* 16-bit code segment */

#define DBITS_PRESENT     0x00008000    /* present segment */
#define DBITS_NOTPRESENT  0x00000000    /* segment not present in the physcial memory*/

#define DBITS_DPL0        0x00000000    /* kernel privilege level segment */
#define DBITS_DPL3        0x00006000    /* user privilege level segment */

#define DBITS_SYSTEM      0x00000000    /* segment used by system */
#define DBITS_APP         0x00001000    /* segment used by application */

#define DBITS_CODE        0x00000800    /* code segment descriptor */
#define DBITS_DATA        0x00000000    /* data segment descriptor */

#define DBITS_EXPDOWN     0x00000400    /* data segment is expandable down */
#define DBITS_WRT         0x00000200    /* writing to data segment is permitted */
#define DBITS_ACCESIBLE   0x00000100    /* data segment is accesible */

#define DBITS_CONFORM     0x00000400    /* conforming code segment */
#define DBITS_READ        0x00000200    /* read from code segment is permitted */


/*
 * Predefined descriptor types
 */


/* Descriptor of Task State Segment - used in CPU context switching */
#define DESCR_TSS    (DBITS_1B | DBITS_PRESENT | DBITS_DPL0 | DBITS_SYSTEM | 0x00000900)

/* Descriptor of user task code segment */
#define DESCR_UCODE  (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL3 | DBITS_APP | DBITS_CODE | DBITS_READ)

/* Descriptor of user task data segment */
#define DESCR_UDATA  (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL3 | DBITS_APP | DBITS_DATA | DBITS_WRT)


/* Descriptor of user task code segment */
#define DESCR_KCODE  (DBITS_4KB | DBITS_CODE32 | DBITS_PRESENT | DBITS_DPL0 | DBITS_APP | DBITS_CODE | DBITS_READ)

/* Descriptor of user task data segment */
#define DESCR_KDATA  (DBITS_4KB | DBITS_PRESENT | DBITS_DPL0 | DBITS_APP | DBITS_DATA | DBITS_WRT)


/* Segment selectors */
#define SEL_KCODE    8
#define SEL_KDATA    16
#define SEL_UCODE    27
#define SEL_UDATA    35


#define NULL 0


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


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u64 cycles_t;

typedef u64 usec_t;
typedef s64 offs_t;

typedef unsigned int size_t;
typedef unsigned long long time_t;

typedef u32 ptr_t;

/* Object identifier - contains server port and object id */
typedef u64 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;

#pragma pack(push, 1)

/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct {
	u32 savesp;
#ifndef NDEBUG
	u32 dr0;
	u32 dr1;
	u32 dr2;
	u32 dr3;
#endif
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


/* platform specific syscall */


extern int hal_platformctl(void *ptr);


/* io access */


static inline u8 hal_inb(void *addr)
{
	u8 b;

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inb %%dx, %%al; \
		movb %%al, %0;" \
	:"=b" (b) \
	:"g" (addr) \
	:"edx", "eax");
	return b;
}


static inline void hal_outb(void *addr, u8 b)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movb %1, %%al; \
		outb %%al, %%dx"
	:
	:"g" (addr), "b" (b)
	:"eax", "edx");

	return;
}


static inline u16 hal_inw(void *addr)
{
	u16 w;

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inw %%dx, %%ax; \
		movw %%ax, %0;" \
	:"=g" (w) \
	:"g" (addr) \
	:"edx", "eax");

	return w;
}


static inline void hal_outw(void *addr, u16 w)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movw %1, %%ax; \
		outw %%ax, %%dx"
		:
		:"g" (addr), "g" (w)
		:"eax", "edx");

	return;
}


static inline u32 hal_inl(void *addr)
{
	u32 l;

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inl %%dx, %%eax; \
		movl %%eax, %0;" \
		:"=g" (l) \
		:"g" (addr) \
		:"eax", "edx", "memory");

	return l;
}


static inline void hal_outl(void *addr, u32 l)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movl %1, %%eax; \
		outl %%eax, %%dx"
		:
		:"g" (addr), "g" (l)
		:"eax", "edx");

	return;
}


static inline void hal_wrmsr(u32 id, u64 v)
{
	__asm__ volatile ("wrmsr":: "c" (id), "A" (v));
}


static inline u64 hal_rdmsr(u32 id)
{
	u64 v;

	__asm__ volatile ("rdmsr" : "=A" (v) : "c" (id));
	return v;
}


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


static inline time_t hal_cpuLowPower(time_t ms)
{
	return 0;
}


static inline void hal_cpuHalt(void)
{
	__asm__ volatile ("hlt":);
}


static inline void hal_cpuGetCycles(void *cb)
{
	__asm__ volatile
	(" \
		rdtsc; \
		movl %0, %%edi; \
		movl %%eax, (%%edi); \
		movl %%edx, 4(%%edi)"
		:
		:"g" (cb)
		:"eax", "edx", "edi");
	return;
}


/* memory management */


static inline void hal_cpuFlushTLB(void *vaddr)
{
	unsigned long tmpreg;

	do {

		__asm__ volatile
		(" \
			movl %%cr3, %0; \
			movl %0, %%cr3"
			:"=r" (tmpreg)
			:
			:"memory");

	} while (0);

	return;
}


static inline void hal_cpuSwitchSpace(addr_t cr3)
{
	__asm__ volatile
	(" \
 		movl %0, %%eax; \
 		movl %%eax, %%cr3;"
	:
	:"g" (cr3)
	: "eax", "memory");

	return;
}


/* bit operations */


static inline unsigned int hal_cpuGetLastBit(u32 v)
{
	int lb;

	__asm__ volatile
	(" \
 		movl %1, %%eax; \
 		bsrl %%eax, %0; \
 		jnz 1f; \
 		xorl %0, %0; \
	1:"
	:"=r" (lb)
	:"g" (v)
	:"eax");

	return lb;
}


static inline unsigned int hal_cpuGetFirstBit(u32 v)
{
	int fb;

	__asm__ volatile
	(" \
 		mov %1, %%eax; \
 		bsfl %%eax, %0; \
 		jnz 1f; \
 		xorl %0, %0; \
	1:"
	:"=r" (fb)
	:"g" (v)
	:"eax");

	return fb;
}


static inline u32 hal_cpuSwapBits(const u32 v)
{
	u32 data = v;

	__asm__ volatile ("bswap %0": "=r" (data));

	return data;
}


/* debug */


static inline void hal_cpuSetBreakpoint(void *addr)
{
	__asm__ volatile
	(" \
 		movl $0x80, %%eax; \
 		movl %%eax, %%dr7; \
 		movl %0, %%eax; \
 		movl %%eax, %%dr3"
	:
	:"g" (addr)
	:"eax","memory");

	return;
}


static inline void hal_cpuGuard(cpu_context_t *ctx, void *addr)
{
#ifndef NDEBUG
	if (ctx != NULL)
		ctx->dr0 = (u32)addr + 0x10;

	else
		__asm__ volatile ("movl %0, %%dr0" : : "r" (addr + 0x10));
#endif
}


/* context management */


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


static inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ctx->cs & 3;
}


/* Function creates new cpu context on top of given thread kernel stack */
extern int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg);


struct _spinlock_t;


extern int hal_cpuReschedule(struct _spinlock_t *spinlock);


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next + sizeof(u32);
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
	ctx->eax = retval;
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


static inline int hal_cpuPushSignal(void *kstack, void (*handler)(void), int n)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	char *ustack = (char *)ctx->esp;

	PUTONSTACK(ustack, u32, ctx->eip);
	PUTONSTACK(ustack, int, n);

	ctx->eip = (u32)handler;
	ctx->esp = (u32)ustack;

	return 0;
}


__attribute__((noreturn,regparm(1)))
void hal_longjmp(cpu_context_t *ctx);


static inline void hal_jmp(void *f, void *kstack, void *stack, int argc)
{
	if (stack == NULL) {
		__asm__ volatile
		(" \
			movl %0, %%esp;\
			call *%1"
		:
		: "g" (kstack), "r" (f)
		: "memory");
	}
	else {
		__asm__ volatile
		(" \
			sti; \
			movl %0, %%esp;\
			pushl %4;\
			pushl %2;\
			pushfl;\
			pushl %3;\
			movw %%dx, %%ds;\
			movw %%dx, %%es;\
			movw %%dx, %%fs;\
			movw %%dx, %%gs;\
			pushl %1;\
			iret"
		:
		: "g" (kstack), "ri" (f), "ri" (stack), "ri" (SEL_UCODE), "d" (SEL_UDATA)
		: "memory");
	}
}


/* core management */


static inline void hal_cpuid(u32 leaf, u32 index, u32 *ra, u32 *rb, u32 *rc, u32 *rd)
{
	__asm__ volatile
	(" \
		cpuid"
	: "=a" (*ra), "=b" (*rb), "=c" (*rc), "=d" (*rd)
	: "a" (leaf), "c" (index)
	: "memory");
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
