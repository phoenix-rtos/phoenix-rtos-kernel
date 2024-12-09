/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ACPI kernel-userspace interface
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _HAL_ACPI_H_
#define _HAL_ACPI_H_

#include "include/arch/ia32/ia32.h"


extern int hal_acpiGet(acpi_var_t var, int *value);


#endif
