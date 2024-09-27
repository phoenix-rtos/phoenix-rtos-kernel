/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Cortex-M System Control Space
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Gerard Swiderski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <arch/cpu.h>
#include "scs.h"


struct scs_s {
	u32 _res0[2];
	volatile u32 actlr;
	u32 _res1;
	volatile u32 csr;
	volatile u32 rvr;
	volatile u32 cvr;
	volatile u32 calib;
	u32 _res2[56];
	volatile u32 iser[8];
	u32 _res3[24];
	volatile u32 icer[8];
	u32 _res4[24];
	volatile u32 ispr[8];
	u32 _res5[24];
	volatile u32 icpr[8];
	u32 _res6[24];
	volatile u32 iabr[8];
	u32 _res7[56];
	volatile u32 ip[60];
	u32 _res8[516];
	volatile u32 cpuid;
	volatile u32 icsr;
	volatile u32 vtor;
	volatile u32 aircr;
	volatile u32 scr;
	volatile u32 ccr;
	volatile u32 shpr1;
	volatile u32 shpr2;
	volatile u32 shpr3;
	volatile u32 shcsr;
	volatile u32 cfsr;
	volatile u32 hfsr;
	u32 _res9;
	volatile u32 mmfar;
	volatile u32 bfar;
	volatile u32 afsr;
	u32 _res10[14];
	volatile u32 clidr;
	volatile u32 ctr;
	volatile u32 ccsidr;
	volatile u32 csselr;
	volatile u32 cpacr;
	u32 _res11[106];
	volatile u32 fpccr;
	volatile u32 fpcar;
	volatile u32 fpdscr;
	u32 _res12[4];
	volatile u32 iciallu;
	u32 _res13;
	volatile u32 icimvau;
	volatile u32 dcimvac;
	volatile u32 dcisw;
	volatile u32 dccmvau;
	volatile u32 dccvac;
	volatile u32 dccsw;
	volatile u32 dccimvac;
	volatile u32 dccisw;
	u32 _res14[6];
	volatile u32 itcmcr;
	volatile u32 dtcmcr;
	volatile u32 ahbpcr;
	volatile u32 cacr;
	volatile u32 ahbscr;
	u32 _res15;
	volatile u32 abfsr;
};


static struct {
	struct scs_s *scs;
} scs_common;


