/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for riscv64-generic
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/hal.h"
#include "include/arch/riscv64/riscv64.h"

static struct {
	spinlock_t lock;
} generic_common;


int hal_platformctl(void *ptr)
{
	platformctl_t *pctl = ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&generic_common.lock, &sc);

	switch (pctl->type) {
		case pctl_reboot:
			if ((pctl->action == pctl_set) && (pctl->task.reboot.magic == PCTL_REBOOT_MAGIC)) {
				hal_cpuReboot();
			}
			break;

		case pctl_iomux:
			ret = 0;
			break;

		default:
			break;
	}
	hal_spinlockClear(&generic_common.lock, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&generic_common.lock, "generic_common.lock");
}
