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

#include "include/errno.h"
#include "include/arch/ia32/ia32.h"
#include "halsyspage.h"


int hal_acpiGet(acpi_var_t var, int *value)
{
	switch (var) {
		case acpi_rsdpAddr:
			if (syspage->hs.acpi_version != ACPI_RSDP) {
				return -EINVAL;
			}
			*value = syspage->hs.rsdp;
			return EOK;
	}

	return -EINVAL;
}
