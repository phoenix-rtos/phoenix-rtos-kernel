/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process resources
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "mutex.h"
#include "threads.h"
#include "cond.h"
#include "resource.h"
#include "name.h"
#include "userintr.h"

#define RESOURCE_ID_MIN 1


int resource_alloc(process_t *process, resource_t *r)
{
	int id;

	r->refs = 2;

	proc_lockSet(&process->lock);
	id = lib_idtreeAlloc(&process->resources, &r->linkage, RESOURCE_ID_MIN);
	proc_lockClear(&process->lock);

	return id;
}


resource_t *resource_get(process_t *process, int id)
{
	resource_t *r;

	proc_lockSet(&process->lock);
	r = lib_idtreeof(resource_t, linkage, lib_idtreeFind(&process->resources, id));
	if (r != NULL) {
		++r->refs;
	}
	proc_lockClear(&process->lock);

	return r;
}


int resource_put(process_t *process, resource_t *r)
{
	return lib_atomicDecrement(&r->refs);
}


static resource_t *resource_remove(process_t *process, int id)
{
	resource_t *r;

	proc_lockSet(&process->lock);
	r = lib_idtreeof(resource_t, linkage, lib_idtreeFind(&process->resources, id));
	if (r != NULL) {
		lib_idtreeRemove(&process->resources, &r->linkage);
	}
	proc_lockClear(&process->lock);

	return r;
}


static void proc_resourcePut(resource_t *r)
{
	switch (r->type) {
		case rtLock:
			mutex_put(r->payload.mutex);
			break;

		case rtCond:
			cond_put(r->payload.cond);
			break;

		case rtInth:
			userintr_put(r->payload.userintr);
			break;

		default:
			LIB_ASSERT(0, "invalid resource type %d", (int)r->type);
			break;
	}
}


int proc_resourceDestroy(process_t *process, int id)
{
	resource_t *r;

	r = resource_remove(process, id);
	if (r == NULL) {
		return -EINVAL;
	}

	proc_resourcePut(r);

	return EOK;
}


void proc_resourcesDestroy(process_t *process)
{
	resource_t *r;

	for (;;) {
		proc_lockSet(&process->lock);
		r = lib_idtreeof(resource_t, linkage, lib_idtreeMinimum(process->resources.root));
		if (r == NULL) {
			proc_lockClear(&process->lock);
			break;
		}

		lib_idtreeRemove(&process->resources, &r->linkage);
		proc_lockClear(&process->lock);

		proc_resourcePut(r);
	}
}


int proc_resourcesCopy(process_t *source)
{
	process_t *process = proc_current()->process;
	idnode_t *n;
	resource_t *r, *newr;
	int err = EOK;

	proc_lockSet(&source->lock);
	for (n = lib_idtreeMinimum(source->resources.root); n != NULL; n = lib_idtreeNext(&n->linkage)) {
		r = lib_idtreeof(resource_t, linkage, n);

		switch (r->type) {
			case rtLock:
				err = proc_mutexCreate(&r->payload.mutex->lock.attr);
				break;

			case rtCond:
				err = proc_condCreate();
				break;

			default:
				/* Don't copy interrupt handlers */
				continue;
		}

		if ((err > 0) && (err != r->linkage.id)) {
			/* Reinsert resource to match original resource id */
			newr = resource_remove(process, err);
			if (newr == NULL) {
				err = -EINVAL;
				break;
			}
			newr->linkage.id = r->linkage.id;
			err = lib_idtreeInsert(&process->resources, &newr->linkage);
		}

		if (err < 0) {
			break;
		}
	}
	proc_lockClear(&source->lock);

	return (err >= 0) ? EOK : err;
}


void _resource_init(process_t *process)
{
	lib_idtreeInit(&process->resources);
}
