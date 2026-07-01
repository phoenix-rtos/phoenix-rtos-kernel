/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32U3 basic peripherals control functions
 *
 * Copyright 2020, 2025 Phoenix Systems
 * Copyright 2026 Apator Metrix
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz, Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/u3/stm32u3_regs.h"
#include "hal/armv8m/stm32/stm32-timer.h"
#include "hal/armv8m/stm32/halsyspage.h"

#include "hal/cpu.h"
#include "hal/hal.h"
#include "include/errno.h"

#include "hal/arm/scs.h"

#include <board_config.h>

#if defined(WATCHDOG) && defined(WATCHDOG_TIMEOUT_MS)
#error "This target doesn't support WATCHDOG_TIMEOUT_MS. Watchdog timeout is 30 s."
#endif


#define GPIOA_BASE ((void *)0x52020000U)
#define GPIOB_BASE ((void *)0x52020400U)
#define GPIOC_BASE ((void *)0x52020800U)
#define GPIOD_BASE ((void *)0x52020c00U)
#define GPIOE_BASE ((void *)0x52021000U)
#define GPIOF_BASE ((void *)0x52021400U)
#define GPIOG_BASE ((void *)0x52021800U)
#define GPIOH_BASE ((void *)0x52021c00U)

#define IWDG_BASE   ((void *)0x50003000U)
#define PWR_BASE    ((void *)0x50030800U)
#define RCC_BASE    ((void *)0x50030c00U)
#define RTC_BASE    ((void *)0x50007800U)
#define SYSCFG_BASE ((void *)0x50040400U)
#define GPDMA1_BASE ((void *)0x50020000U)
#define DBGMCU_BASE ((void *)0xe0044000U)

#define DMA_CHANNELS 12U


static struct {
	volatile u32 *rcc;
	volatile u32 *gpio[8];
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *syscfg;
	volatile u32 *iwdg;

	u32 cpuclk;

	spinlock_t pltctlSp;
} stm32_common;


/* platformctl syscall */


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;
	u32 state, lpState;
	spinlock_ctx_t sc;

	hal_spinlockSet(&stm32_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_devclk:
			if (data->action == pctl_set) {
				ret = _stm32_rccSetDevClock(data->devclk.dev, data->devclk.state, data->devclk.lpState);
			}
			else if (data->action == pctl_get) {
				ret = _stm32_rccGetDevClock(data->devclk.dev, &state, &lpState);
				if (ret == EOK) {
					data->devclk.state = state;
					data->devclk.lpState = lpState;
				}
			}
			else {
				/* No action required */
			}

			break;
		case pctl_cpuclk:
			if (data->action == pctl_get) {
				data->cpuclk.hz = _stm32_rccGetCPUClock();
				ret = EOK;
			}

			break;
		case pctl_ipclk:
			if (data->action == pctl_set) {
				ret = _stm32_rccSetIPClk(data->ipclk.ipclk, data->ipclk.setting);
			}
			else if (data->action == pctl_get) {
				ret = _stm32_rccGetIPClk(data->ipclk.ipclk, &state);
				if (ret == EOK) {
					data->ipclk.setting = state;
				}
			}
			else {
				/* No action required */
			}

			break;
		case pctl_reboot:
			if (data->action == pctl_set) {
				if (data->reboot.magic == PCTL_REBOOT_MAGIC) {
					_hal_scsSystemReset();
				}
			}
			else if (data->action == pctl_get) {
				data->reboot.reason = syspage->hs.bootReason;
				ret = EOK;
			}
			else {
				/* No action required */
			}

			break;
		case pctl_dmaPermissions:
			if (data->action == pctl_set) {
				ret = _stm32_dmaSetPermissions(
						data->dmaPermissions.dev,
						data->dmaPermissions.channel,
						data->dmaPermissions.secure,
						data->dmaPermissions.privileged,
						data->dmaPermissions.lock);
			}
			break;
		case pctl_dmaLinkBaseAddr:
			if (data->action == pctl_set) {
				ret = _stm32_dmaSetLinkBaseAddr(
						data->dmaLinkBaseAddr.dev,
						data->dmaLinkBaseAddr.channel,
						data->dmaLinkBaseAddr.addr);
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}

	hal_spinlockClear(&stm32_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&stm32_common.pltctlSp, "pltctl");
}


/* DMA controller permissions */


