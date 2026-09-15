/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32N6 basic peripherals control functions
 *
 * Copyright 2020, 2025 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/n6/stm32n6_regs.h"
#include "hal/armv8m/stm32/stm32-timer.h"
#include "hal/armv8m/stm32/halsyspage.h"

#include "hal/cpu.h"
#include "hal/hal.h"
#include "include/errno.h"

#include "hal/arm/scs.h"

#include <board_config.h>

#if defined(WATCHDOG) && defined(WATCHDOG_TIMEOUT_MS)
#error "This target doesn't support WATCHDOG_TIMEOUT_MS. Watchdog timeout is 31992 ms."
#endif


#ifndef USE_HSE_CLOCK_SOURCE
#define USE_HSE_CLOCK_SOURCE 1
#endif

#ifndef NPU
#define NPU 0
#endif

#ifndef NPU_CACHEAXI
#define NPU_CACHEAXI 0
#endif


#define IWDG_BASE     ((void *)0x56004800U)
#define PWR_BASE      ((void *)0x56024800U)
#define RCC_BASE      ((void *)0x56028000U)
#define RTC_BASE      ((void *)0x56004000U)
#define SYSCFG_BASE   ((void *)0x56008000U)
#define RIFSC_BASE    ((void *)0x54024000U)
#define GPDMA1_BASE   ((void *)0x50021000U)
#define HPDMA1_BASE   ((void *)0x58020000U)
#define DBGMCU_BASE   ((void *)0x54001000U)
#define CACHEAXI_BASE ((void *)0x580dfc00U)

#define DMA_CHANNELS 16U


static struct {
	volatile u32 *rcc;
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *syscfg;
	volatile u32 *iwdg;
	volatile u32 *rifsc;
	volatile u32 *cacheaxiconf;

	u32 cpuclk;
	u32 perclk;

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
		case pctl_gpioPrivilege:
			if (data->action == pctl_set) {
				ret = _stm32_gpioSetPrivilege(data->gpioPrivilege.port, data->gpioPrivilege.mask);
			}
			else if (data->action == pctl_get) {
				ret = _stm32_gpioGetPrivilege(data->gpioPrivilege.port, &state);
				if (ret == EOK) {
					data->gpioPrivilege.mask = state;
				}
			}
			else {
				/* No action required */
			}
			break;
		case pctl_risup:
			if (data->action == pctl_set) {
				ret = _stm32_rifsc_risup_change(data->risup.index, data->risup.secure, data->risup.privileged, data->risup.lock);
			}
			break;
		case pctl_rimc:
			if (data->action == pctl_set) {
				ret = _stm32_rifsc_rimc_change(data->rimc.index, data->rimc.secure, data->rimc.privileged, data->rimc.cid);
			}
			break;
		case pctl_otp:
			if (data->action == pctl_set) {
				ret = _stm32_bsec_otp_write(data->otp.addr, data->otp.val);
			}
			else if (data->action == pctl_get) {
				ret = _stm32_bsec_otp_read(data->otp.addr, &state);
				if (ret == EOK) {
					data->otp.val = state;
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
		case pctl_cleanInvalAXICache:
		case pctl_cleanAXICache:
		case pctl_invalAXICache:
			if (data->action == pctl_set) {
				ret = _stm32_AXICacheCmd(data->opAXICache.addr, data->opAXICache.sz, (int)data->type);
			}
			break;
		case pctl_enableAXICache:
			if (data->action == pctl_set) {
				ret = _stm32_setAXICacheEnable(data->opEnable.enable);
			}
			else if (data->action == pctl_get) {
				data->opEnable.enable = 0;
				ret = _stm32_getAXICacheEnable();
				if (ret >= 0) {
					data->opEnable.enable = (unsigned int)ret;
					ret = EOK;
				}
			}
			else {
				/* No action required */
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


/* RIFSC (resource isolation framework security controller) */


int _stm32_rifsc_risup_change(int index, int secure, int privileged, int lock)
{
	u32 reg, shift;
	if (index >= pctl_risups_count) {
		return -EINVAL;
	}

	reg = (u32)index / 32U;
	shift = (u32)index % 32U;
	if (secure > 0) {
		*(stm32_common.rifsc + rifsc_risc_seccfgr0 + reg) |= (1UL << shift);
	}
	else if (secure < 0) {
		*(stm32_common.rifsc + rifsc_risc_seccfgr0 + reg) &= ~(1UL << shift);
	}
	else {
		/* No action required */
	}

	if (privileged > 0) {
		*(stm32_common.rifsc + rifsc_risc_privcfgr0 + reg) |= (1UL << shift);
	}
	else if (privileged < 0) {
		*(stm32_common.rifsc + rifsc_risc_privcfgr0 + reg) &= ~(1UL << shift);
	}
	else {
		/* No action required */
	}

	if (lock != 0) {
		*(stm32_common.rifsc + rifsc_risc_rcfglockr0 + reg) = (1UL << shift);
	}

	return EOK;
}


int _stm32_rifsc_rimc_change(int index, int secure, int privileged, int cid)
{
	u32 tmp;
	if (index >= pctl_rimcs_count) {
		return -EINVAL;
	}

	if (secure > 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) |= (1UL << 8);
	}
	else if (secure < 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) &= ~(1UL << 8);
	}
	else {
		/* No action required */
	}

	if (privileged > 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) |= (1UL << 9);
	}
	else if (privileged < 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) &= ~(1UL << 9);
	}
	else {
		/* No action required */
	}

