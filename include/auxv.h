/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Auxiliary vector definitions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_AUXV_H_
#define _PHOENIX_AUXV_H_


#include "types.h"


struct auxInfo {
	__u32 a_type; /* Type of element. */
	__u64 a_v;    /* Value of element. */
};


#define AT_NULL  0 /* End of auxiliary vector. */
#define AT_PHDR  1 /* Location of program header table. */
#define AT_PHENT 2 /* Size of one entry in program header table. */
#define AT_PHNUM 3 /* Number of entries in program header table. */


#define AUXV_TYPE_COUNT 4 /* Number of auxiliary vector element types. */


#endif
