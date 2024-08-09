/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System Control Block
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Gerard Swiderski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <arch/cpu.h>
#include "scb.h"


struct scb_s {
	u32 _res0[2];
	volatile u32 actlr;
	u32 _res1;
	volatile u32 csr;
	volatile u32 rvr;
	volatile u32 cvr;
	volatile u32 calib;
	u32 _res2[824];
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
	u32 _res3;
	volatile u32 mmfar;
	volatile u32 bfar;
	volatile u32 afsr;
	u32 _res4[14];
	volatile u32 clidr;
	volatile u32 ctr;
	volatile u32 ccsidr;
	volatile u32 csselr;
	volatile u32 cpacr;
	u32 _res5[106];
	volatile u32 fpccr;
	volatile u32 fpcar;
	volatile u32 fpdscr;
	u32 _res6[4];
	volatile u32 iciallu;
	u32 _res7;
	volatile u32 icimvau;
	volatile u32 scimvac;
	volatile u32 dcisw;
	volatile u32 dccmvau;
	volatile u32 dccvac;
	volatile u32 dccsw;
	volatile u32 dccimvac;
	volatile u32 dccisw;
	u32 _res8[6];
	volatile u32 itcmcr;
	volatile u32 dtcmcr;
	volatile u32 ahbpcr;
	volatile u32 cacr;
	volatile u32 ahbscr;
	u32 _res9;
	volatile u32 abfsr;
};


static struct {
	struct scb_s *scb;
} scb_common;


void _hal_scbSetPriorityGrouping(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = scb_common.scb->aircr & ~0xffff0700;

	/* Set AIRCR.PRIGROUP to 3: 16 priority groups and 16 subgroups
	   The value is same as for armv7m4-stm32l4x6 target
	   Setting various priorities is not supported on Phoenix-RTOS, so it's just default value */
	scb_common.scb->aircr = t | 0x5fa0000 | ((group & 7) << 8);
}


void _hal_scbSetPriority(s8 excpn, u32 priority)
{
	volatile u8 *ptr = (u8 *)&scb_common.scb->shpr1 + excpn - 4;

	/* We set only group priority field */
	*ptr = (priority << 4) & 0xff;
}


void _hal_scbSystemReset(void)
{
	scb_common.scb->aircr = ((0x5fau << 16) | (scb_common.scb->aircr & (0x700u)) | (1u << 2));

	hal_cpuDataSyncBarrier();

	for (;;) {
		hal_cpuHalt();
	}
}


unsigned int _hal_scbCpuid(void)
{
	return scb_common.scb->cpuid;
}


void _hal_scbSetFPU(int state)
{
	if (state != 0) {
		scb_common.scb->cpacr |= 0xf << 20;
	}
	else {
		scb_common.scb->cpacr = 0;
		scb_common.scb->fpccr = 0;
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

	if ((scb_common.scb->ccr & (1 << 16)) == 0) {
		scb_common.scb->csselr = 0;
		hal_cpuDataSyncBarrier();

		ccsidr = scb_common.scb->ccsidr;

		/* Invalidate D$ */
		sets = (ccsidr >> 13) & 0x7fff;
		do {
			ways = (ccsidr >> 3) & 0x3ff;
			do {
				scb_common.scb->dcisw = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
			} while (ways-- != 0);
		} while (sets-- != 0);
		hal_cpuDataSyncBarrier();

		scb_common.scb->ccr |= 1 << 16;

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

	scb_common.scb->csselr = 0;
	hal_cpuDataSyncBarrier();

	scb_common.scb->ccr &= ~(1 << 16);
	hal_cpuDataSyncBarrier();

	ccsidr = scb_common.scb->ccsidr;

	sets = (ccsidr >> 13) & 0x7fff;
	do {
		ways = (ccsidr >> 3) & 0x3ff;
		do {
			scb_common.scb->dcisw = ((sets & 0x1ff) << 5) | ((ways & 0x3) << 30);
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
		scb_common.scb->dccimvac = daddr;
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

	if ((scb_common.scb->ccr & (1 << 17)) == 0) {
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scb_common.scb->iciallu = 0; /* Invalidate I$ */
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		scb_common.scb->ccr |= 1 << 17;
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
	scb_common.scb->ccr &= ~(1 << 17);
	scb_common.scb->iciallu = 0;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _hal_scbSetDeepSleep(int state)
{
	if (state != 0) {
		scb_common.scb->scr |= 1 << 2;
		scb_common.scb->csr &= ~1;
	}
	else {
		scb_common.scb->scr &= ~(1 << 2);
		scb_common.scb->csr |= 1;
	}
}


void _hal_scbSystickInit(u32 load)
{
	scb_common.scb->rvr = load;
	scb_common.scb->cvr = 0;

	/* Enable systick */
	scb_common.scb->csr |= 0x7;
}


void _hal_scbInit(void)
{
	scb_common.scb = (void *)0xe000e000;

	/* Enable UsageFault, BusFault and MemManage exceptions */
	scb_common.scb->shcsr |= (1u << 16) | (1u << 17) | (1u << 18);
}
