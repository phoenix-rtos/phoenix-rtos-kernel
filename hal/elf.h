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

#ifndef _HAL_ELF_H_
#define _HAL_ELF_H_


#ifdef NOMMU


#include <arch/elf.h>


static int hal_isRelReloc(int relType);


static int hal_isFuncdescValueReloc(int relType);


#endif


#endif
