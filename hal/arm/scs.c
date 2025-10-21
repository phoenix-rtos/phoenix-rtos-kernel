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

#define CPUID_PARTNO_M7  0xc27UL
#define CPUID_PARTNO_M55 0xd22UL


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


void _hal_scsIRQSet(u8 irqn, u8 state)
{
	volatile u32 *ptr = (state != 0U) ? scs_common.scs->iser : scs_common.scs->icer;

	*(ptr + (irqn >> 5)) = 1UL << (irqn & 0x1fU);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


/* MISRA TODO: restrict priority to u8? */
void _hal_scsIRQPrioritySet(u8 irqn, u32 priority)
{
	volatile u8 *ptr = (volatile u8 *)scs_common.scs->ip;

	*(ptr + irqn) = ((u8)priority << 4) & 0xffU;

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scsIRQPendingSet(u8 irqn)
{
	volatile u32 *ptr = scs_common.scs->ispr;

	*(ptr + (irqn >> 5)) = 1UL << (irqn & 0x1fU);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


int _hal_scsIRQPendingGet(u8 irqn)
{
	volatile u32 *ptr = &scs_common.scs->ispr[irqn >> 5];
	return ((*ptr & (1UL << (irqn & 0x1fU))) != 0U) ? 1 : 0;
}


int _hal_scsIRQActiveGet(u8 irqn)
{
	volatile u32 *ptr = &scs_common.scs->iabr[irqn >> 5];
	return ((*ptr & (1UL << (irqn & 0x1fU))) != 0U) ? 1 : 0;
}


void _hal_scsPriorityGroupingSet(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = scs_common.scs->aircr & ~0xffff0700U;

	/* Set AIRCR.PRIGROUP to 3: 16 priority groups and 16 subgroups
	   The value is same as for armv7m4-stm32l4x6 target
	   Setting various priorities is not supported on Phoenix-RTOS, so it's just default value */
	scs_common.scs->aircr = t | 0x5fa0000U | ((group & 7U) << 8);
}


u32 _hal_scsPriorityGroupingGet(void)
{
	return (scs_common.scs->aircr & 0x700U) >> 8;
}


void _hal_scsExceptionPrioritySet(u32 excpn, u32 priority)
{
	volatile u8 *ptr = (u8 *)&scs_common.scs->shpr1 + excpn - 4U;

	/* We set only group priority field */
	*ptr = ((u8)priority << 4) & 0xffU;
}


u32 _imxrt_scsExceptionPriorityGet(u32 excpn)
{
	volatile u8 *ptr = (u8 *)&scs_common.scs->shpr1 + excpn - 4U;

	return (u32)*ptr >> 4U;
}


void _hal_scsSystemReset(void)
{
	scs_common.scs->aircr = ((0x5faUL << 16) | (scs_common.scs->aircr & (0x700U)) | (1U << 2));

	hal_cpuDataSyncBarrier();

	for (;;) {
		hal_cpuHalt();
	}
}


unsigned int _hal_scsCpuID(void)
{
	return scs_common.scs->cpuid;
}


void _hal_scsFPUSet(int state)
{
	if (state != 0) {
		scs_common.scs->cpacr |= 0xfUL << 20;
	}
	else {
		scs_common.scs->cpacr = 0;
		scs_common.scs->fpccr = 0;
	}
	hal_cpuDataSyncBarrier();
}


static int _hal_scbCacheIsSupported(void)
{
	u32 partno = ((_hal_scsCpuID() >> 4) & 0xfffU);

	/* Only supported on Cortex-M7 and Cortex-M55 for now */
	if ((partno == CPUID_PARTNO_M7) || (partno == CPUID_PARTNO_M55)) {
		return 1;
	}

	return 0;
}


void _hal_scsDCacheEnable(void)
{
	u32 ccsidr, sets, ways;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if ((scs_common.scs->ccr & (1UL << 16)) == 0U) {
		scs_common.scs->csselr = 0;
		hal_cpuDataSyncBarrier();

		ccsidr = scs_common.scs->ccsidr;

		/* Invalidate D$ */
		sets = (ccsidr >> 13) & 0x7fffU;
		do {
			ways = (ccsidr >> 3) & 0x3ffU;
			do {
				scs_common.scs->dcisw = ((sets & 0x1ffU) << 5) | ((ways & 0x3U) << 30);
			} while (ways-- != 0U);
		} while (sets-- != 0U);
		hal_cpuDataSyncBarrier();

		scs_common.scs->ccr |= 1UL << 16;

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}
}


void _hal_scsDCacheDisable(void)
{
	register u32 ccsidr, sets, ways;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	scs_common.scs->csselr = 0;
	hal_cpuDataSyncBarrier();

	scs_common.scs->ccr &= ~(1UL << 16);
	hal_cpuDataSyncBarrier();

	ccsidr = scs_common.scs->ccsidr;

	sets = (ccsidr >> 13) & 0x7fffU;
	do {
		ways = (ccsidr >> 3) & 0x3ffU;
		do {
			scs_common.scs->dcisw = ((sets & 0x1ffU) << 5) | ((ways & 0x3U) << 30);
		} while (ways-- != 0U);
	} while (sets-- != 0U);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


static void _hal_scsDCacheOpAddr(void *addr, u32 sz, volatile u32 *reg)
{
	u32 daddr;
	int dsize;

	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if (sz == 0U) {
		return;
	}

	daddr = (((u32)addr) & ~0x1fU);
	dsize = sz + ((u32)addr & 0x1fU);

	hal_cpuDataSyncBarrier();

	do {
		*reg = daddr;
		daddr += 0x20U;
		dsize -= 0x20;
	} while (dsize > 0);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scsDCacheCleanInvalAddr(void *addr, u32 sz)
{
	_hal_scsDCacheOpAddr(addr, sz, &scs_common.scs->dccimvac);
}


void _hal_scsDCacheCleanAddr(void *addr, u32 sz)
{
	_hal_scsDCacheOpAddr(addr, sz, &scs_common.scs->dccvac);
}


void _hal_scsDCacheInvalAddr(void *addr, u32 sz)
{
	_hal_scsDCacheOpAddr(addr, sz, &scs_common.scs->dcimvac);
}


void _hal_scsICacheEnable(void)
{
	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	if ((scs_common.scs->ccr & (1UL << 17)) == 0U) {
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scs_common.scs->iciallu = 0; /* Invalidate I$ */
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scs_common.scs->ccr |= 1UL << 17;
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}
}


void _hal_scsICacheDisable(void)
{
	if (_hal_scbCacheIsSupported() == 0) {
		return;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	scs_common.scs->ccr &= ~(1UL << 17);
	scs_common.scs->iciallu = 0;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scsDeepSleepSet(int state)
{
	if (state != 0) {
		scs_common.scs->scr |= 1U << 2;
		scs_common.scs->csr &= ~1U;
	}
	else {
		scs_common.scs->scr &= ~(1U << 2);
		scs_common.scs->csr |= 1U;
	}
}


void _hal_scsSystickInit(u32 load)
{
	scs_common.scs->rvr = load;
	scs_common.scs->cvr = 0;

	/* Enable systick */
	scs_common.scs->csr |= 0x7U;
}


u32 _hal_scsSystickGetCount(u8 *overflow_out)
{
	u8 overflow;
	u32 ret = scs_common.scs->cvr;
	if (overflow_out != NULL) {
		/* An overflow may occur between reading CVR and CSR. For this reason,
		 * if overflow flag is set, read the timer again to ensure we don't return
		 * a timestamp from the previous epoch.
		 */
		overflow = (u8)(scs_common.scs->csr >> 16) & 1U;
		if (overflow != 0U) {
			ret = scs_common.scs->cvr;
		}

		*overflow_out = overflow;
	}

	return ret;
}


u32 _hal_scsGetDefaultFPSCR(void)
{
	return scs_common.scs->fpdscr;
}


void _hal_scsInit(void)
{
	scs_common.scs = (void *)0xe000e000U;

	/* Enable UsageFault, BusFault and MemManage exceptions */
	scs_common.scs->shcsr |= (1UL << 16) | (1UL << 17) | (1UL << 18);
}
