/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * utsname
 *
 * Copyright 2025 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_UTSNAME_H_
#define _PHOENIX_UTSNAME_H_


#include "limits.h"


struct utsname {
	char sysname[16];
	char nodename[HOST_NAME_MAX + 1U];
	char release[16];
	char version[32];
	char machine[16];
};


#endif