int _stm32_dmaSetPermissions(int dev, unsigned int channel, int secure, int privileged, int lock)
{
	volatile u32 *base;
	if (dev == pctl_gpdma1) {
		base = GPDMA1_BASE;
	}
	else {
		return -EINVAL;
	}

	if (channel >= DMA_CHANNELS) {
		return -EINVAL;
	}

	if (secure > 0) {
		*(base + gpdma_seccfgr) |= (1UL << channel);
	}
	else if (secure < 0) {
		*(base + gpdma_seccfgr) &= ~(1UL << channel);
	}
	else {
		/* No action required */
	}

	if (privileged > 0) {
		*(base + gpdma_privcfgr) |= (1UL << channel);
	}
	else if (privileged < 0) {
		*(base + gpdma_privcfgr) &= ~(1UL << channel);
	}
	else {
		/* No action required */
	}

	if (lock != 0) {
		*(base + gpdma_rcfglockr) |= (1UL << channel);
	}

	return EOK;
}


int _stm32_dmaSetLinkBaseAddr(int dev, unsigned int channel, unsigned int addr)
{
	volatile u32 *base;
	if (dev == pctl_gpdma1) {
		base = GPDMA1_BASE;
	}
	else {
		return -EINVAL;
	}

	if (channel >= DMA_CHANNELS) {
		return -EINVAL;
	}

	*(base + (unsigned int)gpdma_cxlbar + (0x20U * channel)) = addr & 0xffff0000U;
	return EOK;
}


/* RCC (Reset and Clock Controller) */

static const struct {
	u16 reg_offs;
	u8 mask;
	u8 shift;
} ipclk_lookup[pctl_ipclks_count] = {
	[pctl_ipclk_usart1sel] = { (u16)rcc_ccipr1, 0x1U, 0U },
	[pctl_ipclk_usart3sel] = { (u16)rcc_ccipr1, 0x1U, 2U },
	[pctl_ipclk_uart4sel] = { (u16)rcc_ccipr1, 0x1U, 4U },
	[pctl_ipclk_uart5sel] = { (u16)rcc_ccipr1, 0x1U, 6U },
	[pctl_ipclk_i3c1sel] = { (u16)rcc_ccipr1, 0x1U, 8U },
	[pctl_ipclk_i2c1sel] = { (u16)rcc_ccipr1, 0x1U, 10U },
	[pctl_ipclk_i2c2sel] = { (u16)rcc_ccipr1, 0x1U, 12U },
	[pctl_ipclk_i3c2sel] = { (u16)rcc_ccipr1, 0x1U, 14U },
	[pctl_ipclk_spi2sel] = { (u16)rcc_ccipr1, 0x1U, 16U },
	[pctl_ipclk_lptim2sel] = { (u16)rcc_ccipr1, 0x3U, 18U },
	[pctl_ipclk_spi1sel] = { (u16)rcc_ccipr1, 0x1U, 20U },
	[pctl_ipclk_systicksel] = { (u16)rcc_ccipr1, 0x3U, 22U },
	[pctl_ipclk_fdcansel] = { (u16)rcc_ccipr1, 0x1U, 24U },
	[pctl_ipclk_iclksel] = { (u16)rcc_ccipr1, 0x3U, 26U },
	[pctl_ipclk_adf1sel] = { (u16)rcc_ccipr2, 0x3U, 0U },
	[pctl_ipclk_spi3sel] = { (u16)rcc_ccipr2, 0x1U, 3U },
	[pctl_ipclk_sai1sel] = { (u16)rcc_ccipr2, 0x3U, 5U },
	[pctl_ipclk_spi4sel] = { (u16)rcc_ccipr2, 0x1U, 7U },
	[pctl_ipclk_i2c4sel] = { (u16)rcc_ccipr2, 0x1U, 9U },
	[pctl_ipclk_rngsel] = { (u16)rcc_ccipr2, 0x1U, 11U },
	[pctl_ipclk_adcdacsel] = { (u16)rcc_ccipr2, 0x3U, 16U },
	[pctl_ipclk_dac1shsel] = { (u16)rcc_ccipr2, 0x1U, 19U },
	[pctl_ipclk_octospisel] = { (u16)rcc_ccipr2, 0x1U, 20U },
	[pctl_ipclk_usart2sel] = { (u16)rcc_ccipr2, 0x1U, 22U },
	[pctl_ipclk_lpuart1sel] = { (u16)rcc_ccipr3, 0x3U, 0U },
	[pctl_ipclk_i2c3sel] = { (u16)rcc_ccipr3, 0x1U, 6U },
	[pctl_ipclk_lptim34sel] = { (u16)rcc_ccipr3, 0x3U, 8U },
	[pctl_ipclk_lptim1sel] = { (u16)rcc_ccipr3, 0x3U, 10U },
};


