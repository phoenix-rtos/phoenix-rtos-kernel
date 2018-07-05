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


int proc_fileAdd(unsigned int *h, oid_t *oid, unsigned mode)
{
	process_t *process;
	resource_t *r;

	process = proc_current()->process;

	if ((r = resource_alloc(process, h, rtFile)) == NULL)
		return -ENOMEM;

	hal_memcpy(&r->fd->oid, oid, sizeof(oid_t));
	r->fd->offs = 0;
	r->fd->mode = mode;
	resource_put(process, r);

	return EOK;
}


int proc_fileSet(unsigned int h, char flags, oid_t *oid, offs_t offs, unsigned mode)
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
		hal_memcpy(&r->fd->oid, oid, sizeof(oid_t));
	if (flags & 2)
		r->fd->offs = offs;
	if (flags & 4)
		r->fd->mode = mode;

	resource_put(process, r);

	return EOK;
}


int proc_fileGet(unsigned int h, char flags, oid_t *oid, offs_t *offs, unsigned *mode)
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
		hal_memcpy(oid, &r->fd->oid, sizeof(oid_t));

	if (flags & 2)
		*offs = r->fd->offs;

	if (flags & 4)
		*mode = r->fd->mode;

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
	hal_memcpy(&dst->fd->oid, &src->fd->oid, sizeof(oid_t));
	dst->fd->offs = src->fd->offs;
	dst->fd->mode = src->fd->mode;

	return EOK;
}
