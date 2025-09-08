/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2012-2013, 2016-2017, 2020 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "halsyspage.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/list.h"
#include "hal/cpu.h"
#include "hal/pmap.h"
#include "ia32.h"

#include "proc/userintr.h"
#include "include/errno.h"
#include "init.h"

#include "perf/trace-events.h"

#include <arch/tlb.h>


/* Hardware interrupt stubs */
extern void _interrupts_irq0(void);
extern void _interrupts_irq1(void);
extern void _interrupts_irq2(void);
extern void _interrupts_irq3(void);
extern void _interrupts_irq4(void);
extern void _interrupts_irq5(void);
extern void _interrupts_irq6(void);
extern void _interrupts_irq7(void);
extern void _interrupts_irq8(void);
extern void _interrupts_irq9(void);
extern void _interrupts_irq10(void);
extern void _interrupts_irq11(void);
extern void _interrupts_irq12(void);
extern void _interrupts_irq13(void);
extern void _interrupts_irq14(void);
extern void _interrupts_irq15(void);

extern void _interrupts_unexpected(void);

extern void _interrupts_syscall(void);


extern void _interrupts_TLBShootdown(void);


#define SIZE_INTERRUPTS 16


struct {
	struct {
		spinlock_t spinlock;
		intr_handler_t *handler;
		u32 counter;
	} interrupts[SIZE_INTERRUPTS];
	struct {
		void *ioapic;
		u8 flags;
		u8 vector;
	} irqs[SIZE_INTERRUPTS];
	enum { pic_undefined,
		pic_ioapic,
		pic_8259 } pic;
	u32 systickIRQ;
	spinlock_t sp_ioapic;
	int trace_irqs;
} interrupts_common;


unsigned int _interrupts_multilock;


static inline u32 _hal_ioapicRead(void *ioapic, u8 reg)
{
	*(u32 volatile *)ioapic = reg;
	return *(u32 volatile *)(ioapic + 0x10);
}


static inline void _hal_ioapicWrite(void *ioapic, u8 reg, u32 val)
{
	*(u32 volatile *)ioapic = reg;
	*(u32 volatile *)(ioapic + 0x10) = val;
}


static void _hal_ioapicWriteIRQ(void *ioapic, unsigned int n, u32 high, u32 low)
{
	low &= 0x0000ffffu;
	high &= 0xff000000u;
	_hal_ioapicWrite(ioapic, 0x10 + 2 * n, IOAPIC_IRQ_MASK);
	_hal_ioapicWrite(ioapic, 0x11 + 2 * n, high);
	_hal_ioapicWrite(ioapic, 0x10 + 2 * n, low);
}


static inline void _hal_ioapicReadIRQ(void *ioapic, unsigned int n, u32 *high, u32 *low)
{
	*high = _hal_ioapicRead(ioapic, 0x11 + 2 * n);
	*low = _hal_ioapicRead(ioapic, 0x10 + 2 * n);
}


static inline void _hal_ioapicRoundRobin(unsigned int n)
{
	u32 high, low;
	spinlock_ctx_t ctx;
	if (n < SIZE_INTERRUPTS) {
		if (interrupts_common.pic == pic_ioapic) {
			if (n == SYSTICK_IRQ) {
				n = interrupts_common.systickIRQ;
			}
			hal_spinlockSet(&interrupts_common.sp_ioapic, &ctx);
			_hal_ioapicReadIRQ(interrupts_common.irqs[n].ioapic, n, &high, &low);
			high = cpu.cpus[(hal_cpuGetID() + 1) % hal_cpuGetCount()];
			_hal_ioapicWriteIRQ(interrupts_common.irqs[n].ioapic, n, high << 24, low);
			hal_spinlockClear(&interrupts_common.sp_ioapic, &ctx);
		}
	}
}


static inline void _hal_interrupts_8259EOI(unsigned int n)
{
	if ((hal_isLapicPresent() != 0) && (n == TLB_IRQ)) {
		_hal_lapicWrite(LAPIC_EOI_REG, LAPIC_EOI);
		return;
	}
	/* Check for rare case, when we use 8259 PIC with multiple cores and APIC */
	if (hal_cpuGetID() != 0) {
		_hal_lapicWrite(LAPIC_EOI_REG, LAPIC_EOI);
		return;
	}
	if (n < 8) {
		hal_outb(PORT_PIC_MASTER_COMMAND, 0x60 | n);
	}
	else {
		hal_outb(PORT_PIC_MASTER_COMMAND, 0x62);
		hal_outb(PORT_PIC_SLAVE_COMMAND, 0x60 | (n - 8));
	}
}


