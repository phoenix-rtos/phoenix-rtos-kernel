/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - event buffer implementation using large in-memory circular buffer
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */


#include "include/errno.h"
#include "vm/vm.h"

#include "buffer.h"


#define TRACE_BUFFER_SIZE (8 << 20) /* 8 MB */


static struct {
	vm_map_t *kmap;
	cbuffer_t buffer;
	page_t *pages;
} buffer_common;


static void _trace_bufferFree(void *data, page_t **pages)
{
	size_t sz = 0;
	page_t *p = *pages;

	while (p != NULL) {
		*pages = p->next;
		vm_pageFree(p);
		sz += SIZE_PAGE;
		p = *pages;
	}

	vm_munmap(buffer_common.kmap, data, sz);
}


static void *_trace_bufferAlloc(page_t **pages, size_t sz)
{
	page_t *p;
	void *v, *data;
	int err;

	*pages = NULL;
	data = vm_mapFind(buffer_common.kmap, NULL, sz, MAP_NONE, PROT_READ | PROT_WRITE);

	if (data == NULL) {
		return NULL;
	}

	for (v = data; v < data + sz; v += SIZE_PAGE) {
		p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

		if (p == NULL) {
			err = -ENOMEM;
			break;
		}

		p->next = *pages;
		*pages = p;

		err = page_map(&buffer_common.kmap->pmap, v, p->addr, PGHD_PRESENT | PGHD_WRITE | PGHD_READ);
		if (err < 0) {
			break;
		}
	}

	if (err < 0) {
		_trace_bufferFree(data, pages);
		return NULL;
	}

	return data;
}


int _trace_bufferStart(void)
{
	void *data = _trace_bufferAlloc(&buffer_common.pages, TRACE_BUFFER_SIZE);

	if (data == NULL) {
		return -ENOMEM;
	}

	return _cbuffer_init(&buffer_common.buffer, data, TRACE_BUFFER_SIZE);
}


int _trace_bufferRead(void *buf, size_t bufsz)
{
	return _cbuffer_read(&buffer_common.buffer, buf, bufsz);
}


int _trace_bufferWrite(const void *data, size_t sz)
{
	return _cbuffer_write(&buffer_common.buffer, data, sz);
}


int _trace_bufferWaitUntilAvail(size_t sz)
{
	/* overwrite intentionally to prevent deadlock */
	return 0;
}


int _trace_bufferFinish(void)
{
	_trace_bufferFree(buffer_common.buffer.data, &buffer_common.pages);

	return EOK;
}


int trace_bufferInit(vm_map_t *kmap)
{
	buffer_common.kmap = kmap;

	return EOK;
}
