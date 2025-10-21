/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for riscv64-gr765
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/gaisler/ambapp.h"

#include "hal/hal.h"
#include "include/arch/riscv64/riscv64.h"

static struct {
	spinlock_t lock;
} gr765_common;


int hal_platformctl(void *ptr)
{
	platformctl_t *pctl = ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&gr765_common.lock, &sc);

	switch (pctl->type) {
		case pctl_reboot:
			if ((pctl->action == pctl_set) && (pctl->task.reboot.magic == PCTL_REBOOT_MAGIC)) {
				hal_cpuReboot();
			}
			break;

		case pctl_iomux:
			ret = 0;
			break;

		case pctl_ambapp:
			if (pctl->action == pctl_get) {
				ret = ambapp_findSlave(pctl->task.ambapp.dev, pctl->task.ambapp.instance);
			}
			break;

		default:
			/* No action required */
			break;
	}
	hal_spinlockClear(&gr765_common.lock, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr765_common.lock, "gr765_common.lock");
	ambapp_init();
}
