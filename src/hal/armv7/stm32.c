/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32 basic peripherals control functions
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "stm32.h"
#include "interrupts.h"
#include "pmap.h"
#include "../../../include/errno.h"


struct {
	volatile u32 *rcc;
	volatile u32 *gpio[8];
	volatile u32 *pwr;
	volatile u32 *scb;
	volatile u32 *rtc;
	volatile u32 *nvic;
	volatile u32 *exti;
	volatile u32 *stk;
	volatile u32 *mpu;
	volatile u32 *syscfg;
	volatile u32 *iwdg;

	u32 cpuclk;

	int hsi;
	int msi;

	u32 gpio_state[8];
	u32 uart_state[5];

	u32 resetFlags;

	spinlock_t pltctlSp;
} stm32_common;


enum { rcc_cr = 0, rcc_icscr, rcc_cfgr, rcc_cir, rcc_ahbrstr, rcc_apb2rstr, rcc_apb1rstr,
	rcc_ahbenr, rcc_apb2enr, rcc_apb1enr, rcc_ahblpenr, rcc_apb2lpenr, rcc_apb1lpenr, rcc_csr };


enum { gpio_moder = 0, gpio_otyper, gpio_ospeedr, gpio_pupdr, gpio_idr,
	gpio_odr, gpio_bsrr, gpio_lckr, gpio_afrl, gpio_afrh, gpio_brr };


enum { pwr_cr = 0, pwr_csr };


enum { rtc_tr = 0, rtc_dr, rtc_cr, rtc_isr, rtc_prer, rtc_wutr, rtc_calibr, rtc_alrmar,
	rtc_alrmbr, rtc_wpr, rtc_ssr, rtc_shiftr, rtc_tstr, rtc_tsdr, rtc_tsssr, rtc_calr,
	rtc_tafcr, rtc_alrmassr, rtc_alrmbssr, rtc_bkp0r, rtc_bkp31r };


enum { scb_cpuid = 0, scb_icsr, scb_vtor, scb_aircr, scb_scr, scb_ccr, scb_shp0, scb_shp1,
	scb_shp2, scb_shcsr, scb_cfsr, scb_hfsr, scb_dfsr, scb_mmfar, scb_bfar, scb_afsr, scb_pfr0,
	scb_pfr1, scb_dfr, scb_adr, scb_mmfr0, scb_mmfr1, scb_mmfr2, scb_mmf3, scb_isar0, scb_isar1,
	scb_isar2, scb_isar3, scb_isar4, /* skip reserved registers */ scb_cpacr = 35 };


enum { nvic_iser = 0, nvic_icer = 32, nvic_ispr = 64, nvic_icpr = 96, nvic_iabr = 128,
	nvic_ip = 192, nvic_stir = 896 };


enum { exti_imr = 0, exti_emr, exti_rtsr, exti_ftsr, exti_swier, exti_pr };


enum {stk_ctrl = 0, stk_load, stk_val, stk_calib };


enum { mpu_typer = 0, mpu_cr, mpu_rnr, mpu_rbar, mpu_rasr };


enum { syscfg_memrmp = 0, syscfg_pmc, syscfg_exticr };


enum { iwdg_kr = 0, iwdg_pr, iwdg_rlr, iwdg_sr };


/* platformctl syscall */


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;

	hal_spinlockSet(&stm32_common.pltctlSp);

	switch (data->type) {
	case pctl_devclk:
		if (data->action == pctl_set)
			ret = _stm32_rccSetDevClock(data->devclk.dev, data->devclk.state);
		else if (data->action == pctl_get)
			ret = _stm32_rccGetDevClock(data->devclk.dev, &data->devclk.state);

		break;
	case pctl_cpuclk:
		if (data->action == pctl_set) {
			ret = _stm32_rccSetCPUClock(data->cpuclk.hz);
			_stm32_systickInit(1000);
		}
		else if (data->action == pctl_get) {
			data->cpuclk.hz = _stm32_rccGetCPUClock();
			ret = EOK;
		}

		break;
	case pctl_reboot:
		if (data->action == pctl_set) {
			if (data->reboot.magic == PCTL_REBOOT_MAGIC)
				_stm32_nvicSystemReset();
		}
		else if (data->action == pctl_get) {
			data->reboot.reason = stm32_common.resetFlags;
		}
	}

	hal_spinlockClear(&stm32_common.pltctlSp);

	return ret;
}


