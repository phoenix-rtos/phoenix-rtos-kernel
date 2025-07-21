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


#include "include/perf.h"
#include "include/errno.h"
#include "vm/vm.h"

#include "buffer.h"
#include "trace.h"


#define TRACE_EVENT_BUFFER_SIZE (2 << 20) /* 8 MB */
#define TRACE_META_BUFFER_SIZE  (1 << 20) /* 1 MB */


static struct {
	vm_map_t *kmap;
	struct {
		cbuffer_t buffer;
		page_t *pages;
	} meta, event;
} buffer_common;


static inline cbuffer_t *getBuffer(u8 chan)
{
	cbuffer_t *cbuf = NULL;
	switch (chan) {
		case perf_trace_channel_meta:
			cbuf = &buffer_common.meta.buffer;
			break;
		case perf_trace_channel_event:
			cbuf = &buffer_common.event.buffer;
			break;
		default:
			cbuf = NULL;
			break;
	}
	LIB_ASSERT(cbuf != NULL, "wrong chan id: %d", chan);
	return cbuf;
}


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
	int err = EOK;

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
	void *data = _trace_bufferAlloc(&buffer_common.event.pages, TRACE_EVENT_BUFFER_SIZE);

	if (data == NULL) {
		return -ENOMEM;
	}

	if (_cbuffer_init(&buffer_common.event.buffer, data, TRACE_EVENT_BUFFER_SIZE) < 0) {
		_trace_bufferFree(buffer_common.event.buffer.data, &buffer_common.event.pages);
		return -ENOMEM;
	}

	data = _trace_bufferAlloc(&buffer_common.meta.pages, TRACE_META_BUFFER_SIZE);
	if (data == NULL) {
		_trace_bufferFree(buffer_common.event.buffer.data, &buffer_common.event.pages);
		return -ENOMEM;
	}

	if (_cbuffer_init(&buffer_common.meta.buffer, data, TRACE_META_BUFFER_SIZE) < 0) {
		_trace_bufferFree(buffer_common.meta.buffer.data, &buffer_common.meta.pages);
		_trace_bufferFree(buffer_common.event.buffer.data, &buffer_common.event.pages);
		return -ENOMEM;
	}

	return 0;
}


int _trace_bufferRead(u8 chan, void *buf, size_t bufsz)
{
	cbuffer_t *cbuf = getBuffer(chan);
	return _cbuffer_read(cbuf, buf, bufsz);
}


int _trace_bufferWrite(u8 chan, const void *data, size_t sz)
{
	cbuffer_t *cbuf = getBuffer(chan);
	return _cbuffer_write(cbuf, data, sz);
}


int _trace_bufferWaitUntilAvail(u8 chan, size_t sz)
{
	/* overwrite intentionally to prevent deadlock */
	return 0;
}


int _trace_bufferAvail(u8 chan)
{
	cbuffer_t *cbuf = getBuffer(chan);
	return _cbuffer_free(cbuf);
}


int _trace_bufferDiscard(u8 chan, size_t sz)
{
	cbuffer_t *cbuf = getBuffer(chan);
	return _cbuffer_discard(cbuf, sz);
}


int _trace_bufferFinish(void)
{
	_trace_bufferFree(buffer_common.meta.buffer.data, &buffer_common.meta.pages);
	_trace_bufferFree(buffer_common.event.buffer.data, &buffer_common.event.pages);

	return EOK;
}


int trace_bufferInit(vm_map_t *kmap)
{
	buffer_common.kmap = kmap;

	return EOK;
}
