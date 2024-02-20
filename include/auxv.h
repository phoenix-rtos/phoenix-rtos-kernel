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


#define AT_NULL   0
#define AT_PAGESZ 1
#define AT_BASE   2
#define AT_ENTRY  3
#define AT_PHDR   4
#define AT_PHENT  5
#define AT_PHNUM  6


#define AUXV_TYPE_COUNT 7


#endif