void _stm32_platformInit(void)
{
	hal_spinlockCreate(&stm32_common.pltctlSp, "pltctl");
}


/* RCC (Reset and Clock Controller) */


int _stm32_rccSetHSI(u32 on)
{
	if (on) {
		if (!stm32_common.hsi) {
			/* Turn HSI on */
			*(stm32_common.rcc + rcc_cr) |= 1;
			hal_cpuDataBarrier();

			/* Wait for HSIRDY flag */
			while (!(*(stm32_common.rcc + rcc_cr) & 2));
		}

		++stm32_common.hsi;
	}
	else if (stm32_common.hsi) {
		--stm32_common.hsi;

		if (!stm32_common.hsi) {
			/* Disable HSI */
			*(stm32_common.rcc + rcc_cr) &= ~1;
			hal_cpuDataBarrier();

			/* Wait for HSIRDY flag to clear */
			while (*(stm32_common.rcc + rcc_cr) & 2);
		}
	}

	return EOK;
}


int _stm32_rccSetMSI(u32 on)
{
	if (on) {
		if (!stm32_common.msi) {
			/* Turn MSI on */
			*(stm32_common.rcc + rcc_cr) |= 0x100;
			hal_cpuDataBarrier();

			/* Wait for MSIRDY flag */
			while (!((*(stm32_common.rcc + rcc_cr)) & 0x200));
		}

		++stm32_common.msi;
	}
	else if (stm32_common.msi) {
		--stm32_common.msi;

		if (!stm32_common.msi) {
			/* Disable MSI */
			*(stm32_common.rcc + rcc_cr) &= ~0x100;
			hal_cpuDataBarrier();

			/* Wait for MSIRDY flag to clear */
			while ((*(stm32_common.rcc + rcc_cr)) & 0x200);
		}
	}

	return EOK;
}


int _stm32_rccSetDevClock(unsigned int d, u32 hz)
{
	u32 t;

	hz = !!hz;

	if (d <= ahb_end) {
		t = *(stm32_common.rcc + rcc_ahbenr) & ~(1 << d);
		*(stm32_common.rcc + rcc_ahbenr) = t | (hz << d);
	}
	else if (d <= apb2_end) {
		t = *(stm32_common.rcc + rcc_apb2enr) & ~(1 << (d - apb2_begin));
		*(stm32_common.rcc + rcc_apb2enr) = t | (hz << (d - apb2_begin));
	}
	else if (d <= apb1_end) {
		t = *(stm32_common.rcc + rcc_apb1enr) & ~(1 << (d - apb1_begin));
		*(stm32_common.rcc + rcc_apb1enr) = t | (hz << (d - apb1_begin));
	}
	else if (d == pctl_rtc) {
		t = *(stm32_common.rcc + rcc_csr) & ~(1 << 22);
		*(stm32_common.rcc + rcc_csr) = t | (hz << 22);
	}
	else if (d == pctl_msi) {
		_stm32_rccSetMSI(hz);
	}
	else if (d == pctl_hsi) {
		_stm32_rccSetHSI(hz);
	}
	else
		return -EINVAL;

	hal_cpuDataBarrier();

	return EOK;
}


int _stm32_rccGetDevClock(unsigned int d, u32 *hz)
{
	if (d <= ahb_end) {
		if (d == pctl_gpiof || d == pctl_gpiog)
			d += 1;
		else if (d == pctl_gpioh)
			d -= 2;

		*hz = !!(*(stm32_common.rcc + rcc_ahbenr) & (1 << d));
	}
	else if (d <= apb2_end) {
		*hz = !!(*(stm32_common.rcc + rcc_apb2enr) & (1 << (d - apb2_begin)));
	}
	else if (d <= apb1_end) {
		*hz = !!(*(stm32_common.rcc + rcc_apb1enr) & (1 << (d - apb1_begin)));
	}
	else if (d == pctl_rtc) {
		*hz = !!(*(stm32_common.rcc + rcc_csr) & (1 << 22));
	}
	else if (d == pctl_msi) {
		*hz = !!(*(stm32_common.rcc + rcc_cr) & 0x100);
	}
	else if (d == pctl_hsi) {
		*hz = !!(*(stm32_common.rcc + rcc_cr) & 1);
	}
	else {
		return -EINVAL;
	}

	return EOK;
}


