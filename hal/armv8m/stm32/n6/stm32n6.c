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
#include "include/errno.h"

#include "hal/arm/scs.h"

#include <board_config.h>

#if defined(WATCHDOG) && defined(WATCHDOG_TIMEOUT_MS)
#warning "This target doesn't support WATCHDOG_TIMEOUT_MS. Watchdog timeout is 31992 ms."
#endif


#ifndef USE_HSE_CLOCK_SOURCE
#define USE_HSE_CLOCK_SOURCE 1
#endif


#define GPIOA_BASE ((void *)0x56020000)
#define GPIOB_BASE ((void *)0x56020400)
#define GPIOC_BASE ((void *)0x56020800)
#define GPIOD_BASE ((void *)0x56020c00)
#define GPIOE_BASE ((void *)0x56021000)
#define GPIOF_BASE ((void *)0x56021400)
#define GPIOG_BASE ((void *)0x56021800)
#define GPIOH_BASE ((void *)0x56021c00)
#define GPION_BASE ((void *)0x56023400)
#define GPIOO_BASE ((void *)0x56023800)
#define GPIOP_BASE ((void *)0x56023c00)
#define GPIOQ_BASE ((void *)0x56024000)

#define IWDG_BASE   ((void *)0x56004800)
#define PWR_BASE    ((void *)0x56024800)
#define RCC_BASE    ((void *)0x56028000)
#define RTC_BASE    ((void *)0x56004000)
#define SYSCFG_BASE ((void *)0x56008000)
#define EXTI_BASE   ((void *)0x56025000)
#define RIFSC_BASE  ((void *)0x54024000)
#define GPDMA1_BASE ((void *)0x50021000)
#define HPDMA1_BASE ((void *)0x58020000)
#define DBGMCU_BASE ((void *)0x54001000)

#define EXTI_LINES   78
#define DMA_CHANNELS 16


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
	}

	hal_spinlockClear(&stm32_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&stm32_common.pltctlSp, "pltctl");
}


/* RIFSC (resource isolation framework security controller) */


int _stm32_rifsc_risup_change(unsigned int index, int secure, int privileged, int lock)
{
	u32 reg, shift;
	if (index >= pctl_risups_count) {
		return -EINVAL;
	}

	reg = index / 32;
	shift = index % 32;
	if (secure > 0) {
		*(stm32_common.rifsc + rifsc_risc_seccfgr0 + reg) |= (1 << shift);
	}
	else if (secure < 0) {
		*(stm32_common.rifsc + rifsc_risc_seccfgr0 + reg) &= ~(1 << shift);
	}

	if (privileged > 0) {
		*(stm32_common.rifsc + rifsc_risc_privcfgr0 + reg) |= (1 << shift);
	}
	else if (privileged < 0) {
		*(stm32_common.rifsc + rifsc_risc_privcfgr0 + reg) &= ~(1 << shift);
	}

	if (lock != 0) {
		*(stm32_common.rifsc + rifsc_risc_rcfglockr0 + reg) = (1 << shift);
	}

	return EOK;
}


int _stm32_rifsc_rimc_change(unsigned int index, int secure, int privileged, int cid)
{
	u32 tmp;
	if (index >= pctl_rimcs_count) {
		return -EINVAL;
	}

	if (secure > 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) |= (1 << 8);
	}
	else if (secure < 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) &= ~(1 << 8);
	}

	if (privileged > 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) |= (1 << 9);
	}
	else if (privileged < 0) {
		*(stm32_common.rifsc + rifsc_rimc_attr0 + index) &= ~(1 << 9);
	}

	if ((cid >= 0) && (cid < 0x7)) {
		tmp = *(stm32_common.rifsc + rifsc_rimc_attr0 + index);
		tmp &= ~(0x7 << 4);
		tmp |= (cid & 0x7) << 4;
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
		*(base + gpdma_seccfgr) |= (1 << channel);
	}
	else if (secure < 0) {
		*(base + gpdma_seccfgr) &= ~(1 << channel);
	}

	if (privileged > 0) {
		*(base + gpdma_privcfgr) |= (1 << channel);
	}
	else if (privileged < 0) {
		*(base + gpdma_privcfgr) &= ~(1 << channel);
	}

	if (lock != 0) {
		*(base + gpdma_rcfglockr) |= (1 << channel);
	}

	return EOK;
}


/* RCC (Reset and Clock Controller) */

