/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module - circular buffers
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib.h"
#include "vm/vm.h"


void _cbuffer_init(cbuffer_t *buf, void *data, size_t sz)
{
	hal_memset(buf, 0, sizeof(cbuffer_t));
	buf->sz = sz;
	buf->data = data;
}


size_t _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz)
{
	size_t bytes = 0;

	if (sz == 0U || buf->full != 0U) {
		return 0U;
	}

	if (buf->r > buf->w) {
		bytes = min(sz, buf->r - buf->w);
		hal_memcpy(buf->data + buf->w, data, bytes);
	}

	else {
		bytes = min(sz, buf->sz - buf->w);
		hal_memcpy(buf->data + buf->w, data, bytes);

		if ((bytes < sz) && (buf->r != 0U)) {
			data += bytes;
			sz -= bytes;
			hal_memcpy(buf->data, data, min(sz, buf->r));
			bytes += min(sz, buf->r);
		}
	}

	buf->w = (buf->w + bytes) & (buf->sz - 1U);
	buf->full = (buf->w == buf->r) ? 1U : 0U;

	return bytes;
}


size_t _cbuffer_read(cbuffer_t *buf, void *data, size_t sz)
{
	size_t bytes = _cbuffer_peek(buf, data, sz);

	if (bytes > 0U) {
		LIB_ASSERT(buf->sz > 0, "cbuffer: buf->sz=0");
		buf->r = (buf->r + bytes) & (buf->sz - 1U);
		buf->full = 0U;
	}

	return bytes;
}


size_t _cbuffer_peek(const cbuffer_t *buf, void *data, size_t sz)
{
	size_t bytes = 0;

	if (sz == 0U || (buf->r == buf->w && buf->full == 0U)) {
		return 0U;
	}
	if (buf->w > buf->r) {
		bytes = min(sz, buf->w - buf->r);
		hal_memcpy(data, buf->data + buf->r, bytes);
	}
	else {
		bytes = min(sz, buf->sz - buf->r);
		hal_memcpy(data, buf->data + buf->r, bytes);

		if (bytes < sz) {
			data += bytes;
			sz -= bytes;
			hal_memcpy(data, buf->data, min(sz, buf->w));
			bytes += min(sz, buf->w);
		}
	}

	return bytes;
}