int _stm32_rccSetCPUClock(u32 hz)
{
	u8 range = hal_cpuGetLastBit(hz) - 16;
	u32 t = *(stm32_common.rcc + rcc_icscr);

	if (range == 7 || range > 8) {
		return -EINVAL;
	}
	else if (range == 8) {
		_stm32_pwrSetCPUVolt(1);

		/* 16 MHz */
		*(stm32_common.rcc + rcc_icscr) = t | (0x1f << 8);
		hal_cpuDataBarrier();

		_stm32_rccSetHSI(1);

		/* System clock switch */
		*(stm32_common.rcc + rcc_cfgr) |= 1;

		/* Wait for switch to happen */
		while (!(*(stm32_common.rcc + rcc_cfgr) & (1 << 2)));

		_stm32_rccSetMSI(0);
	}
	else {
		/* Set new MSIRANGE */
		t = (t & 0xffff1fff) | (range << 13);
		*(stm32_common.rcc + rcc_icscr) = t;
		hal_cpuDataBarrier();

		_stm32_rccSetMSI(1);

		/* System clock switch */
		*(stm32_common.rcc + rcc_cfgr) &= ~1;

		/* Wait for switch to happen */
		while (*(stm32_common.rcc + rcc_cfgr) & (1 << 2));

		_stm32_rccSetHSI(0);
		_stm32_pwrSetCPUVolt(2);
	}

	stm32_common.cpuclk = 1 << (16 + range);
	return EOK;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


void _stm32_rccClearResetFlags(void)
{
	*(stm32_common.rcc + rcc_csr) |= 0x01000000;
}


int _stm32_rccIsIWDGResetFlag(void)
{
	u32 t = 0x5d & 0x1f;

	return ((*(stm32_common.rcc + rcc_csr) & (1 << t)) == 0);
}


/* PWR */


void _stm32_pwrSetCPUVolt(u8 range)
{
	u32 t;

	t = *(stm32_common.pwr + pwr_cr) & ~(3 << 11);
	*(stm32_common.pwr + pwr_cr) = t | ((range & 0x3) << 11);

	/* Wait for VOSF flag */
	while (*(stm32_common.pwr + pwr_csr) & (1 << 4));
}


void _stm32_pwrEnterLPRun(u32 state)
{
	/* Both bits has to be set and reset in the correct order! */
	if (state) {
		/* Set LPSDSR bit */
		*(stm32_common.pwr + pwr_cr) |= 1;

		/* Set LPRUN bit */
		*(stm32_common.pwr + pwr_cr) |= 1 << 14;
	}
	else {
		/* Reset LPRUN bit */
		*(stm32_common.pwr + pwr_cr) &= ~(1 << 14);

		/* Reset LPSDSR bit */
		*(stm32_common.pwr + pwr_cr) &= ~1;
	}
}


int _stm32_pwrEnterLPStop(void)
{
#ifdef NDEBUG
	u8 lprun_state = !!(*(stm32_common.pwr + pwr_cr) & (1 << 14));
	u8 regulator_state = (*(stm32_common.pwr + pwr_csr) >> 11) & 3;
	u32 cpuclk_state = (*(stm32_common.rcc + rcc_icscr) >> 13) & 7;
	int slept = 0, i;

	/* Disable gpios during sleep */
	for (i = 0; i < 8; ++i) {
		stm32_common.gpio_state[i] = *(stm32_common.gpio[i] + gpio_moder);
		*(stm32_common.gpio[i] + gpio_moder) = (unsigned int)(-1);
	}

	/* Disable uarts during sleep */
	stm32_common.uart_state[0] = *((volatile u32 *)0x4001380c);
	*((volatile u32 *)0x4001380c) = 0;
	stm32_common.uart_state[1] = *((volatile u32 *)0x4000440c);
	*((volatile u32 *)0x4000440c) = 0;
	stm32_common.uart_state[2] = *((volatile u32 *)0x4000480c);
	*((volatile u32 *)0x4000480c) = 0;
	stm32_common.uart_state[3] = *((volatile u32 *)0x40004c0c);
	*((volatile u32 *)0x40004c0c) = 0;
	stm32_common.uart_state[4] = *((volatile u32 *)0x4000500c);
	*((volatile u32 *)0x4000500c) = 0;

	/* Convert range to Hz */
	cpuclk_state = 1 << (16 + cpuclk_state);

	/* Set LPSDSR and ULP bits */
	*(stm32_common.pwr + pwr_cr) |= 1;
	*(stm32_common.pwr + pwr_cr) &= ~2;

	/* Set internal regulator to default range to further conserve power */
	_stm32_pwrSetCPUVolt(1);

	/* Set SLEEPDEEP bit of Cortex System Control Register */
	*(stm32_common.scb + scb_scr) |= 1 << 2;

	_stm32_rtcUnlockRegs();
	/* Set wakeup timer and interrupt bits */
	*(stm32_common.rtc + rtc_cr) |= (1 << 10) | (1 << 14);
	_stm32_rtcLockRegs();

	*(stm32_common.exti + exti_pr) |= 0xffffffff;

	/* Enter Stop mode */
	__asm__ volatile ("\
		dmb; \
		wfe; \
		nop; ");

	/* Find out if device actually woke up because of the alarm */
	slept = !!(*(stm32_common.pwr + pwr_csr) & 1);

	/* Reset SLEEPDEEP bit of Cortex System Control Register */
	*(stm32_common.scb + scb_scr) &= ~(1 << 2);

	/* Reset LPSDSR and ULP bits */
	*(stm32_common.pwr + pwr_cr) &= ~1;

	/* Clear standby and wakeup flags */
	*(stm32_common.pwr + pwr_cr) |= (3 << 2) | 1;

	/* Clear wakeup timer and interrupt bits */
	*(stm32_common.rtc + rtc_cr) &= ~((1 << 10) | (1 << 14));

	for (i = 0; i < 8; ++i)
		*(stm32_common.gpio[i] + gpio_moder) = stm32_common.gpio_state[i];

	*((volatile u32 *)0x4001380c) = stm32_common.uart_state[0];
	*((volatile u32 *)0x4000440c) = stm32_common.uart_state[1];
	*((volatile u32 *)0x4000480c) = stm32_common.uart_state[2];
	*((volatile u32 *)0x40004c0c) = stm32_common.uart_state[3];
	*((volatile u32 *)0x4000500c) = stm32_common.uart_state[4];

	/* Recover previous configuration */
	_stm32_pwrSetCPUVolt(regulator_state);
	_stm32_rccSetCPUClock(cpuclk_state);
	_stm32_pwrEnterLPRun(lprun_state);

	return slept;
#else
	return 0;
#endif
}


/* RTC */


void _stm32_rtcUnlockRegs(void)
{
	/* Set DBP bit */
	*(stm32_common.pwr + pwr_cr) |= 1 << 8;

	/* Unlock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000ca;
	*(stm32_common.rtc + rtc_wpr) = 0x00000053;
}


void _stm32_rtcLockRegs(void)
{
	/* Lock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000ff;

	/* Reset DBP bit */
	*(stm32_common.pwr + pwr_cr) &= ~(1 << 8);
}


void _stm32_rtcSetAlarm(u32 ms)
{
	_stm32_rtcUnlockRegs();

	/* Clear WUTF flag */
	*(stm32_common.rtc + rtc_isr) &= ~(1 << 10);

	/* Clear sleep status (CSBF and CWUF bits) */
	*(stm32_common.pwr + pwr_cr) |= (3 << 2) | 1;

	/* Clear wakeup timer and interrupt bits */
	*(stm32_common.rtc + rtc_cr) &= ~((1 << 10) | (1 << 14));

	/* Wait for WUTWF flag */
	while (!(*(stm32_common.rtc + rtc_isr) & (1 << 2)));

	/* Load wakeup timer register */
	*(stm32_common.rtc + rtc_wutr) = (ms << 1) & 0xffff;

	/* Select RTC/16 wakeup clock */
	*(stm32_common.rtc + rtc_cr) &= ~0x7;

	/* Unmask interrupt */
	_stm32_extiMaskEvent(20, 1);

	/* Set rising edge trigger */
	_stm32_extiSetTrigger(20, 1, 1);

	_stm32_rtcLockRegs();
}


u32 _stm32_rtcGetms(void)
{
	u32 ms = 255 - (*(stm32_common.rtc + rtc_ssr) & 0xffff);

	/* Perform fixed point mult to get 1/1000 of second */
	ms = (ms << 5) * 0x7d;	/* ms = ms * 3.90625 */
	return ms >> 10;
}


/* SCB */


void _stm32_scbSetPriorityGrouping(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = *(stm32_common.scb + scb_aircr) & ~0xffff0700;

	/* Store new value */
	*(stm32_common.scb + scb_aircr) = t | 0x5fa0000 | ((group & 7) << 8);
}


u32 _stm32_scbGetPriorityGrouping(void)
{
	return (*(stm32_common.scb + scb_aircr) & 0x700) >> 8;
}


void _stm32_scbSetPriority(s8 excpn, u32 priority)
{
	volatile u8 *ptr;

	ptr = &((u8*)(stm32_common.scb + scb_shp0))[excpn - 4];

	*ptr = (priority << 4) & 0x0ff;
}


u32 _stm32_scbGetPriority(s8 excpn)
{
	volatile u8 *ptr;

	ptr = &((u8*)(stm32_common.scb + scb_shp0))[excpn - 4];

	return *ptr >> 4;
}


/* NVIC (Nested Vectored Interrupt Controller */


void _stm32_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = stm32_common.nvic + ((u8)irqn >> 5) + (state ? nvic_iser: nvic_icer);
	*ptr |= 1 << (irqn & 0x1F);
}


