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


unsigned int _cbuffer_init(cbuffer_t *buf, void *data, size_t sz)
{
	hal_memset(buf, 0, sizeof(cbuffer_t));
	buf->sz = sz;
	buf->data = data;
	return 0U;
}


unsigned int _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz)
{
	unsigned int bytes = 0;

	if (sz == 0 || buf->full != 0) {
		return 0;
	}

	if (buf->r > buf->w) {
		hal_memcpy(buf->data + buf->w, data, bytes = min(sz, buf->r - buf->w));
	}

	else {
		hal_memcpy(buf->data + buf->w, data, bytes = min(sz, buf->sz - buf->w));

		if (bytes < sz && buf->r != 0) {
			data += bytes;
			sz -= bytes;
			hal_memcpy(buf->data, data, min(sz, buf->r));
			bytes += min(sz, buf->r);
		}
	}

	buf->w = (buf->w + bytes) & (buf->sz - 1);
	buf->full = buf->w == buf->r;

	return bytes;
}


unsigned int _cbuffer_read(cbuffer_t *buf, void *data, size_t sz)
{
	unsigned int bytes = _cbuffer_peek(buf, data, sz);

	if (bytes > 0) {
		buf->r = (buf->r + bytes) & (buf->sz - 1);
		buf->full = 0;
	}

	return bytes;
}


unsigned int _cbuffer_peek(const cbuffer_t *buf, void *data, size_t sz)
{
	unsigned int bytes = 0;

	if (sz == 0 || (buf->r == buf->w && buf->full == 0)) {
		return 0;
	}
	if (buf->w > buf->r) {
		hal_memcpy(data, buf->data + buf->r, bytes = min(sz, buf->w - buf->r));
	}
	else {
		hal_memcpy(data, buf->data + buf->r, bytes = min(sz, buf->sz - buf->r));

		if (bytes < sz) {
			data += bytes;
			sz -= bytes;
			hal_memcpy(data, buf->data, min(sz, buf->w));
			bytes += min(sz, buf->w);
		}
	}

	return bytes;
}