void _hal_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = (state != 0) ? scs_common.scs->iser : scs_common.scs->icer;

	*(ptr + ((u8)irqn >> 5)) = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u8 *ptr = (volatile u8 *)scs_common.scs->ip;

	*(ptr + irqn) = (priority << 4) & 0xff;

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_nvicSetPending(s8 irqn)
{
	volatile u32 *ptr = scs_common.scs->ispr;

	*(ptr + ((u8)irqn >> 5)) = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


int _hal_nvicGetPendingIRQ(s8 irqn)
{
	volatile u32 *ptr = &scs_common.scs->ispr[(u8)irqn >> 5];
	return ((*ptr & (1 << (irqn & 0x1f))) != 0) ? 1 : 0;
}


int _hal_nvicGetActive(s8 irqn)
{
	volatile u32 *ptr = &scs_common.scs->iabr[(u8)irqn >> 5];
	return ((*ptr & (1 << (irqn & 0x1f))) != 0) ? 1 : 0;
}


void _hal_scbSetPriorityGrouping(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = scs_common.scs->aircr & ~0xffff0700;

	/* Set AIRCR.PRIGROUP to 3: 16 priority groups and 16 subgroups
	   The value is same as for armv7m4-stm32l4x6 target
	   Setting various priorities is not supported on Phoenix-RTOS, so it's just default value */
	scs_common.scs->aircr = t | 0x5fa0000 | ((group & 7) << 8);
}


u32 _hal_scbGetPriorityGrouping(void)
{
	return (scs_common.scs->aircr & 0x700) >> 8;
}


void _hal_scbSetPriority(s8 excpn, u32 priority)
{
	volatile u8 *ptr = (u8 *)&scs_common.scs->shpr1 + excpn - 4;

	/* We set only group priority field */
	*ptr = (priority << 4) & 0xff;
}


u32 _imxrt_scbGetPriority(s8 excpn)
{
	volatile u8 *ptr = (u8 *)&scs_common.scs->shpr1 + excpn - 4;

	return *ptr >> 4;
}


void _hal_scbSystemReset(void)
{
	scs_common.scs->aircr = ((0x5fau << 16) | (scs_common.scs->aircr & (0x700u)) | (1u << 2));

	hal_cpuDataSyncBarrier();

	for (;;) {
		hal_cpuHalt();
	}
}


unsigned int _hal_scbCpuid(void)
{
	return scs_common.scs->cpuid;
}


void _hal_scbSetFPU(int state)
{
	if (state != 0) {
		scs_common.scs->cpacr |= 0xf << 20;
	}
	else {
		scs_common.scs->cpacr = 0;
		scs_common.scs->fpccr = 0;
	}
	hal_cpuDataSyncBarrier();
}


static int _hal_scbCacheIsSupported(void)
{
	u32 partno = ((_hal_scbCpuid() >> 4) & 0xfff);

	/* Only supported on Cortex-M7 for now */
	if (partno == 0xc27) {
		return 1;
	}

	return 0;
}


void _hal_scbEnableDCache(void)
{
	u32 ccsidr, sets, ways;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if ((scs_common.scs->ccr & (1 << 16)) == 0) {
		scs_common.scs->csselr = 0;
		hal_cpuDataSyncBarrier();

		ccsidr = scs_common.scs->ccsidr;

		/* Invalidate D$ */
		sets = (ccsidr >> 13) & 0x7fff;
		do {
			ways = (ccsidr >> 3) & 0x3ff;
			do {
				scs_common.scs->dcisw = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
			} while (ways-- != 0);
		} while (sets-- != 0);
		hal_cpuDataSyncBarrier();

		scs_common.scs->ccr |= 1 << 16;

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}
}


void _hal_scbDisableDCache(void)
{
	register u32 ccsidr, sets, ways;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	scs_common.scs->csselr = 0;
	hal_cpuDataSyncBarrier();

	scs_common.scs->ccr &= ~(1 << 16);
	hal_cpuDataSyncBarrier();

	ccsidr = scs_common.scs->ccsidr;

	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			scs_common.scs->dcisw = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
		} while (ways-- != 0);
	} while (sets-- != 0);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scbCleanInvalDCacheAddr(void *addr, u32 sz)
{
	u32 daddr;
	int dsize;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if (sz == 0u) {
		return;
	}

	daddr = (((u32)addr) & ~0x1fu);
	dsize = sz + ((u32)addr & 0x1fu);

	hal_cpuDataSyncBarrier();

	do {
		scs_common.scs->dccimvac = daddr;
		daddr += 0x20u;
		dsize -= 0x20;
	} while (dsize > 0);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scbEnableICache(void)
{
	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if ((scs_common.scs->ccr & (1 << 17)) == 0) {
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scs_common.scs->iciallu = 0; /* Invalidate I$ */
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scs_common.scs->ccr |= 1 << 17;
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}
}


void _hal_scbDisableICache(void)
{
	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	scs_common.scs->ccr &= ~(1 << 17);
	scs_common.scs->iciallu = 0;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scbSetDeepSleep(int state)
{
	if (state != 0) {
		scs_common.scs->scr |= 1 << 2;
		scs_common.scs->csr &= ~1;
	}
	else {
		scs_common.scs->scr &= ~(1 << 2);
		scs_common.scs->csr |= 1;
	}
}


void _hal_scbSystickInit(u32 load)
{
	scs_common.scs->rvr = load;
	scs_common.scs->cvr = 0;

	/* Enable systick */
	scs_common.scs->csr |= 0x7;
}


void _hal_scsInit(void)
{
	scs_common.scs = (void *)0xe000e000;

	/* Enable UsageFault, BusFault and MemManage exceptions */
	scs_common.scs->shcsr |= (1u << 16) | (1u << 17) | (1u << 18);
}