u32 _stm32_nvicGetPendingIRQ(s8 irqn)
{
	volatile u32 *ptr = stm32_common.nvic + ((u8)irqn >> 5) + nvic_ispr;
	return !!(*ptr & (1 << (irqn & 0x1F)));
}


void _stm32_nvicSetPendingIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = stm32_common.nvic + ((u8)irqn >> 5) + (state ? nvic_ispr: nvic_icpr);
	*ptr |= 1 << (irqn & 0x1F);
}


u32 _stm32_nvicGetActive(s8 irqn)
{
	volatile u32 *ptr = stm32_common.nvic + ((u8)irqn >> 5) + nvic_iabr;
	return !!(*ptr & (1 << (irqn & 0x1F)));
}


void _stm32_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u8 *ptr;

	ptr = ((u8*)(stm32_common.nvic + nvic_ip)) + irqn;

	*ptr = (priority << 4) & 0x0ff;
}


u8 _stm32_nvicGetPriority(s8 irqn)
{
	volatile u8 *ptr;

	ptr = ((u8*)(stm32_common.nvic + nvic_ip)) + irqn;

	return *ptr >> 4;
}


void _stm32_nvicSystemReset(void)
{
	*(stm32_common.scb + scb_aircr) = ((0x5fa << 16) | (*(stm32_common.scb + scb_aircr) & (0x700)) | (1 << 0x02));

	__asm__ volatile ("dsb");

	for(;;);
}