static const struct {
	u16 reg_offs;
	u8 mask;
	u8 shift;
} ipclk_lookup[] = {
	[pctl_ipclk_adf1sel] = { rcc_ccipr1, 0x7, 0 },
	[pctl_ipclk_adc12sel] = { rcc_ccipr1, 0x7, 4 },
	[pctl_ipclk_adcpre] = { rcc_ccipr1, 0xff, 8 },
	[pctl_ipclk_dcmippsel] = { rcc_ccipr1, 0x3, 20 },
	[pctl_ipclk_eth1ptpsel] = { rcc_ccipr2, 0x3, 0 },
	[pctl_ipclk_eth1ptpdiv] = { rcc_ccipr2, 0xf, 4 },
	[pctl_ipclk_eth1pwrdownack] = { rcc_ccipr2, 0x1, 8 },
	[pctl_ipclk_eth1clksel] = { rcc_ccipr2, 0x3, 12 },
	[pctl_ipclk_eth1sel] = { rcc_ccipr2, 0x7, 16 },
	[pctl_ipclk_eth1refclksel] = { rcc_ccipr2, 0x1, 20 },
	[pctl_ipclk_eth1gtxclksel] = { rcc_ccipr2, 0x1, 24 },
	[pctl_ipclk_fdcansel] = { rcc_ccipr3, 0x3, 0 },
	[pctl_ipclk_fmcsel] = { rcc_ccipr3, 0x3, 4 },
	[pctl_ipclk_dftsel] = { rcc_ccipr3, 0x1, 8 },
	[pctl_ipclk_i2c1sel] = { rcc_ccipr4, 0x7, 0 },
	[pctl_ipclk_i2c2sel] = { rcc_ccipr4, 0x7, 4 },
	[pctl_ipclk_i2c3sel] = { rcc_ccipr4, 0x7, 8 },
	[pctl_ipclk_i2c4sel] = { rcc_ccipr4, 0x7, 12 },
	[pctl_ipclk_i3c1sel] = { rcc_ccipr4, 0x7, 16 },
	[pctl_ipclk_i3c2sel] = { rcc_ccipr4, 0x7, 20 },
	[pctl_ipclk_ltdcsel] = { rcc_ccipr4, 0x3, 24 },
	[pctl_ipclk_mco1sel] = { rcc_ccipr5, 0x7, 0 },
	[pctl_ipclk_mco1pre] = { rcc_ccipr5, 0xf, 4 },
	[pctl_ipclk_mco2sel] = { rcc_ccipr5, 0x7, 8 },
	[pctl_ipclk_mco2pre] = { rcc_ccipr5, 0xf, 12 },
	[pctl_ipclk_mdf1sel] = { rcc_ccipr5, 0x7, 16 },
	[pctl_ipclk_xspi1sel] = { rcc_ccipr6, 0x3, 0 },
	[pctl_ipclk_xspi2sel] = { rcc_ccipr6, 0x3, 4 },
	[pctl_ipclk_xspi3sel] = { rcc_ccipr6, 0x3, 8 },
	[pctl_ipclk_otgphy1sel] = { rcc_ccipr6, 0x3, 12 },
	[pctl_ipclk_otgphy1ckrefsel] = { rcc_ccipr6, 0x1, 16 },
	[pctl_ipclk_otgphy2sel] = { rcc_ccipr6, 0x3, 20 },
	[pctl_ipclk_otgphy2ckrefsel] = { rcc_ccipr6, 0x1, 24 },
	[pctl_ipclk_persel] = { rcc_ccipr7, 0x7, 0 },
	[pctl_ipclk_pssisel] = { rcc_ccipr7, 0x3, 4 },
	[pctl_ipclk_rtcsel] = { rcc_ccipr7, 0x3, 8 },
	[pctl_ipclk_rtcpre] = { rcc_ccipr7, 0x3f, 12 },
	[pctl_ipclk_sai1sel] = { rcc_ccipr7, 0x7, 20 },
	[pctl_ipclk_sai2sel] = { rcc_ccipr7, 0x7, 24 },
	[pctl_ipclk_sdmmc1sel] = { rcc_ccipr8, 0x3, 0 },
	[pctl_ipclk_sdmmc2sel] = { rcc_ccipr8, 0x3, 4 },
	[pctl_ipclk_spdifrx1sel] = { rcc_ccipr9, 0x7, 0 },
	[pctl_ipclk_spi1sel] = { rcc_ccipr9, 0x7, 4 },
	[pctl_ipclk_spi2sel] = { rcc_ccipr9, 0x7, 8 },
	[pctl_ipclk_spi3sel] = { rcc_ccipr9, 0x7, 12 },
	[pctl_ipclk_spi4sel] = { rcc_ccipr9, 0x7, 16 },
	[pctl_ipclk_spi5sel] = { rcc_ccipr9, 0x7, 20 },
	[pctl_ipclk_spi6sel] = { rcc_ccipr9, 0x7, 24 },
	[pctl_ipclk_lptim1sel] = { rcc_ccipr12, 0x7, 8 },
	[pctl_ipclk_lptim2sel] = { rcc_ccipr12, 0x7, 12 },
	[pctl_ipclk_lptim3sel] = { rcc_ccipr12, 0x7, 16 },
	[pctl_ipclk_lptim4sel] = { rcc_ccipr12, 0x7, 20 },
	[pctl_ipclk_lptim5sel] = { rcc_ccipr12, 0x7, 24 },
	[pctl_ipclk_usart1sel] = { rcc_ccipr13, 0x7, 0 },
	[pctl_ipclk_usart2sel] = { rcc_ccipr13, 0x7, 4 },
	[pctl_ipclk_usart3sel] = { rcc_ccipr13, 0x7, 8 },
	[pctl_ipclk_uart4sel] = { rcc_ccipr13, 0x7, 12 },
	[pctl_ipclk_uart5sel] = { rcc_ccipr13, 0x7, 16 },
	[pctl_ipclk_usart6sel] = { rcc_ccipr13, 0x7, 20 },
	[pctl_ipclk_uart7sel] = { rcc_ccipr13, 0x7, 24 },
	[pctl_ipclk_uart8sel] = { rcc_ccipr13, 0x7, 28 },
	[pctl_ipclk_uart9sel] = { rcc_ccipr14, 0x7, 0 },
	[pctl_ipclk_usart10sel] = { rcc_ccipr14, 0x7, 4 },
	[pctl_ipclk_lpuart1sel] = { rcc_ccipr14, 0x7, 8 },
};


