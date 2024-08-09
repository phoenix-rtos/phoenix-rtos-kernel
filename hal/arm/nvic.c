/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Nested Vector Interrupt Controller
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <arch/cpu.h>
#include "nvic.h"


static struct {
	volatile u32 *nvic;
} nvic_common;


/* clang-format off */
enum { nvic_iser = 0, nvic_icer = 32, nvic_ispr = 64, nvic_icpr = 96, nvic_iabr = 128,
	nvic_ip = 192 };
/* clang-format on */


void _hal_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = nvic_common.nvic + ((u8)irqn >> 5) + ((state != 0) ? nvic_iser : nvic_icer);
	*ptr = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u8 *ptr;

	ptr = ((u8*)(nvic_common.nvic + nvic_ip)) + irqn;

	*ptr = (priority << 4) & 0xff;
}


void _hal_nvicSetPending(s8 irqn)
{
	volatile u32 *ptr = nvic_common.nvic + ((u8)irqn >> 5) + nvic_ispr;

	*ptr = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
}


void _hal_nvicInit(void)
{
	nvic_common.nvic = (void *)0xe000e100;
}