/* EXTI */


int _stm32_extiMaskInterrupt(u32 line, u8 state)
{
	if (line > 23)
		return -EINVAL;

	*(stm32_common.exti + exti_imr) &= ~(!state << line);
	*(stm32_common.exti + exti_imr) |= !!state << line;

	return EOK;
}


int _stm32_extiMaskEvent(u32 line, u8 state)
{
	if (line > 23)
		return -EINVAL;

	*(stm32_common.exti + exti_emr) &= ~(!state << line);
	*(stm32_common.exti + exti_emr) |= !!state << line;

	return EOK;
}


int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge)
{
	volatile u32 *p;

	if (line > 23)
		return -EINVAL;

	if (edge)
		p = stm32_common.exti + exti_rtsr; /* rising edge trigger */
	else
		p = stm32_common.exti + exti_ftsr; /* falling edge trigger */

	*p &= ~(!state << line);
	*p |= !!state << line;

	return EOK;
}


int _stm32_syscfgExtiLineConfig(u8 port, u8 pin)
{
	u32 t;

	if (port > 7 || pin > 15)
		return -EINVAL;

	t = ((u32)0x0f << (0x04 * (pin & (u8)0x03)));
	(stm32_common.syscfg + syscfg_exticr)[pin >> 0x02] &= ~t;
	(stm32_common.syscfg + syscfg_exticr)[pin >> 0x02] |= (((u32)port) << (0x04 * (pin & (u8)0x03)));

	return EOK;
}


int _stm32_extiSoftInterrupt(u32 line)
{
	if (line > 23)
		return -EINVAL;

	*(stm32_common.exti + exti_swier) |= 1 << line;

	return EOK;
}


u32 _stm32_extiGetPending(void)
{
	return *(stm32_common.exti + exti_pr) & 0xffffff;
}


int _stm32_extiClearPending(u32 line)
{
	if (line > 23)
		return -EINVAL;

	*(stm32_common.exti + exti_pr) |= 1 << line;

	return EOK;
}


/* SysTick */