int _stm32_rccSetIPClk(unsigned int ipclk, unsigned int setting)
{
	u32 v;
	if (ipclk >= pctl_ipclks_count) {
		return -EINVAL;
	}

	if ((setting & (~((u32)ipclk_lookup[ipclk].mask))) != 0) {
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
	if (ipclk >= pctl_ipclks_count) {
		return -EINVAL;
	}

	v = *(stm32_common.rcc + ipclk_lookup[ipclk].reg_offs);
	*setting_out = (v >> ipclk_lookup[ipclk].shift) & ipclk_lookup[ipclk].mask;
	return EOK;
}


static int _stm32_getDevClockRegShift(unsigned int dev, unsigned int *shift_out)
{
	unsigned int reg = dev / 32;
	if (reg > (rcc_apb5enr - rcc_busenr)) {
		return -EINVAL;
	}

	*shift_out = dev % 32;
	return reg;
}


int _stm32_rccSetDevClock(unsigned int dev, u32 status, u32 lpStatus)
{
	u32 shift;
	int reg, statusSC, lpStatusSC;

	reg = _stm32_getDevClockRegShift(dev, &shift);
	if (reg < 0) {
		return -EINVAL;
	}

	statusSC = (status == 0) ? rcc_busencr : rcc_busensr;
	*(stm32_common.rcc + reg + statusSC) = 1 << shift;

	lpStatusSC = (lpStatus == 0) ? rcc_buslpencr : rcc_buslpensr;
	*(stm32_common.rcc + reg + lpStatusSC) = 1 << shift;

	hal_cpuDataSyncBarrier();
	(void)*(stm32_common.rcc + reg + rcc_busenr);

	return EOK;
}


int _stm32_rccGetDevClock(unsigned int dev, u32 *status, u32 *lpStatus)
{
	u32 shift;
	int reg;

	reg = _stm32_getDevClockRegShift(dev, &shift);
	if (reg < 0) {
		return -EINVAL;
	}

	*status = (*(stm32_common.rcc + rcc_busenr + reg) >> shift) & 1;
	*lpStatus = (*(stm32_common.rcc + rcc_buslpencr + reg) >> shift) & 1;
	return EOK;
}


int _stm32_rccDevReset(unsigned int dev, u32 status)
{
	u32 reg = dev / 32, shift = dev % 32;
	int set_clear;

	if (reg > (rcc_apb5rstr - rcc_busrstr)) {
		return -EINVAL;
	}

	set_clear = (status == 0) ? rcc_busrstcr : rcc_busrstsr;
	*(stm32_common.rcc + reg + set_clear) = 1 << shift;
	hal_cpuDataSyncBarrier();
	(void)*(stm32_common.rcc + reg + rcc_busrstr);

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
	*(stm32_common.rcc + rcc_csr) |= 1 << 23;
}


/* DBGMCU */


int _stm32_dbgmcuStopTimerInDebug(unsigned int dev, u32 stop)
{
	u32 reg;
	volatile u32 *base = DBGMCU_BASE;
	if ((pctl_tim2 <= dev) && (dev <= pctl_tim11)) {
		reg = dbgmcu_apb1lfz1;
	}
	else if (((pctl_tim1 <= dev) && (dev <= pctl_tim8)) || ((pctl_tim18 <= dev) && (dev <= pctl_tim9))) {
		reg = dbgmcu_apb2fz1;
	}
	else if (((pctl_lptim2 <= dev) && (dev <= pctl_lptim5)) || (dev == pctl_rtc) || (dev == pctl_iwdg)) {
		reg = dbgmcu_apb4fz1;
	}
	else if (dev == pctl_gfxtim) {
		reg = dbgmcu_apb5fz1;
	}
	else {
		return -EINVAL;
	}

	if (stop) {
		*(base + reg) |= 1 << (dev % 32);
	}
	else {
		*(base + reg) &= ~(1 << (dev % 32));
	}

	hal_cpuDataSyncBarrier();
	return EOK;
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


/* EXTI */


static int _stm32_extiLineToRegBit(u32 line, u32 *reg_offs, u32 *bit)
{
	if (line >= EXTI_LINES) {
		return -1;
	}

	*reg_offs = (line / 32) * 8;
	*bit = (1 << line % 32);
	return 0;
}


int _stm32_extiMaskInterrupt(u32 line, u8 state)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	offs += exti_imr1;
	if (state != 0) {
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

	offs += exti_emr1;
	if (state != 0) {
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

	offs += (edge != 0) ? exti_rtsr1 : exti_ftsr1;
	if (state != 0) {
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


static volatile u32 *_stm32_gpioGetBase(unsigned int d)
{
	if ((d < pctl_gpioa) || (d > pctl_gpioq)) {
		return NULL;
	}

	return stm32_common.gpio[d - pctl_gpioa];
}


int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd)
{
	volatile u32 *base;
	u32 t;

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15)) {
		return -EINVAL;
	}

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

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15)) {
		return -EINVAL;
	}

	*(base + gpio_bsrr) = 1 << ((val == 0) ? (pin + 16) : pin);
	return EOK;
}


