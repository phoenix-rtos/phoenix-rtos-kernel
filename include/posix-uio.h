/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - uio
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_POSIX_UIO_H_
#define _PH_POSIX_UIO_H_


#include "types.h"


struct iovec {
	void *iov_base;
	size_t iov_len;
};


#endif
