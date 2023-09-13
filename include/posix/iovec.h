/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX struct iovec
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_IOVEC_H_
#define _PHOENIX_POSIX_IOVEC_H_

#include "limits.h"
#include "types.h"


#define UIO_MAXIOV IOV_MAX /* Maximum number of scatter/gather elements the system can process in one call */


struct iovec {
	void *iov_base; /* Base address of input/output memory region */
	size_t iov_len; /* Size of memory pointed to be iov_base */
};


#endif
