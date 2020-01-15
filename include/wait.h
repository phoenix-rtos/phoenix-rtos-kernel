/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Waiting for processes
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_WAIT_H_
#define _PHOENIX_WAIT_H_


enum {
	WNOHANG = 1 << 0,
	WUNTRACED = 1 << 1,
	WCONTINUED = 1 << 2,
};


#endif
