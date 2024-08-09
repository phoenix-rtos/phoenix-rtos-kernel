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


struct nvic_s {
	volatile u32 iser;
	u32 _res0[31];
	volatile u32 icer;
	u32 _res1[31];
	volatile u32 ispr;
	u32 _res2[31];
	volatile u32 icpr;
	u32 _res3[31];
	volatile u32 iabr;
	u32 _res4[63];
	volatile u32 ip;
};


static struct {
	struct nvic_s *nvic;
} nvic_common;


void _hal_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = (state != 0) ? &nvic_common.nvic->iser : &nvic_common.nvic->icer;

	*(ptr + ((u8)irqn >> 5)) = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u8 *ptr = (volatile u8 *)&nvic_common.nvic->ip;

	*(ptr + irqn) = (priority << 4) & 0xff;

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicSetPending(s8 irqn)
{
	volatile u32 *ptr = &nvic_common.nvic->ispr;

	*(ptr + ((u8)irqn >> 5)) = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicInit(void)
{
	nvic_common.nvic = (void *)0xe000e100;
}
