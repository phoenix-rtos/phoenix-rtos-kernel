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


int userintr_setHandler(unsigned int n, int (*f)(unsigned int, void *), void *arg, unsigned int cond, unsigned int *h)
{
	resource_t *r, *t;
	process_t *p = proc_current()->process;
	int res;

	if ((r = resource_alloc(p, h)) == NULL)
		return -ENOMEM;

	r->type = rtInth;
	r->inth.next = NULL;
	r->inth.prev = NULL;
	r->inth.f = (int (*)(unsigned int, cpu_context_t *, void *))f;
	r->inth.data = arg;
	r->inth.pmap = NULL;
	r->inth.cond = NULL;
	r->inth.n = n;

	if (p != NULL) {
		r->inth.pmap = &p->mapp->pmap;
		if (cond > 0 && ((t = resource_get(p, cond)) != NULL))
			r->inth.cond = &t->waitq;
	}

	if ((res = hal_interruptsSetHandler(&r->inth)) != EOK) {
		resource_free(r);
		return res;
	}

	return EOK;
}


int userintr_dispatch(intr_handler_t *h)
{
	int ret;
	int (*f)(int, void *) = (int (*)(int, void *))h->f;
	process_t *p;

	p = (proc_current())->process;

	/* Switch into the handler address space */
	pmap_switch(h->pmap);

	ret = f(h->n, h->data);

	if (ret >= 0 && h->cond != NULL)
		proc_threadWakeup((thread_t **)h->cond);

	/* Restore process address space */
	if ((p != NULL) && (p->mapp != NULL))
		pmap_switch(&p->mapp->pmap);

	return ret;
}