int _stm32_systickInit(u32 interval)
{
	u64 load = ((u64) interval * stm32_common.cpuclk) / 1000000;
	if (load > 0x00ffffff)
		return -EINVAL;

	*(stm32_common.stk + stk_load) = (u32) load;
	*(stm32_common.stk + stk_ctrl) = 0x7;

	return EOK;
}


void _stm32_systickSet(u8 state)
{
	*(stm32_common.stk + stk_ctrl) &= ~(!state);
	*(stm32_common.stk + stk_ctrl) |= !!state;
}


u32 _stm32_systickGet(void)
{
	u32 cb;

	cb = ((*(stm32_common.stk + stk_load) - *(stm32_common.stk + stk_val)) * 1000) / *(stm32_common.stk + stk_load);

	/* Add 1000 us if there's systick pending */
	if (*(stm32_common.scb + scb_icsr) & (1 << 26))
		cb += 1000;

	return cb;
}


/* MPU */


void _stm32_mpuReadRegion(u8 region, mpur_t *reg)
{
	u32 t;
	u32 ap;

	*(stm32_common.mpu + mpu_rnr) = region & 0x7;
	t = *(stm32_common.mpu + mpu_rasr);
	ap = ((t >> 24) & 0x7);

	reg->region = region;
	reg->base = *(stm32_common.mpu + mpu_rbar) & 0xffffffe0;
	reg->size = 1 << ((t >> 1) & 0x1f);
	reg->subregions = (t >> 8) & 0xff;
	reg->attr = (t & 1)? PGHD_PRESENT : 0;
	reg->attr |= ((t >> 28) & 1)? PGHD_EXEC : 0;

	if (ap == 3)
		reg->attr |= PGHD_USER | PGHD_WRITE;
	else if (ap == 2)
		reg->attr |= PGHD_USER;
}


void _stm32_mpuEnableRegion(u8 region, u8 state)
{
	u32 t;

	*(stm32_common.mpu + mpu_rnr) = region;
	t = *(stm32_common.mpu + mpu_rasr) & ~1;
	*(stm32_common.mpu + mpu_rasr) = t | !!state;
}


void _stm32_mpuUpdateRegion(mpur_t *reg)
{
	u32 t;
	u32 size = hal_cpuGetLastBit(reg->size);

	/* Turn off region */
	_stm32_mpuEnableRegion(reg->region, 0);

	*(stm32_common.mpu + mpu_rbar) = (reg->base & 0xffffffe0) | (1 << 4) | (reg->region & 0xf);

	t = *(stm32_common.mpu + mpu_rasr) & ~((1 << 28) | (0x7 << 24) | (0xff << 8) | 0x1f);
	t |= size << 1;
	t |= reg->subregions << 8;
	t |= (reg->attr & PGHD_EXEC)? (1 << 28) : 0;

	if (!(reg->attr & PGHD_USER))
		t |= 1 << 24;
	else if (!(reg->attr & PGHD_WRITE))
		t |= 2 << 24;
	else
		t |= 3 << 24;

	*(stm32_common.mpu + mpu_rasr) = t;

	/* Turn on region if present*/
	_stm32_mpuEnableRegion(reg->region, reg->attr & PGHD_PRESENT);
}


/* GPIO */


int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd)
{
	volatile u32 *base;
	u32 t;

	if (d > pctl_gpioh || pin > 15)
		return -EINVAL;

	base = stm32_common.gpio[d - pctl_gpioa];

	t = *(base + gpio_moder) & ~(0x3 << (pin << 1));
	*(base + gpio_moder) = t | (mode & 0x3) << (pin << 1);

	t = *(base + gpio_otyper) & ~(1 << pin);
	*(base + gpio_otyper) = t | (otype & 1) << pin;

	t = *(base + gpio_ospeedr) & ~(0x3 << (pin << 1));
	*(base + gpio_ospeedr) = t | (ospeed & 0x3) << (pin << 1);

	t = *(base + gpio_pupdr) & ~(0x03 << (pin << 1));
	*(base + gpio_pupdr) = t | (pupd & 0x3) << (pin << 1);

	if (pin < 8) {
		t = *(base + gpio_afrl) & ~(0xf << (pin << 2));
		*(base + gpio_afrl) = t | (af & 0xf) << (pin << 2);
	}
	else {
		t = *(base + gpio_afrh) & ~(0xf << ((pin - 8) << 2));
		*(base + gpio_afrh) = t | (af & 0xf) << ((pin - 8) << 2);
	}

	return EOK;
}