static inline void _hal_interruptsApicEOI(unsigned int n)
{
	_hal_ioapicRoundRobin(n);
	_hal_lapicWrite(LAPIC_EOI_REG, LAPIC_EOI);
}


void _interrupts_eoi(unsigned int n)
{
	if ((n >= SIZE_INTERRUPTS) && ((n < SYSCALL_IRQ) || (n > TLB_IRQ))) {
		return;
	}

	switch (interrupts_common.pic) {
		case pic_8259:
			_hal_interrupts_8259EOI(n);
			break;
		case pic_ioapic:
			_hal_interruptsApicEOI(n);
			break;
		default:
			break;
	}

	return;
}


int interrupts_dispatchIRQ(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;
	int trace;

	if (n >= SIZE_INTERRUPTS) {
		return 0;
	}

	trace = interrupts_common.trace_irqs != 0 && n != SYSTICK_IRQ;
	if (trace != 0) {
		trace_eventInterruptEnter(n);
	}

	hal_spinlockSet(&interrupts_common.interrupts[n].spinlock, &sc);

	interrupts_common.interrupts[n].counter++;

	h = interrupts_common.interrupts[n].handler;
	if (h != NULL) {
		do {
			if ((h->f(n, ctx, h->data)) != 0) {
				reschedule = 1;
			}
			h = h->next;
		} while (h != interrupts_common.interrupts[n].handler);
	}

	hal_spinlockClear(&interrupts_common.interrupts[n].spinlock, &sc);

	if (trace != 0) {
		trace_eventInterruptExit(n);
	}

	return reschedule;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.interrupts[h->n].spinlock, &sc);
	HAL_LIST_ADD(&interrupts_common.interrupts[h->n].handler, h);
	hal_spinlockClear(&interrupts_common.interrupts[h->n].spinlock, &sc);

	return EOK;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.interrupts[h->n].spinlock, &sc);
	HAL_LIST_REMOVE(&interrupts_common.interrupts[h->n].handler, h);
	hal_spinlockClear(&interrupts_common.interrupts[h->n].spinlock, &sc);

	return EOK;
}


/* Function setups interrupt stub in IDT */
static int _interrupts_setIDTEntry(unsigned int n, void *addr, u32 type)
{
	u32 w0, w1;
	volatile u32 *idtr = (volatile u32 *)syspage->hs.idtr.addr;

	if (n > 255) {
		return -EINVAL;
	}

	w0 = ((u32)addr & 0xffff0000u);
	w1 = ((u32)addr & 0x0000ffffu);
	type &= 0xef00u;

	w0 |= type;
	w1 |= (SEL_KCODE << 16);

	idtr[n * 2 + 1] = w0;
	idtr[n * 2] = w1;

	return EOK;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	switch (interrupts_common.pic) {
		case pic_8259:
			hal_strncpy(features, "Using i8259 interrupt controller", len);
			break;
		case pic_ioapic:
			hal_strncpy(features, "Using I/O advanced programmable interrupt controller", len);
			break;
		default:
			hal_strncpy(features, "Using unknown interrupt controller", len);
			break;
	}
	features[len - 1] = 0;

	return features;
}


static void _hal_interrupts8259PICRemap(void)
{
	/* Initialize interrupt controllers (8259A) */
	hal_outb(PORT_PIC_MASTER_COMMAND, 0x11); /* ICW1 */
	hal_outb(PORT_PIC_MASTER_DATA, 0x20);    /* ICW2 (Master) */
	hal_outb(PORT_PIC_MASTER_DATA, 0x04);    /* ICW3 (Master) */
	hal_outb(PORT_PIC_MASTER_DATA, 0x01);    /* ICW4 */

	hal_outb(PORT_PIC_SLAVE_COMMAND, 0x11); /* ICW1 (Slave) */
	hal_outb(PORT_PIC_SLAVE_DATA, 0x28);    /* ICW2 (Slave) */
	hal_outb(PORT_PIC_SLAVE_DATA, 0x02);    /* ICW3 (Slave) */
	hal_outb(PORT_PIC_SLAVE_DATA, 0x01);    /* ICW4 (Slave) */
}


