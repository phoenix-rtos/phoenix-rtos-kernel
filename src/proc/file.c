/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * File
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "threads.h"
#include "cond.h"
#include "resource.h"


int proc_fileAdd(unsigned int *h, oid_t *oid)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_alloc(process, h)) == NULL)
		return -ENOMEM;

	r->type = rtFile;
	hal_memcpy(&r->oid, oid, sizeof(oid_t));
	r->offs = 0;
	resource_put(process, r);

	return EOK;
}


int proc_fileSet(unsigned int h, char flags, oid_t *oid, offs_t offs)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	if (r->type != rtFile) {
		resource_put(process, r);
		return -EINVAL;
	}

	if (flags & 1)
		hal_memcpy(&r->oid, oid, sizeof(oid_t));
	if (flags & 2)
		r->offs = offs;

	resource_put(process, r);

	return EOK;
}


int proc_fileGet(unsigned int h, char flags, oid_t *oid, offs_t *offs)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	if (r->type != rtFile) {
		resource_put(process, r);
		return -EINVAL;
	}

	if (flags & 1)
		hal_memcpy(oid, &r->oid, sizeof(oid_t));

	if (flags & 2)
		*offs = r->offs;


	resource_put(process, r);

	return EOK;
}


int proc_fileRemove(unsigned int h)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_get(process, h)) == NULL)
		return -EINVAL;

	if (r->type != rtFile) {
		resource_put(process, r);
		return -EINVAL;
	}

	resource_free(r);

	return EOK;
}


int proc_fileCopy(resource_t *dst, resource_t *src)
{
	dst->type = rtFile;
	hal_memcpy(&dst->oid, &src->oid, sizeof(oid_t));
	dst->offs = src->offs;

	return EOK;
}
