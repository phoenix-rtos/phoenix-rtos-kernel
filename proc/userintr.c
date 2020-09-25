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

#include "resource.h"
#include "userintr.h"
#include "cond.h"
#include "../lib/lib.h"
#include "proc.h"


struct {
	userintr_t *volatile active;
} userintr_common;


int userintr_put(userintr_t *ui)
{
	int rem;

	if (!(rem = resource_put(&ui->resource))) {
		hal_interruptsDeleteHandler(&ui->handler);

		if (ui->cond != NULL)
			cond_put(ui->cond);

		vm_kfree(ui);
	}

	return rem;
}


static int userintr_dispatch(unsigned int n, cpu_context_t *ctx, void *arg)
{
	userintr_t *ui = arg;
	int ret, attr, reschedule = 0;
	process_t *p = NULL;

	if (proc_current() != NULL)
		p = (proc_current())->process;

	/* Switch into the handler address space */
	pmap_switch(ui->process->pmapp);

#ifdef TARGET_RISCV64
	/* Clear PGHD_USER attribute in interrupt handler code page (RISC-V specification forbids user code execution in kernel mode) */
	/* Assumes that entire interrupt handler code lies within one page */
	attr = PGHD_READ | PGHD_WRITE | PGHD_EXEC | PGHD_PRESENT;
	pmap_enter(ui->process->pmapp, pmap_resolve(ui->process->pmapp, ui->f), (void *)((u64)ui->f & ~(SIZE_PAGE - 1)), attr, NULL);
#endif

	userintr_common.active = ui;
	ret = ui->f(ui->handler.n, ui->arg);
	userintr_common.active = NULL;

#ifdef TARGET_RISCV64
	/* Restore PGHD_USER attribute */
	attr |= PGHD_USER;
	pmap_enter(ui->process->pmapp, pmap_resolve(ui->process->pmapp, ui->f), (void *)((u64)ui->f & ~(SIZE_PAGE - 1)), attr, NULL);
#endif

	if (ret >= 0 && ui->cond != NULL) {
		reschedule = 1;
		proc_threadWakeup(&ui->cond->queue);
	}

	/* Restore process address space */
	if ((p != NULL) && (p->pmapp != NULL))
		pmap_switch(p->pmapp);

	return reschedule;
}


int userintr_setHandler(unsigned int n, int (*f)(unsigned int, void *), void *arg, unsigned int c)
{
	process_t *process;
	userintr_t *ui;
	int res;

	process = proc_current()->process;

	if ((ui = vm_kmalloc(sizeof(userintr_t))) == NULL)
		return -ENOMEM;

	ui->handler.next = NULL;
	ui->handler.prev = NULL;
	ui->handler.f = userintr_dispatch;
	ui->handler.data = ui;
	ui->handler.n = n;

	ui->f = f;
	ui->arg = arg;
	ui->process = process;
	ui->cond = NULL;

	if (c == 0 || (ui->cond = cond_get(c)) != NULL) {
		if ((res = hal_interruptsSetHandler(&ui->handler)) == EOK) {
			if ((res = resource_alloc(process, &ui->resource, rtInth))) {
				userintr_put(ui);
				return res;
			}
			else {
				res = -ENOMEM;
			}

			hal_interruptsDeleteHandler(&ui->handler);
		}

		if (c) cond_put(ui->cond);
	}
	else {
		res = -EINVAL;
	}

	vm_kfree(ui);
	return res;
}


userintr_t *userintr_active(void)
{
	return userintr_common.active;
}


void _userintr_init(void)
{
	userintr_common.active = NULL;
}
