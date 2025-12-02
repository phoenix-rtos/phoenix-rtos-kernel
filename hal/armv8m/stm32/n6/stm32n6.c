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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
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


#define GPIOA_BASE ((void *)0x56020000U)
#define GPIOB_BASE ((void *)0x56020400U)
#define GPIOC_BASE ((void *)0x56020800U)
#define GPIOD_BASE ((void *)0x56020c00U)
#define GPIOE_BASE ((void *)0x56021000U)
#define GPIOF_BASE ((void *)0x56021400U)
#define GPIOG_BASE ((void *)0x56021800U)
#define GPIOH_BASE ((void *)0x56021c00U)
#define GPION_BASE ((void *)0x56023400U)
#define GPIOO_BASE ((void *)0x56023800U)
#define GPIOP_BASE ((void *)0x56023c00U)
#define GPIOQ_BASE ((void *)0x56024000U)

#define IWDG_BASE   ((void *)0x56004800U)
#define PWR_BASE    ((void *)0x56024800U)
#define RCC_BASE    ((void *)0x56028000U)
#define RTC_BASE    ((void *)0x56004000U)
#define SYSCFG_BASE ((void *)0x56008000U)
#define EXTI_BASE   ((void *)0x56025000U)
#define RIFSC_BASE  ((void *)0x54024000U)
#define GPDMA1_BASE ((void *)0x50021000U)
#define HPDMA1_BASE ((void *)0x58020000U)
#define DBGMCU_BASE ((void *)0x54001000U)

#define EXTI_LINES   78U
#define DMA_CHANNELS 16U


static struct {
	volatile u32 *rcc;
	volatile u32 *gpio[17];
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *exti;
	volatile u32 *syscfg;
	volatile u32 *iwdg;
	volatile u32 *rifsc;

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


/* EXTI */


static int _stm32_extiLineToRegBit(u32 line, u32 *reg_offs, u32 *bit)
{
	if (line >= EXTI_LINES) {
		return -1;
	}

	*reg_offs = (line / 32U) * 8U;
	*bit = (1UL << (line % 32U));
	return 0;
}


int _stm32_extiMaskInterrupt(u32 line, u8 state)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	offs += (u32)exti_imr1;
	if (state != 0U) {
		*(stm32_common.exti + offs) |= bit;
	}
	else {
		*(stm32_common.exti + offs) &= ~bit;
	}

	return EOK;
}


int _stm32_extiMaskEvent(u32 line, u8 state)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	offs += (u32)exti_emr1;
	if (state != 0U) {
		*(stm32_common.exti + offs) |= bit;
	}
	else {
		*(stm32_common.exti + offs) &= ~bit;
	}

	return EOK;
}


int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	offs += (u32)((edge != 0U) ? exti_rtsr1 : exti_ftsr1);
	if (state != 0U) {
		*(stm32_common.exti + offs) |= bit;
	}
	else {
		*(stm32_common.exti + offs) &= ~bit;
	}

	return EOK;
}


int _stm32_extiSoftInterrupt(u32 line)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	*(stm32_common.exti + exti_swier1 + offs) |= bit;
	return EOK;
}


/* GPIO */


static volatile u32 *_stm32_gpioGetBase(int d)
{
	if ((d < pctl_gpioa) || (d > pctl_gpioq)) {
		return NULL;
	}

	return stm32_common.gpio[d - pctl_gpioa];
}


int _stm32_gpioConfig(int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd)
{
	volatile u32 *base;
	u32 t;

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15U)) {
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


int _stm32_gpioSet(int d, u8 pin, u8 val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15U)) {
		return -EINVAL;
	}

	*(base + gpio_bsrr) = 1UL << ((val == 0U) ? ((u32)pin + 16U) : (u32)pin);
	return EOK;
}


int _stm32_gpioSetPort(int d, u16 val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*(base + gpio_odr) = (u32)val;

	return EOK;
}


int _stm32_gpioGet(int d, u8 pin, u8 *val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15U)) {
		return -EINVAL;
	}

	*val = (u8)(*(base + gpio_idr) >> pin) & 1U;

	return EOK;
}


int _stm32_gpioGetPort(int d, u32 *val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*val = *(base + gpio_idr);

	return EOK;
}


int _stm32_gpioSetPrivilege(int d, u32 val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*(base + gpio_privcfgr) = val;

	return EOK;
}


int _stm32_gpioGetPrivilege(int d, u32 *val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*val = *(base + gpio_privcfgr);

	return EOK;
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
	u32 i;
	static const int gpioDevs[] = {
		pctl_gpioa, pctl_gpiob, pctl_gpioc, pctl_gpiod,
		pctl_gpioe, pctl_gpiof, pctl_gpiog, pctl_gpioh,
		pctl_gpion, pctl_gpioo, pctl_gpiop, pctl_gpioq
	};

	/* Base addresses init */
	stm32_common.iwdg = IWDG_BASE;
	stm32_common.pwr = PWR_BASE;
	stm32_common.rcc = RCC_BASE;
	stm32_common.rtc = RTC_BASE;
	stm32_common.exti = EXTI_BASE;
	stm32_common.syscfg = SYSCFG_BASE;
	stm32_common.rifsc = RIFSC_BASE;
	stm32_common.gpio[0] = GPIOA_BASE;
	stm32_common.gpio[1] = GPIOB_BASE;
	stm32_common.gpio[2] = GPIOC_BASE;
	stm32_common.gpio[3] = GPIOD_BASE;
	stm32_common.gpio[4] = GPIOE_BASE;
	stm32_common.gpio[5] = GPIOF_BASE;
	stm32_common.gpio[6] = GPIOG_BASE;
	stm32_common.gpio[7] = GPIOH_BASE;
	stm32_common.gpio[8] = NULL;
	stm32_common.gpio[9] = NULL;
	stm32_common.gpio[10] = NULL;
	stm32_common.gpio[11] = NULL;
	stm32_common.gpio[12] = NULL;
	stm32_common.gpio[13] = GPION_BASE;
	stm32_common.gpio[14] = GPIOO_BASE;
	stm32_common.gpio[15] = GPIOP_BASE;
	stm32_common.gpio[16] = GPIOQ_BASE;

	_hal_scsInit();

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

	/* GPIO init */
	for (i = 0; i < sizeof(gpioDevs) / sizeof(gpioDevs[0]); ++i) {
		(void)_stm32_rccSetDevClock(gpioDevs[i], 1U, 1U);
	}

#if NPU
	/* Enable NPU clock */
	(void)_stm32_rccSetDevClock(pctl_npu, 1U, 1U);
#endif

#if NPU_CACHEAXI
#error "CACHE AXI not yet supported"
	(void)_stm32_rccSetDevClock(pctl_npucacheram, 1U, 1U);
	(void)_stm32_rccSetDevClock(pctl_npucache, 1U, 1U);
	*(stm32_common.rcc + rcc_ahb5rstsr) |= (1U << 30);
	*(stm32_common.rcc + rcc_ahb5rstcr) |= (1U << 30);
	*(stm32_common.rcc + 0x82df00) |= 1U;
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