int _stm32_rccSetIPClk(unsigned int ipclk, unsigned int setting)
{
	u32 v;
	if (ipclk >= (unsigned int)pctl_ipclks_count) {
		return -EINVAL;
	}

	if ((setting & (~((u32)ipclk_lookup[ipclk].mask))) != 0U) {
		return -EINVAL;
	}

	v = *(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs);
	v &= ~((u32)ipclk_lookup[ipclk].mask << ipclk_lookup[ipclk].shift);
	v |= (u32)setting << ipclk_lookup[ipclk].shift;
	*(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs) = v;
	return EOK;
}


int _stm32_rccGetIPClk(unsigned int ipclk, unsigned int *setting_out)
{
	u32 v;
	if (ipclk >= (unsigned int)pctl_ipclks_count) {
		return -EINVAL;
	}

	v = *(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs);
	*setting_out = (v >> ipclk_lookup[ipclk].shift) & ipclk_lookup[ipclk].mask;
	return EOK;
}


static int _stm32_getDevClockRegShift(int dev, unsigned int *shift_out)
{
	int reg = dev / 32;
	if ((dev < 0) || ((reg > (rcc_apb3enr - rcc_ahb1enr1)) && (dev != pctl_rtc))) {
		return -EINVAL;
	}

	*shift_out = (unsigned int)dev % 32U;
	return reg;
}


int _stm32_rccSetDevClock(int dev, u32 status, u32 lpStatus)
{
	u32 shift;
	int reg;

	reg = _stm32_getDevClockRegShift(dev, &shift);
	if (reg < 0) {
		return -EINVAL;
	}

	if (status != 0) {
		*(stm32_common.rcc + reg + rcc_ahb1enr1) |= (1UL << shift);
	}
	else {
		*(stm32_common.rcc + reg + rcc_ahb1enr1) &= ~(1UL << shift);
	}

	if (dev != pctl_rtc) {
		if (lpStatus != 0) {
			*(stm32_common.rcc + reg + rcc_ahb1slpenr1) |= (1UL << shift);
		}
		else {
			*(stm32_common.rcc + reg + rcc_ahb1slpenr1) &= ~(1UL << shift);
		}
	}

	hal_cpuDataSyncBarrier();
	return EOK;
}