int _stm32_gpioSet(unsigned int d, u8 pin, u8 val)
{
	volatile u32 *base;
	u32 t;

	if (d > pctl_gpioh || pin > 15)
		return -EINVAL;

	base = stm32_common.gpio[d - pctl_gpioa];

	t = *(base + gpio_odr) & ~(!(u32)val << pin);
	*(base + gpio_odr) = t | !!(u32)val << pin;

	return EOK;
}


int _stm32_gpioSetPort(unsigned int d, u16 val)
{
	volatile u32 *base;

	if (d > pctl_gpioh)
		return -EINVAL;

	base = stm32_common.gpio[d - pctl_gpioa];
	*(base + gpio_odr) = val;

	return EOK;
}


int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val)
{
	volatile u32 *base;

	if (d > pctl_gpioh || pin > 15)
		return -EINVAL;

	base = stm32_common.gpio[d - pctl_gpioa];
	*val = !!(*(base + gpio_idr) & (1 << pin));

	return EOK;
}


int _stm32_gpioGetPort(unsigned int d, u16 *val)
{
	volatile u32 *base;

	if (d > pctl_gpioh)
		return -EINVAL;

	base = stm32_common.gpio[d - pctl_gpioa];
	*val = *(base + gpio_idr);

	return EOK;
}


/* Exception triggering */


void _stm32_invokePendSV(void)
{
	*(stm32_common.scb + scb_icsr) |= (1 << 28);
}


void _stm32_invokeSysTick(void)
{
	*(stm32_common.scb + scb_icsr) |= (1 << 26);
}


/* CPU info */


unsigned int _stm32_cpuid(void)
{
	return *(stm32_common.scb + scb_cpuid);
}


/* Watchdog */


void _stm32_wdgReload(void)
{
#if defined(WATCHDOG) && defined(NDEBUG)
	*(stm32_common.iwdg + iwdg_kr) = 0xaaaa;
#endif
}


void _stm32_init(void)
{
	u32 t, i;
	static const int gpio2pctl[] = { pctl_gpioa, pctl_gpiob, pctl_gpioc,
		pctl_gpiod, pctl_gpioe, pctl_gpiof, pctl_gpiog, pctl_gpioh };

	/* Base addresses init */
	stm32_common.rcc = (void *)0x40023800;
	stm32_common.pwr = (void *)0x40007000;
	stm32_common.scb = (void *)0xe000ed00;
	stm32_common.rtc = (void *)0x40002800;
	stm32_common.nvic = (void *)0xe000e100;
	stm32_common.exti = (void *)0x40010400;
	stm32_common.stk = (void *)0xe000e010;
	stm32_common.mpu = (void *)0xe000ed90;
	stm32_common.syscfg = (void *)0x40010000;
	stm32_common.iwdg = (void *)0x40003000;
	stm32_common.gpio[0] = (void *)0x40020000; /* GPIOA */
	stm32_common.gpio[1] = (void *)0x40020400; /* GPIOB */
	stm32_common.gpio[2] = (void *)0x40020800; /* GPIOC */
	stm32_common.gpio[3] = (void *)0x40020c00; /* GPIOD */
	stm32_common.gpio[4] = (void *)0x40021000; /* GPIOE */
	stm32_common.gpio[5] = (void *)0x40021800; /* GPIOF */
	stm32_common.gpio[6] = (void *)0x40021c00; /* GPIOG */
	stm32_common.gpio[7] = (void *)0x40021400; /* GPIOH */

	/* Init HSI & MSI refcount */
	stm32_common.hsi = 0;
	stm32_common.msi = 0;

	/* Store reset flags and then clean them */
	_stm32_rtcUnlockRegs();
	stm32_common.resetFlags = (*(stm32_common.rcc + rcc_csr) >> 26);
	*(stm32_common.rcc + rcc_csr) |= 1 << 24;
	_stm32_rtcLockRegs();

	/* Fundamental system init */
	_stm32_rccSetCPUClock(2 * 2097152); /* 4,2 MHz */

	/* Set buses divider to 1 */
	*(stm32_common.rcc + rcc_cfgr) = 0x8802c000;

	hal_cpuDataBarrier();

	/* Enable System configuration controller */
	_stm32_rccSetDevClock(pctl_syscfg, 1);

	/* Enable power module */
	_stm32_rccSetDevClock(pctl_pwr, 1);

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cir) = 0;

	hal_cpuDataBarrier();

	/* Rescue */
	_stm32_rccSetDevClock(pctl_gpiob, 1);
	_stm32_gpioConfig(pctl_gpiob, 8, 0, 0, 0, 0, 1);
	u8 val;
	_stm32_gpioGet(pctl_gpiob, 8, &val);

	while (!val)
		_stm32_gpioGet(pctl_gpiob, 8, &val);

	_stm32_rccSetDevClock(pctl_gpiob, 0);

	/* GPIO LP init */
