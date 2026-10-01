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

#ifndef RCC_DEFAULT_CPU_CLOCK
#define RCC_DEFAULT_CPU_CLOCK (24U * 1000U * 1000U)
#endif


#define IWDG_BASE   ((void *)0x50003000U)
#define PWR_BASE    ((void *)0x50030800U)
#define RCC_BASE    ((void *)0x50030c00U)
#define RTC_BASE    ((void *)0x50007800U)
#define SYSCFG_BASE ((void *)0x50040400U)
#define GPDMA1_BASE ((void *)0x50020000U)
#define FLASH_BASE  ((void *)0x50022000U)
#define DBGMCU_BASE ((void *)0xe0044000U)

#define DMA_CHANNELS 12U

/*
 * After exiting sleep mode, the chip will always be in Range 2 with EPOD booster off.
 * If the requested frequency is higher than maximum in this configuration, additional steps need to be taken
 * when entering and exiting sleep mode.
 */
#define RCC_MAX_SLEEP_CLOCK (24U * 1000U * 1000U)

#define PWR_VOSR_BOOSTEN (1UL << 8)

#define RCC_CFGR1_SW_MSIS 0U /* System clock source - MSIS */
#define RCC_CFGR1_SW_HSI  1U /* System clock source - HSI16 */
#define RCC_CFGR1_SW_HSE  2U /* System clock source - HSE */

#define RCC_CR_HSION    (1UL << 11)
#define RCC_CR_HSIKERON (1UL << 12)
#define RCC_CR_HSIRDY   (1UL << 13)