int _stm32_rccGetDevClock(int dev, u32 *status, u32 *lpStatus)
{
	u32 shift;
	int reg;

	reg = _stm32_getDevClockRegShift(dev, &shift);
	if (reg < 0) {
		return -EINVAL;
	}

	*status = (*(stm32_common.rcc + reg + rcc_ahb1enr1) >> shift) & 1U;
	if (dev == pctl_rtc) {
		*lpStatus = *status;
	}
	else {
		*lpStatus = (*(stm32_common.rcc + reg + rcc_ahb1slpenr1) >> shift) & 1U;
	}
	return EOK;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


u32 _stm32_rccGetPerClock(void)
{
	/* Bootloader initializes HPRE to 1 and PPRE1 ~ 3 to 2 */
	return stm32_common.cpuclk / 2UL;
}


void _stm32_rccClearResetFlags(void)
{
	*(stm32_common.rcc + rcc_csr) |= (1UL << 23); /* RMVF */
}


/* DBGMCU */


int _stm32_dbgmcuStopTimerInDebug(int dev, u32 stop)
{
	u32 reg;
	volatile u32 *base = DBGMCU_BASE;
	if ((pctl_tim2 <= dev) && (dev <= pctl_rtcapb)) {
		reg = (u32)dbgmcu_apb1lfzr;
	}
	else if (dev == pctl_lptim2) {
		reg = (u32)dbgmcu_apb1hfzr;
	}
	else if ((pctl_tim1 <= dev) && (dev <= pctl_tim17)) {
		reg = (u32)dbgmcu_apb2fzr;
	}
	else if (((pctl_lptim1 <= dev) && (dev <= pctl_lptim4))) {
		reg = (u32)dbgmcu_apb3fzr;
	}
	else {
		return -EINVAL;
	}

	if (stop != 0U) {
		*(base + reg) |= 1UL << ((u32)dev % 32U);
	}
	else {
		*(base + reg) &= ~(1UL << ((u32)dev % 32U));
	}

	hal_cpuDataSyncBarrier();
	return EOK;
}


/* SysTick */


int _stm32_systickInit(u32 interval)
{
	u64 load = ((u64)interval * stm32_common.cpuclk) / 1000000U;
	if (load > 0x00ffffffU) {
		return -EINVAL;
	}

	_hal_scsSystickInit((u32)load);

	return EOK;
}


/* GPIO */


static volatile u32 *_stm32_gpioGetBase(int d, u8 pin)
{
	if ((d < pctl_gpioa) || (d > pctl_gpioh) || (pin > 15U)) {
		return NULL;
	}

	return stm32_common.gpio[d - pctl_gpioa];
}


int _stm32_gpioConfig(int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd)
{
	volatile u32 *base;
	u32 t;

	base = _stm32_gpioGetBase(d, pin);
	if (base == NULL) {
		return -EINVAL;
	}

	t = *(base + gpio_moder) & ~(0x3UL << (pin << 1));
	*(base + gpio_moder) = t | ((u32)mode & 0x3U) << (pin << 1);

	t = *(base + gpio_otyper) & ~(1UL << pin);
	*(base + gpio_otyper) = t | ((u32)otype & 1U) << pin;

	t = *(base + gpio_ospeedr) & ~(0x3UL << (pin << 1));
	*(base + gpio_ospeedr) = t | ((u32)ospeed & 0x3U) << (pin << 1);

	t = *(base + gpio_pupdr) & ~(0x03UL << (pin << 1));
	*(base + gpio_pupdr) = t | ((u32)pupd & 0x3U) << (pin << 1);

	if (pin < 8U) {
		t = *(base + gpio_afrl) & ~(0xfUL << (pin << 2));
		*(base + gpio_afrl) = t | ((u32)af & 0xfU) << (pin << 2);
	}
	else {
		t = *(base + gpio_afrh) & ~(0xfUL << ((pin - 8U) << 2));
		*(base + gpio_afrh) = t | ((u32)af & 0xfU) << ((pin - 8U) << 2);
	}

	return EOK;
}


/* Real time clock */


static void _stm32_rtcInit(void)
{
	u32 t = 0;

	/* Enable LSI clock */
	*(stm32_common.rcc + rcc_csr) |= (1U << 0); /* LSION */
	hal_cpuDataMemoryBarrier();
	while ((*(stm32_common.rcc + rcc_csr) & (1U << 1)) == 0) {
		/* Wait for LSIRDY */
	}

	/* Unlock backup domain register */
	*(stm32_common.pwr + pwr_dbpr) = (1U << 0); /* DBP */
	hal_cpuDataMemoryBarrier();
	while ((*(stm32_common.pwr + pwr_dbpr) & (1U << 0)) == 0) {
		/* Wait for DBP */
	}

	/* Configure RTC clock source */
	if (((*(stm32_common.rcc + rcc_bdcr) >> 8) & 0x3) != 2) {
		*(stm32_common.rcc + rcc_bdcr) |= (1U << 16); /* enter BDRST */
		hal_cpuDataMemoryBarrier();

		*(stm32_common.rcc + rcc_bdcr) &= ~(1U << 16); /* exit BDRST */
		hal_cpuDataMemoryBarrier();

		t = *(stm32_common.rcc + rcc_bdcr) & ~(0x3U << 8); /* mask RTCSEL */
		*(stm32_common.rcc + rcc_bdcr) = t | (2U << 8);    /* RTCSEL -> LSI */
		hal_cpuDataMemoryBarrier();
	}

	/* Enable RTC device */
	(void)_stm32_rccSetDevClock(pctl_rtcapb, 1, 1);
	(void)_stm32_rccSetDevClock(pctl_rtc, 1, 1);
	hal_cpuDataMemoryBarrier();

	/* Unlock RTC registers */
	*(stm32_common.rtc + rtc_wpr) = 0xcaU;
	*(stm32_common.rtc + rtc_wpr) = 0x53U;
	hal_cpuDataMemoryBarrier();

	/* Enter initialization mode */
	*(stm32_common.rtc + rtc_icsr) |= (1U << 7); /* INIT */
	hal_cpuDataMemoryBarrier();
	while ((*(stm32_common.rtc + rtc_icsr) & (1U << 6)) == 0U) {
		/* Wait for INITF */
	}

	/* Set RTC prescaler to 32'000 (LSI)
	 * From RM0487, 46.6.5: "The initialization must be performed in two separate write accesses."
	 */
	t = *(stm32_common.rtc + rtc_prer) & ~(0x7fUL << 16); /* PREDIV_A */
	*(stm32_common.rtc + rtc_prer) = t | ((128UL - 1UL) << 16);
	t = *(stm32_common.rtc + rtc_prer) & ~0x7fffUL; /* PREDIV_S */
	*(stm32_common.rtc + rtc_prer) = t | (250UL - 1UL);

	/* Reset RTC interrupt bits, select RTC/16 wakeup clock, and turn on shadow register bypass */
	t = *(stm32_common.rtc + rtc_cr) & ~((1UL << 14) | (1UL << 10) | 0x7UL); /* WUTIE | WUTE | WUCKSEL */
	t |= (1U << 5);                                                          /* BYPSHAD */
	*(stm32_common.rtc + rtc_cr) = t;

	/* Exit initialization mode */
	*(stm32_common.rtc + rtc_icsr) &= ~(1U << 7); /* INIT */
	hal_cpuDataMemoryBarrier();

	/* Lock RTC registers */
	*(stm32_common.rtc + rtc_wpr) = 0xffU;

	/* Reset DBP bit */
	*(stm32_common.pwr + pwr_dbpr) = (0U << 0); /* DBP */
}


/* Watchdog */


static void _stm32_wdgInit(void)
{
#if defined(WATCHDOG)
	/* Enable write access to IWDG */
	*(stm32_common.iwdg + iwdg_kr) = 0x5555U;

	/* 32 kHz independent clock */
	*(stm32_common.iwdg + iwdg_pr) = 6U; /* prescaler divider / 256 */
	*(stm32_common.iwdg + iwdg_rlr) = 30U /* s */ * 32000U /* Hz */ / 256U - 1U;

	_stm32_wdgReload();

	/* Enable watchdog */
	*(stm32_common.iwdg + iwdg_kr) = 0xccccU;
#endif
}


void _stm32_wdgReload(void)
{
#if defined(WATCHDOG)
	*(stm32_common.iwdg + iwdg_kr) = 0xaaaaU;
#endif
}


void _stm32_init(void)
{
	u32 i;

	/* Base addresses init */
	stm32_common.iwdg = IWDG_BASE;
	stm32_common.pwr = PWR_BASE;
	stm32_common.rcc = RCC_BASE;
	stm32_common.rtc = RTC_BASE;
	stm32_common.syscfg = SYSCFG_BASE;
	stm32_common.gpio[0] = GPIOA_BASE;
	stm32_common.gpio[1] = GPIOB_BASE;
	stm32_common.gpio[2] = GPIOC_BASE;
	stm32_common.gpio[3] = GPIOD_BASE;
	stm32_common.gpio[4] = GPIOE_BASE;
	stm32_common.gpio[5] = GPIOF_BASE;
	stm32_common.gpio[6] = GPIOG_BASE;
	stm32_common.gpio[7] = GPIOH_BASE;

	_hal_scsInit();

	/* Enable System configuration controller */
	(void)_stm32_rccSetDevClock(pctl_syscfg, 1U, 1U);
	(void)_stm32_rccSetDevClock(pctl_vref, 1U, 1U);

	/* Enable power module */
	(void)_stm32_rccSetDevClock(pctl_pwr, 1U, 1U);

	/* TODO: would be nice to have clock configuration options or the frequency passed from PLO */
	stm32_common.cpuclk = 24U * 1000U * 1000U;

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cier) = 0;

	hal_cpuDataMemoryBarrier();

	/* GPIO init */
	for (i = pctl_gpioa; i <= pctl_gpioh; i++) {
		(void)_stm32_rccSetDevClock(i, 1U, 1U);
	}

	_stm32_rtcInit();
	_stm32_wdgInit();
}