static void _hal_interrupts8259PICInit(void)
{
	interrupts_common.pic = pic_8259;
	interrupts_common.systickIRQ = SYSTICK_IRQ;
	cpu.ncpus = 1;
	cpu.cpus[0] = 0;
	_hal_interrupts8259PICRemap();
}

typedef struct {
	u8 type;
	u8 length;
} __attribute__ ((packed)) madt_entry_header_t;


static int _hal_ioapicInit(void)
{
	madt_entry_header_t *e;
	struct {
		madt_entry_header_t h;
		u8 ioApicID;
		u8 reserved;
		addr_t ioApicAddress;
		u32 globalSystemInterruptBase;
	} __attribute__((packed)) *ioapic;

	struct {
		madt_entry_header_t h;
		u8 bus;
		u8 source;
		u32 globalSystemInterrupt;
		u16 flags;
	} __attribute__((packed)) *sourceOverride;

	struct {
		madt_entry_header_t h;
		u8 acpiProcessorUID;
		u8 apicID;
		u32 flags;
	} __attribute__((packed)) *localApic;

	hal_madtHeader_t *madt = hal_config.madt;
	size_t i;
	u32 high, low, n;
	void *ptr;

	interrupts_common.systickIRQ = SYSTICK_IRQ;

	/* Parse ACPI MADT table: find all LAPICs */
	for (e = (void *)&madt->entries; (u32)e < (u32)madt + madt->header.length; e = (void *)e + e->length) {
		if (e->type == MADT_TYPE_PROCESSOR_LOCAL_APIC) {
			localApic = (void *)e;
			if ((localApic->flags & 3) != 0) {
				cpu.cpus[cpu.ncpus++] = localApic->apicID;
			}
		}
	}

	/* Parse ACPI MADT table: find all IOAPICs*/
	for (e = (void *)&madt->entries; (u32)e < (u32)madt + madt->header.length; e = (void *)e + e->length) {
		if (e->type == MADT_TYPE_IOAPIC) {
			ioapic = (void *)e;
			if (ioapic->globalSystemInterruptBase == 0) { /* We ignore every IOAPIC except the first one */
				ptr = _hal_configMapDevice((u32 *)(syspage->hs.pdir + VADDR_KERNEL), ioapic->ioApicAddress, SIZE_PAGE, PGHD_WRITE);
				/* Read how many entries does this IOAPIC handle */
				n = ((_hal_ioapicRead(ptr, IOAPIC_VERREG) >> 16) & 0xff) + 1;
				if (n > SIZE_INTERRUPTS) {
					n = SIZE_INTERRUPTS;
				}
				for (i = 0; i < n; ++i) {
					interrupts_common.irqs[i + ioapic->globalSystemInterruptBase].ioapic = ptr;
					high = cpu.cpus[0];
					high <<= 24;
					low = IOAPIC_IRQ_MASK | (i + INTERRUPTS_VECTOR_OFFSET);
					interrupts_common.irqs[i + ioapic->globalSystemInterruptBase].flags = low;
					_hal_ioapicWriteIRQ(ptr, i, high, low);
				}
			}
		}
	}

	/* Check if every IRQ is covered by IOAPIC */
	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		if (interrupts_common.irqs[i].ioapic == NULL) {
			return 1;
		}
	}

	hal_spinlockCreate(&interrupts_common.sp_ioapic, "interrupts_common.ioapic.spinlock");
	interrupts_common.pic = pic_ioapic;

	if ((madt->flags & MADT_8259PIC_INSTALLED) != 0) {
		/* Remap 8259 PIC's interrupts, before disabling it */
		_hal_interrupts8259PICRemap();
		/* Disable 8259 PIC (by masking all interrupts) */
		hal_outb(PORT_PIC_MASTER_DATA, 0xff);
		hal_outb(PORT_PIC_SLAVE_DATA, 0xff);
	}

	/* Parse ACPI MADT table: find all interrupt source overrides */
	for (e = (void *)&madt->entries; (u32)e < (u32)madt + madt->header.length; e = (void *)e + e->length) {
		if (e->type == MADT_TYPE_IOAPIC_INTERRUPT_SOURCE_OVERRIDE) {
			sourceOverride = (void *)e;
			n = sourceOverride->globalSystemInterrupt;
			if (n < SIZE_INTERRUPTS) {
				if (sourceOverride->source == interrupts_common.systickIRQ) {
					interrupts_common.systickIRQ = n;
				}
				interrupts_common.irqs[n].vector = INTERRUPTS_VECTOR_OFFSET + sourceOverride->source;
				high = cpu.cpus[0];
				high <<= 24;
				low = IOAPIC_IRQ_MASK | (INTERRUPTS_VECTOR_OFFSET + sourceOverride->source);
				if ((sourceOverride->flags & MADT_ISO_POLAR_MASK) == MADT_ISO_POLAR_LOW) {
					low |= IOAPIC_INTPOL;
				}
				else {
					low &= ~IOAPIC_INTPOL;
				}
				if ((sourceOverride->flags & MADT_ISO_TRIGGER_MASK) == MADT_ISO_TRIGGER_LEVEL) {
					low |= IOAPIC_TRIGGER;
				}
				else {
					low &= ~IOAPIC_TRIGGER;
				}
				interrupts_common.irqs[n].flags = low;
				_hal_ioapicWriteIRQ(interrupts_common.irqs[n].ioapic, n, high, low);
			}
		}
	}

	/* Enable all IRQS */
	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		_hal_ioapicReadIRQ(interrupts_common.irqs[i].ioapic, i, &high, &low);
		_hal_ioapicWriteIRQ(interrupts_common.irqs[i].ioapic, i, high, low & ~IOAPIC_IRQ_MASK);
	}

	return 0;
}


