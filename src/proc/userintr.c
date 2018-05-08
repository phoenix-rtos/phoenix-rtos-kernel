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
#include "../lib/lib.h"
#include "proc.h"


int userintr_setHandler(unsigned int n, int (*f)(unsigned int, void *), void *arg, unsigned int cond)
{
	intr_handler_t *e;
	resource_t *r;
	int res;
	process_t *p = proc_current()->process;

	if ((e = vm_kmalloc(sizeof(intr_handler_t))) == NULL)
		return -ENOMEM;

	e->next = NULL;
	e->prev = NULL;
	e->f = (int (*)(unsigned int, cpu_context_t *, void *))f;
	e->data = arg;
	e->pmap = NULL;
	e->cond = NULL;

	if (p != NULL) {
		e->pmap = &p->mapp->pmap;
		if (cond > 0 && ((r = resource_get(p, cond)) != NULL))
			e->cond = &r->waitq;
	}

	if ((res = hal_interruptsSetHandler(n, e)) != EOK) {
		vm_kfree(e);
		return res;
	}

	return EOK;
}


int userintr_dispatch(unsigned int n, intr_handler_t *h)
{
	int ret;
	int (*f)(int, void *) = (int (*)(int, void *))h->f;
	process_t *p;

	p = (proc_current())->process;

	/* Switch into the handler address space */
	pmap_switch(h->pmap);

	ret = f(n, h->data);

	if (ret >= 0 && h->cond != NULL)
		proc_threadWakeup((thread_t **)h->cond);

	/* Restore process address space */
	if ((p != NULL) && (p->mapp != NULL))
		pmap_switch(&p->mapp->pmap);

	return ret;
}
