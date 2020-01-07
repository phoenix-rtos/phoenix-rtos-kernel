/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * uio.h
 *
 * Copyright 2020 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_UIO_H_
#define _PHOENIX_UIO_H_

struct iovec {
	void *iov_base;
	unsigned int /* TODO: size_t */ iov_len;
};

#endif
