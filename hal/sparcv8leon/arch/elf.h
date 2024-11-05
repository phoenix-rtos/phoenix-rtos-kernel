/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer ELF support
 *
 * Copyright 2024 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define R_SPARC_32 3


static inline int hal_isRelReloc(int relType)
{
	return (relType == R_SPARC_32) ? 1 : 0;
}
