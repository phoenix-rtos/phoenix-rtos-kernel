/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver (HAL RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "riscv64.h"
#include "sbi.h"
#include "hal/string.h"

#include <arch/timer.h>

#include <board_config.h>


static struct {
	intr_handler_t handler;
	spinlock_t sp;

	u64 interval;
} timer_common;


static void timerSet(u64 val)
{
#ifdef __riscv_sstc
	csr_write(stimecmp, val);
#else
	hal_sbiSetTimer(val);
#endif
}


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	timerSet(csr_read(time) + timer_common.interval);

	return 0;
}


void hal_timerSetWakeup(u32 waitUs)
{
	timerSet(csr_read(time) + waitUs * ((u64)TIMER_FREQ / (1000UL * 1000UL)));
}


time_t hal_timerGetUs(void)
{
	return (time_t)csr_read(time) / ((time_t)TIMER_FREQ / (1000 * 1000));
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = (unsigned int)SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	const char str[] = "Using RISC-V timer, ";
	size_t n = sizeof(str) - 1U;

	if (len != 0U) {
		(void)hal_strncpy(features, str, len);
		if (len > (n + 1U)) {
#ifdef __riscv_sstc
			(void)hal_strncpy(features + n, "Sstc", len - n);
#else
			(void)hal_strncpy(features + n, "SBI", len - n);
#endif
		}
		features[len - 1U] = '\0';
	}
	return features;
}


void hal_timerInitCore(void)
{
	timerSet(csr_read(time) + timer_common.interval);
}


__attribute__((section(".init"))) void _hal_timerInit(u32 interval)
{
	timer_common.interval = interval * ((unsigned int)TIMER_FREQ / 1000000UL);

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = timer_irqHandler;
	timer_common.handler.n = (unsigned int)SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	timerSet(csr_read(time) + timer_common.interval);
}
