#include "posix_private.h"

int _cbuffer_init(cbuffer_t *buf, size_t sz)
{
	hal_memset(buf, 0, sizeof(cbuffer_t));

	if ((buf->data = vm_mmap(NULL, NULL, NULL, sz, PROT_READ | PROT_WRITE, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	buf->sz = sz;

	return 0;
}


int _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz)
{
	int bytes = 0;

	if (!sz || buf->full)
		return 0;

	if (buf->r > buf->w) {
		hal_memcpy(buf->data + buf->w, data, bytes = min(sz, buf->r - buf->w));
	}
	else {
		hal_memcpy(buf->data + buf->w, data, bytes = min(sz, buf->sz - buf->w));

		if (bytes < sz && buf->r) {
			buf += bytes;
			sz -= bytes;
			hal_memcpy(buf->data, data, min(sz, buf->r));
			bytes += min(sz, buf->r);
		}
	}

	buf->w = (buf->w + bytes) & (buf->sz - 1);
	buf->full = buf->w == buf->r;

	return bytes;
}


int _cbuffer_read(cbuffer_t *buf, void *data, size_t sz)
{
	int bytes = 0;

	if (!sz || (buf->r == buf->w && !buf->full))
		return 0;

	if (buf->w > buf->r) {
		hal_memcpy(data, buf->data + buf->r, bytes = min(sz, buf->w - buf->r));
	}
	else {
		hal_memcpy(data, buf->data + buf->r, bytes = min(sz, buf->sz - buf->r));

		if (bytes < sz) {
			buf += bytes;
			sz -= bytes;
			hal_memcpy(data, buf->data, min(sz, buf->w));
			bytes += min(sz, buf->w);
		}
	}

	buf->r = (buf->r + bytes) & (buf->sz - 1);
	buf->full = 0;

	return bytes;
}
