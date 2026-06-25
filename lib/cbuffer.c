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
#include "cbuffer.h"
#include "vm/vm.h"


void _cbuffer_init(cbuffer_t *buf, void *data, size_t sz)
{
	LIB_ASSERT_ALWAYS(((sz == 0U) || ((sz & (sz - 1U)) == 0U)), "cbuffer's size has to be either 0 or a power of 2");

	hal_memset(buf, 0, sizeof(cbuffer_t));
	buf->sz = sz;
	buf->data = data;
}


size_t _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz)
{
	size_t bytes = 0;

	/* FIXME: see note in _cbuffer_discard */
	LIB_ASSERT(buf->sz > 0U, "attempted to write to zero-sized buffer");

	if (sz == 0U || buf->sz == 0U || buf->full != 0U) {
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
		/* parasoft-suppress-next-line MISRAC2012-RULE_4_1-k "buf->sz ensured > 0 if _cbuffer_peek() > 0" */
		buf->r = (buf->r + bytes) & (buf->sz - 1U);
		buf->full = 0U;
	}

	return bytes;
}


size_t _cbuffer_peekOffs(const cbuffer_t *buf, void *data, size_t sz, size_t offs)
{
	size_t bytes = 0;
	size_t read = (buf->r + offs) & (buf->sz - 1U);

	/* FIXME: see note in _cbuffer_discard */
	LIB_ASSERT(buf->sz > 0U, "attempted to peek at zero-sized buffer");
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_7-a "there's no more checks for _cbuffer_avail's return value to be made here" */
	LIB_ASSERT(offs <= _cbuffer_avail(buf), "attempted to peek at offset %zu when there's only %zu bytes in the buffer", offs, _cbuffer_avail(buf));

	if (sz == 0U || buf->sz == 0U || (buf->r == buf->w && buf->full == 0U) || offs == _cbuffer_avail(buf)) {
		return 0U;
	}

	if (buf->w > read) {
		bytes = min(sz, buf->w - read);
		hal_memcpy(data, buf->data + read, bytes);
	}
	else {
		bytes = min(sz, buf->sz - read);
		hal_memcpy(data, buf->data + read, bytes);

		if (bytes < sz) {
			data += bytes;
			sz -= bytes;
			hal_memcpy(data, buf->data, min(sz, buf->w));
			bytes += min(sz, buf->w);
		}
	}

	return bytes;
}
