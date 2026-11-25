/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32H5 basic peripherals control functions
 *
 * Copyright 2020, 2025, 2026 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/h5/stm32h5_regs.h"
#include "hal/armv8m/stm32/halsyspage.h"

#include "hal/cpu.h"
#include "hal/hal.h"
#include "include/errno.h"

#include "hal/arm/scs.h"

#include <board_config.h>

#define IWDG_BASE ((void *)0x50003000)
#define PWR_BASE  ((void *)0x54020800)
#define RCC_BASE  ((void *)0x54020c00)
#define RTC_BASE  ((void *)0x54007800)
#define ICB_BASE  ((void *)0xe000e000)

static struct {
	volatile u32 *rcc;
	volatile u32 *icb;
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *iwdg;

	u32 cpuclk;
	u32 perclk;

	spinlock_t pltctlSp;
} stm32_common;


/* Systick registers */
enum {
	icb_systick_csr = 4,
	icb_systick_rvr,
	icb_systick_cvr,
	icb_systick_calib,
};


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
			else {
				ret = -EINVAL;
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
		case pctl_cleanInvalDCache:
			if (data->action == pctl_set) {
				_hal_scsDCacheCleanInvalAddr(data->opDCache.addr, data->opDCache.sz);
				ret = EOK;
			}
			break;
		case pctl_cleanDCache:
			if (data->action == pctl_set) {
				_hal_scsDCacheCleanAddr(data->opDCache.addr, data->opDCache.sz);
				ret = EOK;
			}
			break;
		case pctl_invalDCache:
			if (data->action == pctl_set) {
				_hal_scsDCacheInvalAddr(data->opDCache.addr, data->opDCache.sz);
				ret = EOK;
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


/* RCC (Reset and Clock Controller) */


int _stm32_rccRegIdx(unsigned int dev)
{
	switch (dev / 32) {
		case 0:
			return 0;
		case 1:
			return rcc_ahb2enr - rcc_ahb1enr;
		case 2:
			return rcc_ahb4enr - rcc_ahb1enr;
		case 3:
			return rcc_apb1lenr - rcc_ahb1enr;
		case 4:
			return rcc_apb1henr - rcc_ahb1enr;
		case 5:
			return rcc_apb2enr - rcc_ahb1enr;
		case 6:
			return rcc_apb3enr - rcc_ahb1enr;
		default:
			return -1;
	}
}


int _stm32_rccSetDevClock(int dev, u32 status, u32 lpStatus)
{
	int idx = _stm32_rccRegIdx(dev);
	if (idx < 0) {
		return -1;
	}

	if (status != 0) {
		hal_cpuDataMemoryBarrier();
		*(stm32_common.rcc + rcc_ahb1enr + idx) |= (1UL << (dev % 32));
	}
	else {
		*(stm32_common.rcc + rcc_ahb1enr + idx) &= ~(1UL << (dev % 32));
		hal_cpuDataMemoryBarrier();
	}

	if (lpStatus != 0) {
		hal_cpuDataMemoryBarrier();
		*(stm32_common.rcc + rcc_ahb1lpenr + idx) |= (1UL << (dev % 32));
	}
	else {
		*(stm32_common.rcc + rcc_ahb1lpenr + idx) &= ~(1UL << (dev % 32));
		hal_cpuDataMemoryBarrier();
	}

	return 0;
}


int _stm32_rccGetDevClock(int dev, u32 *status, u32 *lpStatus)
{
	int idx = _stm32_rccRegIdx(dev);
	if (idx < 0) {
		return -1;
	}

	hal_cpuDataMemoryBarrier();
	*status = ((*(stm32_common.rcc + rcc_ahb1enr + idx) & (1UL << (dev % 32))) == 0) ? 0 : 1;
	*lpStatus = ((*(stm32_common.rcc + rcc_ahb1lpenr + idx) & (1UL << (dev % 32))) == 0) ? 0 : 1;

	return 0;
}


int _stm32_rccDevReset(int dev, u32 status)
{
	int idx = _stm32_rccRegIdx(dev);
	if (idx < 0) {
		return -1;
	}

	hal_cpuDataMemoryBarrier();
	*(stm32_common.rcc + rcc_ahb1rstr + idx) |= 1UL << (dev % 32UL);
	hal_cpuDataMemoryBarrier();
	*(stm32_common.rcc + rcc_ahb1rstr + idx) &= ~(1UL << (dev % 32UL));
	hal_cpuDataMemoryBarrier();

	return 0;
}


int _stm32_rccSetIPClk(unsigned int ipclk, unsigned int setting)
{
	static const struct {
		u16 reg_offs;
		u8 mask;
		u8 shift;
	} ipclk_lookup[] = {
		[pctl_ipclk_usart1sel] = { rcc_ccipr1, 0x7, 0 },
		[pctl_ipclk_usart2sel] = { rcc_ccipr1, 0x7, 3 },
		[pctl_ipclk_usart3sel] = { rcc_ccipr1, 0x7, 6 },
		[pctl_ipclk_uart4sel] = { rcc_ccipr1, 0x7, 9 },
		[pctl_ipclk_uart5sel] = { rcc_ccipr1, 0x7, 12 },
		[pctl_ipclk_usart6sel] = { rcc_ccipr1, 0x7, 15 },
		[pctl_ipclk_uart7sel] = { rcc_ccipr1, 0x7, 18 },
		[pctl_ipclk_uart8sel] = { rcc_ccipr1, 0x7, 21 },
		[pctl_ipclk_uart9sel] = { rcc_ccipr1, 0x7, 24 },
		[pctl_ipclk_usart10sel] = { rcc_ccipr1, 0x7, 27 },
		[pctl_ipclk_timicsel] = { rcc_ccipr1, 0x1, 31 },
		[pctl_ipclk_usart11sel] = { rcc_ccipr2, 0x7, 0 },
		[pctl_ipclk_uart12sel] = { rcc_ccipr2, 0x7, 4 },
		[pctl_ipclk_lptim1sel] = { rcc_ccipr2, 0x7, 8 },
		[pctl_ipclk_lptim2sel] = { rcc_ccipr2, 0x7, 12 },
		[pctl_ipclk_lptim3sel] = { rcc_ccipr2, 0x7, 16 },
		[pctl_ipclk_lptim4sel] = { rcc_ccipr2, 0x7, 20 },
		[pctl_ipclk_lptim5sel] = { rcc_ccipr2, 0x7, 24 },
		[pctl_ipclk_lptim6sel] = { rcc_ccipr2, 0x7, 28 },
		[pctl_ipclk_spi1sel] = { rcc_ccipr3, 0x7, 0 },
		[pctl_ipclk_spi2sel] = { rcc_ccipr3, 0x7, 3 },
		[pctl_ipclk_spi3sel] = { rcc_ccipr3, 0x7, 6 },
		[pctl_ipclk_spi4sel] = { rcc_ccipr3, 0x7, 9 },
		[pctl_ipclk_spi5sel] = { rcc_ccipr3, 0x7, 12 },
		[pctl_ipclk_spi6sel] = { rcc_ccipr3, 0x7, 15 },
		[pctl_ipclk_lpuart1sel] = { rcc_ccipr3, 0x7, 24 },
		[pctl_ipclk_octospi1sel] = { rcc_ccipr4, 0x3, 0 },
		[pctl_ipclk_systicksel] = { rcc_ccipr4, 0x3, 2 },
		[pctl_ipclk_usbsel] = { rcc_ccipr4, 0x3, 4 },
		[pctl_ipclk_sdmmc1sel] = { rcc_ccipr4, 0x1, 6 },
		[pctl_ipclk_sdmmc2sel] = { rcc_ccipr4, 0x1, 7 },
		[pctl_ipclk_i2c1sel] = { rcc_ccipr4, 0x3, 16 },
		[pctl_ipclk_i2c2sel] = { rcc_ccipr4, 0x3, 18 },
		[pctl_ipclk_i2c3sel] = { rcc_ccipr4, 0x3, 20 },
		[pctl_ipclk_i2c4sel] = { rcc_ccipr4, 0x3, 22 },
		[pctl_ipclk_i3c1sel] = { rcc_ccipr4, 0x3, 24 },
		[pctl_ipclk_i3c2sel] = { rcc_ccipr4, 0x3, 26 },
		[pctl_ipclk_adcdacsel] = { rcc_ccipr5, 0x7, 0 },
		[pctl_ipclk_dacsel] = { rcc_ccipr5, 0x1, 3 },
		[pctl_ipclk_rngsel] = { rcc_ccipr5, 0x3, 4 },
		[pctl_ipclk_cecsel] = { rcc_ccipr5, 0x3, 6 },
		[pctl_ipclk_fdcansel] = { rcc_ccipr5, 0x3, 8 },
		[pctl_ipclk_sai1sel] = { rcc_ccipr5, 0x7, 16 },
		[pctl_ipclk_sai2sel] = { rcc_ccipr5, 0x7, 19 },
		[pctl_ipclk_ckpersel] = { rcc_ccipr5, 0x3, 30 }
	};

	u32 v;
	if (ipclk >= (sizeof(ipclk_lookup) / sizeof(*ipclk_lookup))) {
		return -1;
	}

	hal_cpuDataMemoryBarrier();
	v = *(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs);
	v &= ~((u32)ipclk_lookup[ipclk].mask << ipclk_lookup[ipclk].shift);
	setting &= ipclk_lookup[ipclk].mask;
	v |= (u32)setting << ipclk_lookup[ipclk].shift;
	*(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs) = v;
	hal_cpuDataMemoryBarrier();

	return 0;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


u32 _stm32_rccGetPerClock(void)
{
	return stm32_common.perclk;
}


/* RTC */


void _stm32_rtcUnlockRegs(void)
{
	/* Set DBP bit */
	*(stm32_common.pwr + pwr_dbpcr) |= 1;

	/* Unlock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000ca;
	*(stm32_common.rtc + rtc_wpr) = 0x00000053;
	hal_cpuDataMemoryBarrier();
}


void _stm32_rtcLockRegs(void)
{
	hal_cpuDataMemoryBarrier();
	/* Lock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000ff;

	/* Reset DBP bit */
	*(stm32_common.pwr + pwr_dbpcr) &= ~1;
}


/* PWR */


void _stm32_pwrSetCPUVolt(u8 range)
{
	u32 t;
	u8 rangeCurr;

	hal_cpuDataMemoryBarrier();
	rangeCurr = (*(stm32_common.pwr + pwr_vossr) >> 14U) & 0x3;

	range &= 0x3U;

	/* Range has be adjusted gradually, we have to reach intermediate steps
	 *   on radical changes */
	while (range != rangeCurr) {
		if (range < rangeCurr) {
			--rangeCurr;
		}
		else {
			++rangeCurr;
		}

		t = *(stm32_common.pwr + pwr_voscr) & ~(0x3U << 4U);
		*(stm32_common.pwr + pwr_voscr) = t | (rangeCurr << 4U);
		hal_cpuDataMemoryBarrier();

		while ((*(stm32_common.pwr + pwr_vossr) & (1UL << 13U)) == 0) {
		}
	}
}


/* DMA controller permissions */


int _stm32_dmaSetLinkBaseAddr(int dev, unsigned int channel, unsigned int addr)
{
	volatile u32 *base;
	if (dev == pctl_gpdma1) {
		base = ((void *)0x50020000);
	}
	else if (dev == pctl_gpdma2) {
		base = ((void *)0x50021000);
	}
	else {
		return -EINVAL;
	}

	if (channel >= 8) {
		return -EINVAL;
	}

	*(base + (unsigned int)gpdma_cxlbar + (0x20U * channel)) = addr & 0xffff0000U;
	return EOK;
}


/* Watchdog */


void _stm32_wdgReload(void)
{
#if defined(WATCHDOG)
	*(stm32_common.iwdg + iwdg_kr) = 0xaaaa;
#endif
}


void _stm32_init(void)
{
	stm32_common.iwdg = IWDG_BASE;
	stm32_common.pwr = PWR_BASE;
	stm32_common.rcc = RCC_BASE;
	stm32_common.rtc = RTC_BASE;
	stm32_common.icb = ICB_BASE;

	_hal_scsInit();

	/* FIXME: Get from plo? Kernel do not configure/reconfigure that */
	stm32_common.cpuclk = 250UL * 1000000UL;
	stm32_common.perclk = 32UL * 1000000UL;
}