	if ((cid >= 0) && (cid < 0x7)) {
		tmp = *(stm32_common.rifsc + rifsc_rimc_attr0 + index);
		tmp &= ~(0x7U << 4);
		tmp |= ((u32)cid & 0x7U) << 4;
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) = tmp;
	}

	return EOK;
}


/* DMA controller permissions */


int _stm32_dmaSetPermissions(int dev, unsigned int channel, int secure, int privileged, int lock)
{
	volatile u32 *base;
	if (dev == pctl_gpdma1) {
		base = GPDMA1_BASE;
	}
	else if (dev == pctl_hpdma1) {
		base = HPDMA1_BASE;
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
	else if (dev == pctl_hpdma1) {
		base = HPDMA1_BASE;
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

/* AXI Cache clean, invalidate and enable */
int _stm32_AXICacheCmd(void *addr, unsigned int sz, int cmdtype)
{
	int ret;
	u32 cmdreg = 0;
	u32 addrVal = ((u32)addr) & 0xffffffc0U;

	ret = _stm32_getAXICacheEnable();
	if (ret == 0) {
		return -ENODEV;
	}
	else if (ret < 0) {
		return ret;
	}
	else {
		/* No action required */
	}

	if ((*(stm32_common.cacheaxiconf + cacheaxi_sr) & 1U) != 0U) {
		return -EBUSY;
	}

	*(stm32_common.cacheaxiconf + cacheaxi_fcr) |= 0x12U;
	*(stm32_common.cacheaxiconf + cacheaxi_cr2) &= ~6U;

	switch (cmdtype) {
		/* Cast of enum to int is necessary for enum inside typedef struct due to MISRAC2012-RULE_10_3-b */
		case (int)pctl_invalAXICache:
			*(stm32_common.cacheaxiconf + cacheaxi_cr1) |= 2U;
			break;
		case (int)pctl_cleanInvalAXICache:
		case (int)pctl_cleanAXICache:
			cmdreg = 4U;
			if (cmdtype == (int)pctl_cleanAXICache) {
				cmdreg = 6U;
			}

			*(stm32_common.cacheaxiconf + cacheaxi_cmdrsaddrr) |= addrVal;
			*(stm32_common.cacheaxiconf + cacheaxi_cmdreaddrr) |= (addrVal + sz - 1U) & 0xffffffc0U;
			*(stm32_common.cacheaxiconf + cacheaxi_cr2) |= (cmdreg & 6U);

			*(stm32_common.cacheaxiconf + cacheaxi_ier) &= ~5U;

			*(stm32_common.cacheaxiconf + cacheaxi_cr2) |= 1U;
			break;

		default:
			return -EINVAL;
	}

	time_t timeStart = hal_timerGetUs();
	time_t timeNow = timeStart;
	while ((*(stm32_common.cacheaxiconf + cacheaxi_sr) & 1U) != 0U) {
		timeNow = hal_timerGetUs();
		if ((timeNow - timeStart) > 400) {
			return -EBUSY;
		}
	}

	return EOK;
}

int _stm32_setAXICacheEnable(unsigned int enable)
{
	int ret;

	ret = _stm32_getAXICacheEnable();
	if (ret < 0) {
		return ret;
	}

	*(stm32_common.rcc + rcc_ahb5rstsr) |= (1UL << 30);
	*(stm32_common.rcc + rcc_ahb5rstcr) |= (1UL << 30);

	time_t timeStart = hal_timerGetUs();
	time_t timeNow = timeStart;
	while ((*(stm32_common.cacheaxiconf + cacheaxi_sr) & 1U) != 0U) {
		timeNow = hal_timerGetUs();
		if ((timeNow - timeStart) > 100) {
			return -EBUSY;
		}
	}

	if (enable != 0U) {
		*(stm32_common.cacheaxiconf + cacheaxi_cr1) |= 1U;
	}
	else {
		*(stm32_common.cacheaxiconf + cacheaxi_cr1) &= ~1U;
	}

	if ((*(stm32_common.cacheaxiconf + cacheaxi_cr1) & 1U) != ((enable & 1U))) {
		return -EPERM;
	}

	return EOK;
}

/* parasoft-begin-suppress MISRAC2012-RULE_14_3-ac "Check seemingly always evaluating to false left for safety and future-proofing" */
int _stm32_getAXICacheEnable(void)
{
	/* Check that clocks are enabled and cacheaxi_cr & 1 = 1 pctl_npucache */
	u32 status, lpStatus;
	int ret;

	ret = _stm32_rccGetDevClock(pctl_npucache, &status, &lpStatus);
	if (ret < 0) {
		return -EINVAL;
	}

	if (status == 0U) {
		return -ENODEV;
	}

	ret = _stm32_rccGetDevClock(pctl_npucacheram, &status, &lpStatus);
	if (ret < 0) {
		return -EINVAL;
	}

	if (status == 0U) {
		return -ENODEV;
	}


	ret = (int)(unsigned int)(*(stm32_common.cacheaxiconf + cacheaxi_cr1) & 1U);

	return ret;
}
/* parasoft-end-suppress MISRAC2012-RULE_14_3-ac */


/* RCC (Reset and Clock Controller) */

static const struct {
	u16 reg_offs;
	u8 mask;
	u8 shift;
} ipclk_lookup[64] = {
	[pctl_ipclk_adf1sel] = { (u16)rcc_ccipr1, 0x7U, 0U },
	[pctl_ipclk_adc12sel] = { (u16)rcc_ccipr1, 0x7U, 4U },
	[pctl_ipclk_adcpre] = { (u16)rcc_ccipr1, 0xffU, 8U },
	[pctl_ipclk_dcmippsel] = { (u16)rcc_ccipr1, 0x3U, 20U },
	[pctl_ipclk_eth1ptpsel] = { (u16)rcc_ccipr2, 0x3U, 0U },
	[pctl_ipclk_eth1ptpdiv] = { (u16)rcc_ccipr2, 0xfU, 4U },
	[pctl_ipclk_eth1pwrdownack] = { (u16)rcc_ccipr2, 0x1U, 8U },
	[pctl_ipclk_eth1clksel] = { (u16)rcc_ccipr2, 0x3U, 12U },
	[pctl_ipclk_eth1sel] = { (u16)rcc_ccipr2, 0x7U, 16U },
	[pctl_ipclk_eth1refclksel] = { (u16)rcc_ccipr2, 0x1U, 20U },
	[pctl_ipclk_eth1gtxclksel] = { (u16)rcc_ccipr2, 0x1U, 24U },
	[pctl_ipclk_fdcansel] = { (u16)rcc_ccipr3, 0x3U, 0U },
	[pctl_ipclk_fmcsel] = { (u16)rcc_ccipr3, 0x3U, 4U },
	[pctl_ipclk_dftsel] = { (u16)rcc_ccipr3, 0x1U, 8U },
	[pctl_ipclk_i2c1sel] = { (u16)rcc_ccipr4, 0x7U, 0U },
	[pctl_ipclk_i2c2sel] = { (u16)rcc_ccipr4, 0x7U, 4U },
	[pctl_ipclk_i2c3sel] = { (u16)rcc_ccipr4, 0x7U, 8U },
	[pctl_ipclk_i2c4sel] = { (u16)rcc_ccipr4, 0x7U, 12U },
	[pctl_ipclk_i3c1sel] = { (u16)rcc_ccipr4, 0x7U, 16U },
	[pctl_ipclk_i3c2sel] = { (u16)rcc_ccipr4, 0x7U, 20U },
	[pctl_ipclk_ltdcsel] = { (u16)rcc_ccipr4, 0x3U, 24U },
	[pctl_ipclk_mco1sel] = { (u16)rcc_ccipr5, 0x7U, 0U },
	[pctl_ipclk_mco1pre] = { (u16)rcc_ccipr5, 0xfU, 4U },
	[pctl_ipclk_mco2sel] = { (u16)rcc_ccipr5, 0x7U, 8U },
	[pctl_ipclk_mco2pre] = { (u16)rcc_ccipr5, 0xfU, 12U },
	[pctl_ipclk_mdf1sel] = { (u16)rcc_ccipr5, 0x7U, 16U },
	[pctl_ipclk_xspi1sel] = { (u16)rcc_ccipr6, 0x3U, 0U },
	[pctl_ipclk_xspi2sel] = { (u16)rcc_ccipr6, 0x3U, 4U },
	[pctl_ipclk_xspi3sel] = { (u16)rcc_ccipr6, 0x3U, 8U },
	[pctl_ipclk_otgphy1sel] = { (u16)rcc_ccipr6, 0x3U, 12U },
	[pctl_ipclk_otgphy1ckrefsel] = { (u16)rcc_ccipr6, 0x1U, 16U },
	[pctl_ipclk_otgphy2sel] = { (u16)rcc_ccipr6, 0x3U, 20U },
	[pctl_ipclk_otgphy2ckrefsel] = { (u16)rcc_ccipr6, 0x1U, 24U },
	[pctl_ipclk_persel] = { (u16)rcc_ccipr7, 0x7U, 0U },
	[pctl_ipclk_pssisel] = { (u16)rcc_ccipr7, 0x3U, 4U },
	[pctl_ipclk_rtcsel] = { (u16)rcc_ccipr7, 0x3U, 8U },
	[pctl_ipclk_rtcpre] = { (u16)rcc_ccipr7, 0x3fU, 12U },
	[pctl_ipclk_sai1sel] = { (u16)rcc_ccipr7, 0x7U, 20U },
	[pctl_ipclk_sai2sel] = { (u16)rcc_ccipr7, 0x7U, 24U },
	[pctl_ipclk_sdmmc1sel] = { (u16)rcc_ccipr8, 0x3U, 0U },
	[pctl_ipclk_sdmmc2sel] = { (u16)rcc_ccipr8, 0x3U, 4U },
	[pctl_ipclk_spdifrx1sel] = { (u16)rcc_ccipr9, 0x7U, 0U },
	[pctl_ipclk_spi1sel] = { (u16)rcc_ccipr9, 0x7U, 4U },
	[pctl_ipclk_spi2sel] = { (u16)rcc_ccipr9, 0x7U, 8U },
	[pctl_ipclk_spi3sel] = { (u16)rcc_ccipr9, 0x7U, 12U },
	[pctl_ipclk_spi4sel] = { (u16)rcc_ccipr9, 0x7U, 16U },
	[pctl_ipclk_spi5sel] = { (u16)rcc_ccipr9, 0x7U, 20U },
	[pctl_ipclk_spi6sel] = { (u16)rcc_ccipr9, 0x7U, 24U },
	[pctl_ipclk_lptim1sel] = { (u16)rcc_ccipr12, 0x7U, 8U },
	[pctl_ipclk_lptim2sel] = { (u16)rcc_ccipr12, 0x7U, 12U },
	[pctl_ipclk_lptim3sel] = { (u16)rcc_ccipr12, 0x7U, 16U },
	[pctl_ipclk_lptim4sel] = { (u16)rcc_ccipr12, 0x7U, 20U },
	[pctl_ipclk_lptim5sel] = { (u16)rcc_ccipr12, 0x7U, 24U },
	[pctl_ipclk_usart1sel] = { (u16)rcc_ccipr13, 0x7U, 0U },
	[pctl_ipclk_usart2sel] = { (u16)rcc_ccipr13, 0x7U, 4U },
	[pctl_ipclk_usart3sel] = { (u16)rcc_ccipr13, 0x7U, 8U },
	[pctl_ipclk_uart4sel] = { (u16)rcc_ccipr13, 0x7U, 12U },
	[pctl_ipclk_uart5sel] = { (u16)rcc_ccipr13, 0x7U, 16U },
	[pctl_ipclk_usart6sel] = { (u16)rcc_ccipr13, 0x7U, 20U },
	[pctl_ipclk_uart7sel] = { (u16)rcc_ccipr13, 0x7U, 24U },
	[pctl_ipclk_uart8sel] = { (u16)rcc_ccipr13, 0x7U, 28U },
	[pctl_ipclk_uart9sel] = { (u16)rcc_ccipr14, 0x7U, 0U },
	[pctl_ipclk_usart10sel] = { (u16)rcc_ccipr14, 0x7U, 4U },
	[pctl_ipclk_lpuart1sel] = { (u16)rcc_ccipr14, 0x7U, 8U },
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
	if (reg > (rcc_apb5enr - rcc_busenr)) {
		return -EINVAL;
	}

	*shift_out = (unsigned int)dev % 32U;
	return reg;
}


int _stm32_rccSetDevClock(int dev, u32 status, u32 lpStatus)
{
	u32 shift;
	int reg, statusSC, lpStatusSC;

	reg = _stm32_getDevClockRegShift(dev, &shift);
	if (reg < 0) {
		return -EINVAL;
	}

	statusSC = (status == 0U) ? rcc_busencr : rcc_busensr;
	*(stm32_common.rcc + reg + statusSC) = 1UL << shift;

	lpStatusSC = (lpStatus == 0U) ? rcc_buslpencr : rcc_buslpensr;
	*(stm32_common.rcc + reg + lpStatusSC) = 1UL << shift;

	hal_cpuDataSyncBarrier();
	(void)*(stm32_common.rcc + reg + rcc_busenr);

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

	*status = (*(stm32_common.rcc + rcc_busenr + reg) >> shift) & 1U;
	*lpStatus = (*(stm32_common.rcc + rcc_buslpencr + reg) >> shift) & 1U;
	return EOK;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


u32 _stm32_rccGetPerClock(void)
{
	return stm32_common.perclk;
}


void _stm32_rccClearResetFlags(void)
{
	*(stm32_common.rcc + rcc_csr) |= 1UL << 23;
}


/* DBGMCU */


int _stm32_dbgmcuStopTimerInDebug(int dev, u32 stop)
{
	u32 reg;
	volatile u32 *base = DBGMCU_BASE;
	if ((pctl_tim2 <= dev) && (dev <= pctl_tim11)) {
		reg = (u32)dbgmcu_apb1lfz1;
	}
	else if (((pctl_tim1 <= dev) && (dev <= pctl_tim8)) || ((pctl_tim18 <= dev) && (dev <= pctl_tim9))) {
		reg = (u32)dbgmcu_apb2fz1;
	}
	else if (((pctl_lptim2 <= dev) && (dev <= pctl_lptim5)) || (dev == pctl_rtc) || (dev == pctl_iwdg)) {
		reg = (u32)dbgmcu_apb4fz1;
	}
	else if (dev == pctl_gfxtim) {
		reg = (u32)dbgmcu_apb5fz1;
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


/* RTC */


void _stm32_rtcUnlockRegs(void)
{
	/* Set DBP bit */
	*(stm32_common.pwr + pwr_dbpcr) |= 1U;

	/* Unlock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000caU;
	*(stm32_common.rtc + rtc_wpr) = 0x00000053U;
	hal_cpuDataMemoryBarrier();
}


void _stm32_rtcLockRegs(void)
{
	hal_cpuDataMemoryBarrier();
	/* Lock RTC */
	*(stm32_common.rtc + rtc_wpr) = 0x000000ffU;

	/* Reset DBP bit */
	*(stm32_common.pwr + pwr_dbpcr) &= ~1U;
}


/* Watchdog */


void _stm32_wdgReload(void)
{
#if defined(WATCHDOG)
	*(stm32_common.iwdg + iwdg_kr) = 0xaaaaU;
#endif
}


void _stm32_init(void)
{
	/* Base addresses init */
	stm32_common.iwdg = IWDG_BASE;
	stm32_common.pwr = PWR_BASE;
	stm32_common.rcc = RCC_BASE;
	stm32_common.rtc = RTC_BASE;
	stm32_common.syscfg = SYSCFG_BASE;
	stm32_common.rifsc = RIFSC_BASE;
	stm32_common.cacheaxiconf = CACHEAXI_BASE;

	_hal_scsInit();

	_stm32_gpioInit();

	/* Enable System configuration controller */
	(void)_stm32_rccSetDevClock(pctl_syscfg, 1U, 1U);

	/* Enable power module */
	(void)_stm32_rccSetDevClock(pctl_pwr, 1U, 1U);

	(void)_stm32_rccSetDevClock(pctl_rifsc, 1U, 1U);
	_stm32_bsec_init();

	/* TODO: would be nice to have clock configuration options or the frequency passed from PLO */
	stm32_common.cpuclk = 600U * 1000U * 1000U;
#if USE_HSE_CLOCK_SOURCE
	stm32_common.perclk = 48U * 1000U * 1000U;
#else
	stm32_common.perclk = 64U * 1000U * 1000U;
#endif

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cier) = 0;

	hal_cpuDataMemoryBarrier();

#if NPU
	/* Enable NPU clock */
	(void)_stm32_rccSetDevClock(pctl_npu, 1U, 1U);
#endif

#if NPU_CACHEAXI
	(void)_stm32_rccSetDevClock(pctl_npucacheram, 1U, 1U);
	(void)_stm32_rccSetDevClock(pctl_npucache, 1U, 1U);
#endif

	(void)_stm32_rccSetDevClock(pctl_risaf, 1U, 1U);
	(void)_stm32_risaf_init();

	(void)_stm32_rccSetDevClock(pctl_dbg, 1U, 1U);

#if defined(WATCHDOG)
	/* Init watchdog */
	/* Enable write access to IWDG */
	*(stm32_common.iwdg + iwdg_kr) = 0x5555U;

	/* Set prescaler to 256, ~30s interval */
	*(stm32_common.iwdg + iwdg_pr) = 0x06U;
	*(stm32_common.iwdg + iwdg_rlr) = 0xfffU;

	_stm32_wdgReload();

	/* Enable watchdog */
	*(stm32_common.iwdg + iwdg_kr) = 0xccccU;
#endif
}