#ifdef NDEBUG
	i = 0;
#else
	/* Don't change setting for debug pins (needed for JTAG) */
	/* Turn off for production to reduce power consumption */
	_stm32_rccSetDevClock(pctl_gpioa, 1);
	*(stm32_common.gpio[0] + gpio_moder) = 0xabffffff;
	_stm32_rccSetDevClock(pctl_gpioa, 0);
	_stm32_rccSetDevClock(pctl_gpiob, 1);
	*(stm32_common.gpio[1] + gpio_moder) = 0xfffffebf;
	_stm32_rccSetDevClock(pctl_gpiob, 0);

	/* Enable debug in stop mode */
	*((u32*)0xE0042004) |= 3;

	i = 2;
#endif

	/* Init all GPIOs to Ain mode to lower power consumption */
	for (; i <= pctl_gpiog - pctl_gpioa; ++i) {
		_stm32_rccSetDevClock(gpio2pctl[i], 1);
		*(stm32_common.gpio[i] + gpio_moder) = 0xffffffff;
		*(stm32_common.gpio[i] + gpio_pupdr) = 0;
		_stm32_rccSetDevClock(gpio2pctl[i], 0);
	}

	/* Set the internal regulator output voltage to 1.5V */
	_stm32_pwrSetCPUVolt(2);

	/* Set DBP bit */
	*(stm32_common.pwr + pwr_cr) |= 1 << 8;

	/* Enable LSE clock source */
	*(stm32_common.rcc + rcc_csr) |= 1 << 8;

	hal_cpuDataBarrier();

	/* And wait for it to turn on */
	while (!(*(stm32_common.rcc + rcc_csr) & (1 << 9)));

	/* Initialize RTC */

	/* Select LSE as clock source for RTC and LCD */
	*(stm32_common.rcc + rcc_csr) |= 1 << 16;

	hal_cpuDataBarrier();

	/* Clear DBP bit */
	*(stm32_common.pwr + pwr_cr) |= 1 << 8;

	/* Unlock RTC */
	_stm32_rtcUnlockRegs();

	/* Turn on RTC */
	_stm32_rccSetDevClock(pctl_rtc, 1);

	/* Set INIT bit */
	*(stm32_common.rtc + rtc_isr) |= 1 << 7;
	while (!(*(stm32_common.rtc + rtc_isr) & (1 << 6)));

	/* Set RTC prescaler (it has to be done this way) */
	t = *(stm32_common.rtc + rtc_prer) & ~(0x7f << 16);
	*(stm32_common.rtc + rtc_prer) = t | (0x7f << 16);
	t = *(stm32_common.rtc + rtc_prer) & ~0x7fff;
	*(stm32_common.rtc + rtc_prer) = t | 0xff;

	/* Reset RTC interrupt bits WUTIE & WUTE */
	*(stm32_common.rtc + rtc_cr) &= ~((1 << 14) | (1 << 10));

	/* Turn on shadow register bypass */
	*(stm32_common.rtc + rtc_cr) |= 1 << 5;

	/* Select RTC/16 wakeup clock */
	*(stm32_common.rtc + rtc_cr) &= ~0x7;

	/* Clear INIT bit */
	*(stm32_common.rtc + rtc_isr) &= ~(1 << 7);
	_stm32_rtcLockRegs();

	/* Clear pending interrupts */
	*(stm32_common.exti + exti_pr) |= 0xffffff;

#if defined(WATCHDOG) && defined(NDEBUG)
	/* Init watchdog */
	/* Enable write access to IWDG */
	*(stm32_common.iwdg + iwdg_kr) = 0x5555;

	/* Set prescaler to 256, ~30s interval */
	*(stm32_common.iwdg + iwdg_pr) = 0x06;
	*(stm32_common.iwdg + iwdg_rlr) = 0xfff;

	_stm32_wdgReload();

	/* Enable watchdog */
	*(stm32_common.iwdg + iwdg_kr) = 0xcccc;
#endif

	return;
}
