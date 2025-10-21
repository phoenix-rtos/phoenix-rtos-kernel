/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Cortex R52 MPS3 AN536 functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/hal.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "include/arch/armv8r/mps3an536/mps3an536.h"


static struct {
	spinlock_t lock;
} mps3an536_common;


int hal_platformctl(void *ptr)
{
	spinlock_ctx_t sc;
	platformctl_t *data = ptr;
	int ret = -1;

	hal_spinlockSet(&mps3an536_common.lock, &sc);

	switch (data->type) {
		case pctl_reboot:
			if (data->action == pctl_set) {
				if (data->task.reboot.magic == PCTL_REBOOT_MAGIC) {
					hal_cpuReboot();
				}
			}
			break;

		default:
			/* No action required */
			break;
	}

	hal_spinlockClear(&mps3an536_common.lock, &sc);

	return ret;
}


unsigned int hal_cpuGetCount(void)
{
	return 1;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&mps3an536_common.lock, "mps3an536_common.lock");
}
