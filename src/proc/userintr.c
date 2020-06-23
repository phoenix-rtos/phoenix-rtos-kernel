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
	int ret;
	process_t *p;

	p = (proc_current())->process;

	/* Switch into the handler address space */
	pmap_switch(ui->process->pmapp);

	userintr_common.active = ui;
	ret = ui->f(ui->handler.n, ui->arg);
	userintr_common.active = NULL;

	if (ret >= 0 && ui->cond != NULL)
		proc_threadWakeupYield(&ui->cond->queue);

	/* Restore process address space */
	if ((p != NULL) && (p->mapp != NULL))
		pmap_switch(p->pmapp);

	return 0;
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