int _stm32_gpioSetPort(unsigned int d, u16 val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*(base + gpio_odr) = val;

	return EOK;
}


int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if ((base == NULL) || (pin > 15)) {
		return -EINVAL;
	}

	*val = (*(base + gpio_idr) >> pin) & 1;

	return EOK;
}


int _stm32_gpioGetPort(unsigned int d, u16 *val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*val = *(base + gpio_idr);

	return EOK;
}


int _stm32_gpioSetPrivilege(unsigned int d, u32 val)
{
	volatile u32 *base;

	base = _stm32_gpioGetBase(d);
	if (base == NULL) {
		return -EINVAL;
	}

	*(base + gpio_privcfgr) = val;

	return EOK;
}


int _stm32_gpioGetPrivilege(unsigned int d, u32 *val)
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
	*(stm32_common.iwdg + iwdg_kr) = 0xaaaa;
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
	_stm32_rccSetDevClock(pctl_syscfg, 1, 1);

	/* Enable power module */
	_stm32_rccSetDevClock(pctl_pwr, 1, 1);

	_stm32_rccSetDevClock(pctl_rifsc, 1, 1);
	_stm32_bsec_init();

	/* TODO: would be nice to have clock configuration options or the frequency passed from PLO */
	stm32_common.cpuclk = 600 * 1000 * 1000;
#if USE_HSE_CLOCK_SOURCE
	stm32_common.perclk = 48 * 1000 * 1000;
#else
	stm32_common.perclk = 64 * 1000 * 1000;
#endif

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cier) = 0;

	hal_cpuDataMemoryBarrier();

	/* GPIO init */
	for (i = 0; i < sizeof(gpioDevs) / sizeof(gpioDevs[0]); ++i) {
		_stm32_rccSetDevClock(gpioDevs[i], 1, 1);
	}

	_stm32_rccSetDevClock(pctl_risaf, 1, 1);
	_stm32_risaf_init();

	_stm32_rccSetDevClock(pctl_dbg, 1, 1);

#if defined(WATCHDOG)
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
}