void _hal_interruptsTrace(int enable)
{
	interrupts_common.trace_irqs = !!enable;
}


__attribute__((noreturn)) void hal_endSyscall(cpu_context_t *ctx)
{
	asm volatile(
			"movl %0, %%esp\n\t"
			"jmp interrupts_popContextUnlocked\n\t"
			:
			: "r"(ctx)
			: "memory");

	__builtin_unreachable();
}


void _hal_interruptsInit(void)
{
	static const u32 flags = IGBITS_PRES | IGBITS_SYSTEM | IGBITS_IRQEXC;
	unsigned int k;

	interrupts_common.trace_irqs = 0;

	_interrupts_multilock = 1;
	interrupts_common.pic = pic_undefined;

	for (k = 0; k < SIZE_INTERRUPTS; ++k) {
		interrupts_common.irqs[k].ioapic = NULL;
		interrupts_common.irqs[k].vector = INTERRUPTS_VECTOR_OFFSET + k;
	}

	cpu.ncpus = 0;

	if ((hal_config.acpi != ACPI_NONE) && (hal_config.madt != NULL)) {
		if (_hal_ioapicInit() != 0) {
			_hal_interrupts8259PICInit();
		}
	}
	else {
		_hal_interrupts8259PICInit();
	}

	/* Set stubs for hardware interrupts */
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 0, _interrupts_irq0, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 1, _interrupts_irq1, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 2, _interrupts_irq2, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 3, _interrupts_irq3, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 4, _interrupts_irq4, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 5, _interrupts_irq5, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 6, _interrupts_irq6, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 7, _interrupts_irq7, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 8, _interrupts_irq8, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 9, _interrupts_irq9, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 10, _interrupts_irq10, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 11, _interrupts_irq11, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 12, _interrupts_irq12, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 13, _interrupts_irq13, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 14, _interrupts_irq14, flags);
	_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + 15, _interrupts_irq15, flags);

	for (k = 0; k < SIZE_INTERRUPTS; k++) {
		hal_spinlockCreate(&interrupts_common.interrupts[k].spinlock, "interrupts_common.interrupts[].spinlock");
		interrupts_common.interrupts[k].handler = NULL;
		interrupts_common.interrupts[k].counter = 0;
	}

	/* Set stubs for unhandled interrupts */
	for (; k < 256; k++) {
		_interrupts_setIDTEntry(INTERRUPTS_VECTOR_OFFSET + k, _interrupts_unexpected, flags);
	}

	/* Set stub for syscall */
	_interrupts_setIDTEntry(SYSCALL_IRQ, _interrupts_syscall, flags | IGBITS_DPL3);
	_interrupts_setIDTEntry(TLB_IRQ, _interrupts_TLBShootdown, flags);

	return;
}
