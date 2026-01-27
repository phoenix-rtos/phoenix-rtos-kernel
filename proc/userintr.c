/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Userspace interrupts handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "lib/lib.h"
#include "resource.h"
#include "userintr.h"
#include "cond.h"
#include "proc.h"


static struct {
	userintr_t *volatile active;
} userintr_common;


void userintr_put(userintr_t *ui)
{
	thread_t *t = proc_current();
	int rem;

	LIB_ASSERT(ui != NULL, "process: %s, pid: %d, tid: %d, ui == NULL",
			t->process->path, process_getPid(t->process), proc_getTid(t));

	rem = resource_put(t->process, &ui->resource);
	LIB_ASSERT(rem >= 0, "process: %s, pid: %d, tid: %d, refcnt below zero",
			t->process->path, process_getPid(t->process), proc_getTid(t));
	if (rem == 0) {
		(void)hal_interruptsDeleteHandler(&ui->handler);

		if (ui->cond != NULL) {
			cond_put(ui->cond);
		}

		vm_kfree(ui);
	}
}


static int userintr_dispatch(unsigned int n, cpu_context_t *ctx, void *arg)
{
	userintr_t *ui = arg;
	int ret, reschedule = 0;
	process_t *p = NULL;

	if (proc_current() != NULL) {
		p = (proc_current())->process;
	}

	/* Switch into the handler address space */
	pmap_switch(ui->process->pmapp);

	userintr_common.active = ui;

#ifdef __TARGET_RISCV64
	u64 gpKernel;
	/* Set GP register to the one saved when setting up the handler */
	gpKernel = hal_cpuGetGP();
	hal_cpuSetGP(ui->handler.gp);

	/* WARN: at this point, if GP relaxations are ever enabled for kernel,
	 * no global data accesses may be done until GP is restored.
	 *
	 * If GP relaxations are enabled, the GP register modifications and the handler call
	 * must be done in an assembly trampoline, and wrapped with `.option norelax` directive.
	 */
#endif

	ret = ui->f(ui->handler.n, ui->arg);

#ifdef __TARGET_RISCV64
	/* Restore previous GP register */
	hal_cpuSetGP(gpKernel);
#endif

	userintr_common.active = NULL;

	if (ret >= 0 && ui->cond != NULL) {
		reschedule = 1;
		(void)proc_threadBroadcast(&ui->cond->queue);
	}

	/* Restore process address space */
	if ((p != NULL) && (p->pmapp != NULL)) {
		pmap_switch(p->pmapp);
	}

	return reschedule;
}


int userintr_setHandler(unsigned int n, userintrFn_t f, void *arg, handle_t c)
{
	process_t *process = proc_current()->process;
	userintr_t *ui;
	cond_t *cond = NULL;
	int id, res;

	if (c > 0) {
		cond = cond_get(c);
		if (cond == NULL) {
			return -EINVAL;
		}
	}

	ui = vm_kmalloc(sizeof(*ui));
	if (ui == NULL) {
		if (cond != NULL) {
			cond_put(cond);
		}
		return -ENOMEM;
	}

	ui->resource.payload.userintr = ui;
	ui->resource.type = rtInth;

	ui->handler.next = NULL;
	ui->handler.prev = NULL;
	ui->handler.f = userintr_dispatch;
	ui->handler.data = ui;
	ui->handler.n = n;

	ui->f = f;
	ui->arg = arg;
	ui->process = process;
	ui->cond = cond;

#ifdef __TARGET_RISCV64
	/* Clear PGHD_USER attribute in interrupt handler code page (RISC-V specification forbids user code execution in kernel mode).
	 * Assumes that entire interrupt handler code lies within one page and is aligned to page boundary.
	 * No other user code should be placed in the same page.
	 */
	int attr = PGHD_READ | PGHD_EXEC | PGHD_PRESENT;
	pmap_enter(ui->process->pmapp, pmap_resolve(ui->process->pmapp, ui->f), (void *)((u64)ui->f & ~(SIZE_PAGE - 1)), attr, NULL);

	/* Save GP register for the interrupt handler */
	ui->handler.gp = hal_cpuGetGP();
#endif

	res = hal_interruptsSetHandler(&ui->handler);
	if (res != EOK) {
		if (cond != NULL) {
			cond_put(cond);
		}
		vm_kfree(ui);
		return res;
	}

	id = resource_alloc(process, &ui->resource);
	if (id < 0) {
		(void)hal_interruptsDeleteHandler(&ui->handler);
		if (cond != NULL) {
			cond_put(cond);
		}
		vm_kfree(ui);
		return -ENOMEM;
	}

	(void)resource_put(process, &ui->resource);

	return id;
}


userintr_t *userintr_active(void)
{
	return userintr_common.active;
}


void _userintr_init(void)
{
	userintr_common.active = NULL;
}