static struct {
	volatile u32 *rcc;
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *syscfg;
	volatile u32 *iwdg;
	volatile u32 *dbgmcu;
	volatile u32 *flash;

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
			else if (data->action == pctl_set) {
				ret = _stm32_rccSetCPUClock(data->cpuclk.hz);
			}
			else {
				/* No action necessary */
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


/* FLASH */


static u8 _stm32_flashGetWaitStates(void)
{
	return *(stm32_common.flash + flash_acr) & 0xfU;
}


static void _stm32_flashSetWaitStates(u8 ws)
{
	u32 acr = *(stm32_common.flash + flash_acr) & ~0xfUL;
	ws &= 0xfU;
	acr |= ws;
	*(stm32_common.flash + flash_acr) = acr;
	while (_stm32_flashGetWaitStates() != ws) {
		/* Ensure the change has taken effect */
		hal_cpuDataSyncBarrier();
	}
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
	[pctl_ipclk_adcdacpre] = { (u16)rcc_ccipr2, 0xfU, 12U },
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


/* MSIS clock range mappings */
typedef struct {
	u32 hz;
	int val;
} range_mapping_t;

#define FREQUENCY_RANGE_COUNT 6U
static const range_mapping_t stm32_frequencyRanges[FREQUENCY_RANGE_COUNT] = {
	{ 3000000UL, 7 },  /* MSIRC1 / 8 */
	{ 6000000UL, 6 },  /* MSIRC1 / 4 */
	{ 12000000UL, 5 }, /* MSIRC1 / 2 */
	{ 24000000UL, 4 }, /* MSIRC1 / 1 */
	{ 48000000UL, 1 }, /* MSIRC0 / 2 */
	{ 96000000UL, 0 }, /* MSIRC0 / 1 */
};

#define WAIT_STATE_RANGE_COUNT 4U
static const range_mapping_t stm32_flashWaitStates[WAIT_STATE_RANGE_COUNT] = {
	{ 0UL, 0 },        /* Below 16 MHz => 0 WS */
	{ 16000000UL, 1 }, /* From 16 MHz => 1 WS */
	{ 32000000UL, 2 }, /* From 32 MHz => 2 WS */
	{ 48000000UL, 3 }, /* From 48 MHz => 3 WS */
};


static const range_mapping_t *_stm32_findRange(u32 hz, const range_mapping_t *ranges, size_t length)
{
	size_t i;
	if ((length == 0) || (hz < ranges[0].hz)) {
		return NULL;
	}

	for (i = 0; i < (length - 1U); i++) {
		if (hz < ranges[i + 1].hz) {
			return &ranges[i];
		}
	}

	return &ranges[i];
}


static void _stm32_rccSetHsi(int on)
{
	u32 waitBit, v;
	v = *(stm32_common.rcc + rcc_cr);
	if (on != 0) {
		v |= RCC_CR_HSION;
		waitBit = RCC_CR_HSIRDY;
		if ((v & waitBit) != 0U) {
			/* Already active */
			return;
		}
	}
	else {
		if ((v & RCC_CR_HSIKERON) != 0U) {
			/* HSI requested to remain active as peripheral kernel clock; don't disable it */
			return;
		}

		v &= ~RCC_CR_HSION;
		waitBit = 0U;
	}

	*(stm32_common.rcc + rcc_cr) = v;
	hal_cpuDataMemoryBarrier();

	while ((*(stm32_common.rcc + rcc_cr) & RCC_CR_HSIRDY) != waitBit) {
		/* Wait for HSIRDY */
	}
}


u8 _stm32_pwrGetCPUVolt(void)
{
	u32 vosr;

	vosr = *(stm32_common.pwr + pwr_vosr) >> 16;
	/* Note: according to documentation, Range 1 and Range 2 cannot be set at the same time */
	if (vosr & (1UL << 1)) {
		return ((vosr & PWR_VOSR_BOOSTEN) != 0) ? PCTL_CPUVOLT_RANGE2B : PCTL_CPUVOLT_RANGE2;
	}

	return ((vosr & PWR_VOSR_BOOSTEN) != 0) ? PCTL_CPUVOLT_RANGE1B : PCTL_CPUVOLT_RANGE1;
}


int _stm32_pwrSetCPUVolt(u8 range)
{
	static const u32 vosrMask = (PWR_VOSR_BOOSTEN | 0x3);
	u32 i, t, vosrVal;

	switch (range) {
		case PCTL_CPUVOLT_RANGE2:
			vosrVal = (1UL << 1);
			break;
		case PCTL_CPUVOLT_RANGE1:
			vosrVal = (1UL << 0);
			break;
		case PCTL_CPUVOLT_RANGE2B:
			vosrVal = (1UL << 1) | PWR_VOSR_BOOSTEN;
			break;
		case PCTL_CPUVOLT_RANGE1B:
			vosrVal = (1UL << 0) | PWR_VOSR_BOOSTEN;
			break;
		default:
			return -EINVAL;
	}

	t = *(stm32_common.rcc + rcc_cfgr4) & ~((0xfUL << 12) | (0x3UL << 0)); /* Clear BOOSTDIV (bypass) */
	if ((vosrVal & PWR_VOSR_BOOSTEN) != 0U) {
		_stm32_rccSetHsi(1);
		*(stm32_common.rcc + rcc_cfgr4) = t | (1UL << 1); /* Set BOOSTSEL to HSI16 */
	}
	hal_cpuDataMemoryBarrier();

	t = *(stm32_common.pwr + pwr_vosr) & ~vosrMask;
	*(stm32_common.pwr + pwr_vosr) = t | vosrVal;
	hal_cpuDataMemoryBarrier();

	for (i = 0U; i < 1000U; i++) {
		/* Wait for BOOSTRDY and RxRDY */
		if (((*(stm32_common.pwr + pwr_vosr) >> 16) & vosrMask) == vosrVal) {
			break;
		}
	}

	if (((*(stm32_common.pwr + pwr_vosr) >> 16) & vosrMask) != vosrVal) {
		return -ETIME;
	}

	if ((vosrVal & PWR_VOSR_BOOSTEN) == 0U) {
		*(stm32_common.rcc + rcc_cfgr4) = t; /* Clear BOOSTSEL */
		_stm32_rccSetHsi(0);
	}

	return EOK;
}


int _stm32_rccSetCPUClock(u32 hz)
{
	int ret = EOK;
	u32 v, clockSrc;
	u8 vRangeNew, vRangeOld, wsOld, wsNew;
	const range_mapping_t *freqRange = NULL;
	const range_mapping_t *wsRange = NULL;

	freqRange = _stm32_findRange(hz, stm32_frequencyRanges, FREQUENCY_RANGE_COUNT);
	if (freqRange == NULL) {
		return -ERANGE;
	}

	hz = freqRange->hz;
	clockSrc = RCC_CFGR1_SW_MSIS;

	wsRange = _stm32_findRange(hz, stm32_flashWaitStates, WAIT_STATE_RANGE_COUNT);
	wsNew = (u8)wsRange->val;
	if (hz > (48U * 1000U * 1000U)) {
		vRangeNew = PCTL_CPUVOLT_RANGE1B;
	}
	else if (hz > (24U * 1000U * 1000U)) {
		vRangeNew = PCTL_CPUVOLT_RANGE2B;
	}
	else {
		vRangeNew = PCTL_CPUVOLT_RANGE2;
	}

	vRangeOld = _stm32_pwrGetCPUVolt();
	wsOld = _stm32_flashGetWaitStates();
	if (vRangeNew > vRangeOld) {
		/* Requested increase in performance - set voltage range before increasing clocks */
		ret = _stm32_pwrSetCPUVolt(vRangeNew);
		if (ret < 0) {
			return ret;
		}
	}

	if (wsOld < wsNew) {
		_stm32_flashSetWaitStates(wsNew);
	}

	/* Configure MSIS */
	v = *(stm32_common.rcc + rcc_icscr1) & ~(0x7UL << 29);
	v |= (((u32)freqRange->val & 0x7UL) << 29) | (1UL << 23); /* Set MSISSEL | MSISDIV | MSIRGSEL */
	*(stm32_common.rcc + rcc_icscr1) = v;

	while ((*(stm32_common.rcc + rcc_cr) & (1U << 2)) == 0U) {
		/* Wait for MSISRDY */
	}

	/* Configure SYSCLK source if necessary */
	v = *(stm32_common.rcc + rcc_cfgr1);
	if (((v >> 2) & 0x3UL) != clockSrc) {
		v &= ~0x3UL;
		v |= clockSrc; /* Set SW */
		*(stm32_common.rcc + rcc_cfgr1) = v;

		while (((*(stm32_common.rcc + rcc_cfgr1) >> 2) & 0x3UL) != clockSrc) {
			/* Wait for SWS == SW */
		}
	}

	/* Update the current frequency */
	stm32_common.cpuclk = hz;

	if (wsOld > wsNew) {
		_stm32_flashSetWaitStates(wsNew);
	}

	if (vRangeNew < vRangeOld) {
		/* Requested reduction in performance - set voltage range after decreasing clocks */
		ret = _stm32_pwrSetCPUVolt(vRangeNew);
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	return ret;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


u32 _stm32_rccGetPerClock(void)
{
	/* At startup we set HPRE to 1 and PPRE1 ~ 3 to 2 */
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
		*(stm32_common.dbgmcu + reg) |= 1UL << ((u32)dev % 32U);
	}
	else {
		*(stm32_common.dbgmcu + reg) &= ~(1UL << ((u32)dev % 32U));
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


void _stm32_pwrEnterLPStop(time_t us)
{
	u32 freqBefore = 0U;

	timer_setAlarm(us);

	if (stm32_common.cpuclk > RCC_MAX_SLEEP_CLOCK) {
		freqBefore = stm32_common.cpuclk;
		_stm32_rccSetCPUClock(RCC_MAX_SLEEP_CLOCK);
	}

	_hal_scsDeepSleepSet(1); /* Switch to deep sleep mode */
	hal_cpuHalt();           /* WFI enters Stop mode */
	_hal_scsDeepSleepSet(0); /* Switch to shallow sleep mode */

	if (freqBefore != 0U) {
		_stm32_rccSetCPUClock(freqBefore);
	}
}


void _stm32_init(void)
{
	u32 v;

	/* Base addresses init */
	stm32_common.iwdg = IWDG_BASE;
	stm32_common.pwr = PWR_BASE;
	stm32_common.rcc = RCC_BASE;
	stm32_common.rtc = RTC_BASE;
	stm32_common.syscfg = SYSCFG_BASE;
	stm32_common.dbgmcu = DBGMCU_BASE;
	stm32_common.flash = FLASH_BASE;

	_hal_scsInit();

	_stm32_gpioInit();

	/* Enable System configuration controller */
	(void)_stm32_rccSetDevClock(pctl_syscfg, 1U, 1U);
	(void)_stm32_rccSetDevClock(pctl_vref, 1U, 1U);

	/* Enable power module */
	(void)_stm32_rccSetDevClock(pctl_pwr, 1U, 1U);

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cier) = 0;

	/* Set peripheral bus clock prescalers */
	v = *(stm32_common.rcc + rcc_cfgr2);
	v &= ~((0x7UL << 8) | (0x7UL << 4) | (0xFUL << 0)); /* Clear PPRE2, PPRE1, HPRE */
	v |= (0x4UL << 8) | (0x4UL << 4) | (0x0UL << 0);    /* Set PPRE1 ~ 2 to 2, HPRE to 1 */
	*(stm32_common.rcc + rcc_cfgr2) = v;

	v = *(stm32_common.rcc + rcc_cfgr3);
	v &= ~(0x7UL << 4); /* Clear PPRE3 */
	v |= (0x4UL << 4);  /* Set PPRE3 to 2 */
	*(stm32_common.rcc + rcc_cfgr3) = v;

	(void)_stm32_rccSetCPUClock(RCC_DEFAULT_CPU_CLOCK);

	/* Select Stop 2 as deep sleep mode */
	v = *(stm32_common.pwr + pwr_cr1) & ~(0x7U);
	*(stm32_common.pwr + pwr_cr1) = v | 2U;

	v = *(stm32_common.rcc + rcc_cfgr1);
	v &= ~(3U << 4); /* Clear STOPWUCK, STOPKERWUCK - use MSIS/MSIK for wake-up */
	*(stm32_common.rcc + rcc_cfgr1) = v;

	hal_cpuDataMemoryBarrier();

	_stm32_rtcInit();
	_stm32_wdgInit();
#ifdef NDEBUG
	*(stm32_common.dbgmcu + dbgmcu_cr) &= ~(1UL << 1); /* Turn off DBG_STOP to ensure clocks will be turned off in Stop mode */
#endif
}
