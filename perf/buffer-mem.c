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


#ifndef PERF_EVENT_CHANNEL_BUFSIZE
#define PERF_EVENT_CHANNEL_BUFSIZE (8 << 20) /* 8 MB */
#endif

#ifndef PERF_META_CHANNEL_BUFSIZE
#define PERF_META_CHANNEL_BUFSIZE (1 << 20) /* 1 MB */
#endif


static struct {
	vm_map_t *kmap;
	struct {
		cbuffer_t buffer;
		page_t *pages;
		size_t bufsize;
	} chans[perf_trace_channel_count];
} buffer_common;


static inline cbuffer_t *getBuffer(u8 chan)
{
	LIB_ASSERT(chan < perf_trace_channel_count, "invalid chan id: %d", chan);
	return &buffer_common.chans[chan].buffer;
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
	void *data;
	size_t i, j;

	for (i = 0; i < perf_trace_channel_count; i++) {
		data = _trace_bufferAlloc(&buffer_common.chans[i].pages, buffer_common.chans[i].bufsize);
		if (data == NULL) {
			for (j = 0; j < i; j++) {
				_trace_bufferFree(buffer_common.chans[j].buffer.data, &buffer_common.chans[j].pages);
			}
			return -ENOMEM;
		}

		if (_cbuffer_init(&buffer_common.chans[i].buffer, data, buffer_common.chans[i].bufsize) < 0) {
			for (j = 0; j <= i; j++) {
				_trace_bufferFree(buffer_common.chans[j].buffer.data, &buffer_common.chans[j].pages);
			}
			return -ENOMEM;
		}
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
	for (size_t i = 0; i < perf_trace_channel_count; i++) {
		_trace_bufferFree(buffer_common.chans[i].buffer.data, &buffer_common.chans[i].pages);
	}

	return EOK;
}


int trace_bufferInit(vm_map_t *kmap)
{
	buffer_common.kmap = kmap;

	buffer_common.chans[perf_trace_channel_event].bufsize = PERF_EVENT_CHANNEL_BUFSIZE;
	buffer_common.chans[perf_trace_channel_meta].bufsize = PERF_META_CHANNEL_BUFSIZE;

	return EOK;
}
