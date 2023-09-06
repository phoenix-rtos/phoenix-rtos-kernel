/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2023 Phoenix Systems
 * Author; Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "tlb.h"
#include "../string.h"
#include "../cpu.h"
#include "../interrupts.h"

/* Maximum number of TLB operations */
#define MAX_CPU_TASK_COUNT 2

extern void cpu_broadcastIPI(unsigned int intr);

struct task_tlb {
	void (*func)(void *);
	const void *entry;
	volatile size_t confirmations;
	spinlock_t *spinlock;
};

struct cpu_tlb {
	struct task_tlb *todo[MAX_CPU_TASK_COUNT * MAX_CPU_COUNT];
	volatile size_t todo_size;
	struct task_tlb tasks[MAX_CPU_TASK_COUNT];
	volatile size_t tasks_size;
	spinlock_t todo_spinlock;
	spinlock_t task_spinlock;
	spinlock_t core_spinlock;
};


static struct {
	struct cpu_tlb tlbs[MAX_CPU_COUNT];
} tlb_common;


static void tlb_flush(void *arg)
{
	struct task_tlb *task = arg;
	spinlock_ctx_t sc;
	hal_tlbFlushLocal();
	hal_spinlockSet(task->spinlock, &sc);
	--task->confirmations;
	hal_spinlockClear(task->spinlock, &sc);
}


static void tlb_invalidate(void *arg)
{
	struct task_tlb *task = arg;
	spinlock_ctx_t sc;
	hal_tlbInvalidateLocalEntry(task->entry);
	hal_spinlockSet(task->spinlock, &sc);
	--task->confirmations;
	hal_spinlockClear(task->spinlock, &sc);
}


/* Must be protected by pmap_common.lock */
void hal_tlbFlush(void)
{
	unsigned int i;
	const unsigned int n = hal_cpuGetCount(), id = hal_cpuGetID();
	size_t tasks_size;
	spinlock_ctx_t sc;

	hal_spinlockSet(&tlb_common.tlbs[id].task_spinlock, &sc);
	tasks_size = tlb_common.tlbs[id].tasks_size;

	tlb_common.tlbs[id].tasks[tasks_size].func = tlb_flush;
	tlb_common.tlbs[id].tasks[tasks_size].entry = NULL;
	tlb_common.tlbs[id].tasks[tasks_size].confirmations = n - 1;
	tlb_common.tlbs[id].tasks[tasks_size].spinlock = &tlb_common.tlbs[id].task_spinlock;

	++tlb_common.tlbs[id].tasks_size;
	hal_spinlockClear(&tlb_common.tlbs[id].task_spinlock, &sc);

	for (i = 0; i < n; ++i) {
		if (i != id) {
			hal_spinlockSet(&tlb_common.tlbs[i].todo_spinlock, &sc);
			tlb_common.tlbs[i].todo[tlb_common.tlbs[i].todo_size++] = &tlb_common.tlbs[id].tasks[tasks_size];
			hal_spinlockClear(&tlb_common.tlbs[i].todo_spinlock, &sc);
		}
	}
	hal_tlbFlushLocal();
}


/* Must be protected by pmap_common.lock */
void hal_tlbInvalidateEntry(const void *vaddr)
{
	unsigned int i;
	const unsigned int n = hal_cpuGetCount(), id = hal_cpuGetID();
	size_t tasks_size;
	spinlock_ctx_t sc;

	hal_spinlockSet(&tlb_common.tlbs[id].task_spinlock, &sc);
	tasks_size = tlb_common.tlbs[id].tasks_size;

	tlb_common.tlbs[id].tasks[tasks_size].func = tlb_invalidate;
	tlb_common.tlbs[id].tasks[tasks_size].entry = vaddr;
	tlb_common.tlbs[id].tasks[tasks_size].confirmations = n - 1;
	tlb_common.tlbs[id].tasks[tasks_size].spinlock = &tlb_common.tlbs[id].task_spinlock;

	++tlb_common.tlbs[id].tasks_size;
	hal_spinlockClear(&tlb_common.tlbs[id].task_spinlock, &sc);

	for (i = 0; i < n; ++i) {
		if (i != id) {
			hal_spinlockSet(&tlb_common.tlbs[i].todo_spinlock, &sc);
			tlb_common.tlbs[i].todo[tlb_common.tlbs[i].todo_size++] = &tlb_common.tlbs[id].tasks[tasks_size];
			hal_spinlockClear(&tlb_common.tlbs[i].todo_spinlock, &sc);
		}
	}
	hal_tlbInvalidateLocalEntry(vaddr);
}


/* Must be protected by pmap_common.lock */
void hal_tlbCommit(spinlock_t *spinlock, spinlock_ctx_t *ctx)
{
	spinlock_ctx_t sc;
	const unsigned int id = hal_cpuGetID();
	size_t i, confirmations;
	cpu_broadcastIPI(TLB_IRQ);
	hal_spinlockSet(&tlb_common.tlbs[id].core_spinlock, &sc);
	hal_spinlockClear(spinlock, &sc);

	do {
		hal_spinlockSet(&tlb_common.tlbs[id].task_spinlock, &sc);
		confirmations = 0;
		for (i = 0; i < tlb_common.tlbs[id].tasks_size; ++i) {
			confirmations += tlb_common.tlbs[id].tasks[i].confirmations;
		}
		if (confirmations == 0) {
			tlb_common.tlbs[id].tasks_size = 0;
		}
		hal_spinlockClear(&tlb_common.tlbs[id].task_spinlock, &sc);

		hal_tlbShootdown();
	} while (confirmations > 0);
	hal_spinlockClear(&tlb_common.tlbs[id].core_spinlock, ctx);
}


void hal_tlbShootdown(void)
{
	spinlock_ctx_t sc;
	size_t i;
	const unsigned int id = hal_cpuGetID();
	hal_spinlockSet(&tlb_common.tlbs[id].todo_spinlock, &sc);
	for (i = 0; i < tlb_common.tlbs[id].todo_size; ++i) {
		tlb_common.tlbs[id].todo[i]->func(tlb_common.tlbs[id].todo[i]);
	}
	tlb_common.tlbs[id].todo_size = 0;
	hal_spinlockClear(&tlb_common.tlbs[id].todo_spinlock, &sc);
}


void hal_tlbInitCore(const unsigned int id)
{
	hal_spinlockCreate(&tlb_common.tlbs[id].todo_spinlock, "tlb_common.tlbs.todo_spinlock");
	hal_spinlockCreate(&tlb_common.tlbs[id].task_spinlock, "tlb_common.tlbs.task_spinlock");
	hal_spinlockCreate(&tlb_common.tlbs[id].core_spinlock, "tlb_common.tlbs.core_spinlock");
	tlb_common.tlbs[id].tasks_size = 0;
	tlb_common.tlbs[id].todo_size = 0;
	hal_tlbFlushLocal();
}
