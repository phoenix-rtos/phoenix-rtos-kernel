/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance tracing subsystem
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */


#include "hal/hal.h"
#include "lib/lib.h"
#include "include/errno.h"

#include "buffer.h"


#define TRACE_BUFFER_SIZE (4 << 20) /* 4 MB */


struct {
	vm_map_t *kmap;
	spinlock_t spinlock;

	time_t lastTimestamp;
	cbuffer_t buffer;
	page_t *pages;

	time_t prev;
} buffer_common;


static void trace_bufferFree(void *data, page_t **pages)
{
	size_t sz = 0;
	page_t *p;

	p = *pages;
	while (p != NULL) {
		*pages = p->next;
		vm_pageFree(p);
		sz += SIZE_PAGE;
		p = *pages;
	}

	vm_munmap(buffer_common.kmap, data, sz);
}


static void *trace_bufferAlloc(page_t **pages, size_t sz)
{
	page_t *p;
	void *v, *data;

	*pages = NULL;
	data = vm_mapFind(buffer_common.kmap, NULL, sz, MAP_NONE, PROT_READ | PROT_WRITE);

	if (data == NULL) {
		return NULL;
	}

	for (v = data; v < data + sz; v += SIZE_PAGE) {
		p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

		if (p == NULL) {
			trace_bufferFree(data, pages);
			return NULL;
		}

		p->next = *pages;
		*pages = p;
		page_map(&buffer_common.kmap->pmap, v, p->addr, PGHD_PRESENT | PGHD_WRITE | PGHD_READ);
	}

	return data;
}


int trace_bufferStart(void)
{
	void *data;

	data = trace_bufferAlloc(&buffer_common.pages, TRACE_BUFFER_SIZE);

	if (data == NULL) {
		return -ENOMEM;
	}

	_cbuffer_init(&buffer_common.buffer, data, TRACE_BUFFER_SIZE);

	return EOK;
}


int trace_bufferRead(void *buf, size_t bufsz)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&buffer_common.spinlock, &sc);
	bufsz = _cbuffer_read(&buffer_common.buffer, buf, bufsz);
	hal_spinlockClear(&buffer_common.spinlock, &sc);

	return bufsz;
}


int trace_bufferFinish(void)
{
	void *data = buffer_common.buffer.data;
	page_t **pages = &buffer_common.pages;

	size_t sz = 0;
	page_t *p;

	p = *pages;
	while (p != NULL) {
		*pages = p->next;
		vm_pageFree(p);
		sz += SIZE_PAGE;
		p = *pages;
	}

	vm_munmap(buffer_common.kmap, data, sz);

	return EOK;
}


void trace_bufferWrite(void *data, size_t sz)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&buffer_common.spinlock, &sc);

	_cbuffer_write(&buffer_common.buffer, data, sz);

	hal_spinlockClear(&buffer_common.spinlock, &sc);
}


int trace_bufferInit(vm_map_t *kmap)
{
	buffer_common.kmap = kmap;

	hal_spinlockCreate(&buffer_common.spinlock, "trace.spinlock");

	return EOK;
}
