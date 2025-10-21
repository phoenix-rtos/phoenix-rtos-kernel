/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32L4 basic peripherals control functions
 *
 * Copyright 2020 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv7m/stm32/stm32.h"
#include "hal/armv7m/stm32/stm32-timer.h"
#include "hal/armv7m/stm32/halsyspage.h"

#include "hal/cpu.h"
#include "include/errno.h"

#include "hal/arm/scs.h"

#include <board_config.h>

#if defined(WATCHDOG) && defined(WATCHDOG_TIMEOUT_MS)
#warning "This target doesn't support WATCHDOG_TIMEOUT_MS. Watchdog timeout is 31992 ms."
#endif


static struct {
	volatile u32 *rcc;
	volatile u32 *gpio[9];
	volatile u32 *pwr;
	volatile u32 *rtc;
	volatile u32 *exti;
	volatile u32 *syscfg;
	volatile u32 *iwdg;
	volatile u32 *flash;

	u32 cpuclk;

	spinlock_t pltctlSp;
} stm32_common;

/* clang-format off */
enum { ahb1_begin = pctl_dma1, ahb1_end = pctl_dma2d, ahb2_begin = pctl_gpioa, ahb2_end = pctl_rng,
	ahb3_begin = pctl_fmc, ahb3_end = pctl_qspi, apb1_1_begin = pctl_tim2, apb1_1_end = pctl_lptim1,
	apb1_2_begin = pctl_lpuart1, apb1_2_end = pctl_lptim2, apb2_begin = pctl_syscfg, apb2_end = pctl_dfsdm1,
	misc_begin = pctl_rtc, misc_end = pctl_rtc };


enum { rcc_cr = 0, rcc_icscr, rcc_cfgr, rcc_pllcfgr, rcc_pllsai1cfgr, rcc_pllsai2cfgr, rcc_cier, rcc_cifr,
	rcc_cicr, rcc_ahb1rstr = rcc_cicr + 2, rcc_ahb2rstr, rcc_ahb3rstr, rcc_apb1rstr1 = rcc_ahb3rstr + 2,
	rcc_apb1rstr2, rcc_apb2rstr, rcc_ahb1enr = rcc_apb2rstr + 2, rcc_ahb2enr, rcc_ahb3enr,
	rcc_apb1enr1 = rcc_ahb3enr + 2, rcc_apb1enr2, rcc_apb2enr, rcc_ahb1smenr = rcc_apb2enr + 2,
	rcc_ahb2smenr, rcc_ahb3smenr, rcc_apb1smenr1 = rcc_ahb3smenr + 2, rcc_apb1smenr2, rcc_apb2smenr,
	rcc_ccipr = rcc_apb2smenr + 2, rcc_bdcr = rcc_ccipr + 2, rcc_csr, rcc_crrcr, rcc_ccipr2 };


enum { gpio_moder = 0, gpio_otyper, gpio_ospeedr, gpio_pupdr, gpio_idr,
	gpio_odr, gpio_bsrr, gpio_lckr, gpio_afrl, gpio_afrh, gpio_brr, gpio_ascr };


enum { pwr_cr1 = 0, pwr_cr2, pwr_cr3, pwr_cr4, pwr_sr1, pwr_sr2, pwr_scr, pwr_pucra, pwr_pdcra, pwr_pucrb,
	pwr_pdcrb, pwr_pucrc, pwr_pdcrc, pwr_pucrd, pwr_pdcrd, pwr_pucre, pwr_pdcre, pwr_pucrf, pwr_pdcrf,
	pwr_pucrg, pwr_pdcrg, pwr_pucrh, pwr_pdcrh, pwr_pucri, pwr_pdcri };


enum { rtc_tr = 0, rtc_dr, rtc_cr, rtc_isr, rtc_prer, rtc_wutr, rtc_alrmar = rtc_wutr + 2, rtc_alrmbr, rtc_wpr,
	rtc_ssr, rtc_shiftr, rtc_tstr, rtc_tsdr, rtc_tsssr, rtc_calr, rtc_tampcr, rtc_alrmassr, rtc_alrmbssr, rtc_or,
	rtc_bkpr };


enum { exti_imr1 = 0, exti_emr1, exti_rtsr1, exti_ftsr1, exti_swier1, exti_pr1, exti_imr2 = 8, exti_emr2,
	exti_rtsr2, exti_ftsr2, exti_swier2, exti_pr2 };


enum { syscfg_memrmp = 0, syscfg_cfgr1, syscfg_exticr1, syscfg_exticr2, syscfg_exticr3, syscfg_exticr4,
	syscfg_scsr, syscfg_cfgr2, syscfg_swpr, syscfg_skr, syscfg_swpr2 };


enum { iwdg_kr = 0, iwdg_pr, iwdg_rlr, iwdg_sr, iwdg_winr };


enum { flash_acr = 0, flash_pdkeyr, flash_keyr, flash_optkeyr, flash_sr, flash_cr, flash_eccr,
	flash_optr = flash_eccr + 2, flash_pcrop1sr, flash_pcrop1er, flash_wrp1ar, flash_wrp1br,
	flash_pcrop2sr = flash_wrp1br + 5, flash_pcrop2er, flash_wrp2ar, flash_wrp2br };
/* clang-format on */


/* platformctl syscall */


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;
	unsigned int state;
	spinlock_ctx_t sc;

	hal_spinlockSet(&stm32_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_devclk:
			if (data->action == pctl_set) {
				ret = _stm32_rccSetDevClock(data->devclk.dev, data->devclk.state);
			}
			else if (data->action == pctl_get) {
				ret = _stm32_rccGetDevClock(data->devclk.dev, &state);
				data->devclk.state = state;
			}
			else {
				/* No action required */
			}

			break;
		case pctl_cpuclk:
			if (data->action == pctl_set) {
				ret = _stm32_rccSetCPUClock(data->cpuclk.hz);
				(void)_stm32_systickInit(SYSTICK_INTERVAL);
			}
			else if (data->action == pctl_get) {
				data->cpuclk.hz = _stm32_rccGetCPUClock();
				ret = EOK;
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

		default:
			/* Error by default */
			break;
	}

	hal_spinlockClear(&stm32_common.pltctlSp, &sc);

	return ret;
}


void _stm32_platformInit(void)
{
	hal_spinlockCreate(&stm32_common.pltctlSp, "pltctl");
}


/* RCC (Reset and Clock Controller) */


int _stm32_rccSetDevClock(unsigned int d, u32 state)
{
	u32 t;

	state = (state == 0UL ? 1UL : 0UL);

	/* there are gaps in numeration so values need to compared with both begin and end */

	if (d >= (unsigned int)ahb1_begin && d <= (unsigned int)ahb1_end) {
		t = *(stm32_common.rcc + rcc_ahb1enr) & ~(1UL << (d - (unsigned int)ahb1_begin));
		*(stm32_common.rcc + rcc_ahb1enr) = t | (state << (d - (unsigned int)ahb1_begin));
	}
	else if (d >= (unsigned int)ahb2_begin && d <= (unsigned int)ahb2_end) {
		t = *(stm32_common.rcc + rcc_ahb2enr) & ~(1UL << (d - (unsigned int)ahb2_begin));
		*(stm32_common.rcc + rcc_ahb2enr) = t | (state << (d - (unsigned int)ahb2_begin));
	}
	else if (d >= (unsigned int)ahb3_begin && d <= (unsigned int)ahb3_end) {
		t = *(stm32_common.rcc + rcc_ahb3enr) & ~(1UL << (d - (unsigned int)ahb3_begin));
		*(stm32_common.rcc + rcc_ahb3enr) = t | (state << (d - (unsigned int)ahb3_begin));
	}
	else if (d >= (unsigned int)apb1_1_begin && d <= (unsigned int)apb1_1_end) {
		t = *(stm32_common.rcc + rcc_apb1enr1) & ~(1UL << (d - (unsigned int)apb1_1_begin));
		*(stm32_common.rcc + rcc_apb1enr1) = t | (state << (d - (unsigned int)apb1_1_begin));
	}
	else if (d >= (unsigned int)apb1_2_begin && d <= (unsigned int)apb1_2_end) {
		t = *(stm32_common.rcc + rcc_apb1enr2) & ~(1UL << (d - (unsigned int)apb1_2_begin));
		*(stm32_common.rcc + rcc_apb1enr2) = t | (state << (d - (unsigned int)apb1_2_begin));
	}
	else if (d >= (unsigned int)apb2_begin && d <= (unsigned int)apb2_end) {
		t = *(stm32_common.rcc + rcc_apb2enr) & ~(1UL << (d - (unsigned int)apb2_begin));
		*(stm32_common.rcc + rcc_apb2enr) = t | (state << (d - (unsigned int)apb2_begin));
	}
	else if (d == (unsigned int)pctl_rtc) {
		t = *(stm32_common.rcc + rcc_bdcr) & ~(1UL << 15);
		*(stm32_common.rcc + rcc_bdcr) = t | (state << 15);
	}
	else if (d == (unsigned int)pctl_hsi48) {
		/* Enable HSI48 */
		*(stm32_common.rcc + rcc_crrcr) |= 1UL;
		hal_cpuDataMemoryBarrier();
		/* And wait for it to turn on */
		while ((*(stm32_common.rcc + rcc_crrcr) & (1U << 1)) == 0U) {
		}
	}
	else {
		return -EINVAL;
	}

	hal_cpuDataMemoryBarrier();

	return EOK;
}


int _stm32_rccGetDevClock(unsigned int d, u32 *state)
{
	/* there are gaps in numeration so values need to compared with both begin and end */
	if ((d >= (unsigned int)ahb1_begin) && (d <= (unsigned int)ahb1_end)) {
		*state = (*(stm32_common.rcc + rcc_ahb1enr) & (1UL << (d - (unsigned int)ahb1_begin))) == 0UL ? 0UL : 1LU;
	}
	else if ((d >= (unsigned int)ahb2_begin) && (d <= (unsigned int)ahb2_end)) {
		*state = (*(stm32_common.rcc + rcc_ahb2enr) & (1UL << (d - (unsigned int)ahb2_begin))) == 0UL ? 0UL : 1LU;
	}
	else if ((d >= (unsigned int)ahb3_begin) && (d <= (unsigned int)ahb3_end)) {
		*state = (*(stm32_common.rcc + rcc_ahb3enr) & (1UL << (d - (unsigned int)ahb3_begin))) == 0UL ? 0UL : 1LU;
	}
	else if ((d >= (unsigned int)apb1_1_begin) && (d <= (unsigned int)apb1_1_end)) {
		*state = (*(stm32_common.rcc + rcc_apb1enr1) & (1UL << (d - (unsigned int)apb1_1_begin))) == 0UL ? 0UL : 1LU;
	}
	else if ((d >= (unsigned int)apb1_2_begin) && (d <= (unsigned int)apb1_2_end)) {
		*state = (*(stm32_common.rcc + rcc_apb1enr2) & (1UL << (d - (unsigned int)apb1_2_begin))) == 0UL ? 0UL : 1LU;
	}
	else if ((d >= (unsigned int)apb2_begin) && (d <= (unsigned int)apb2_end)) {
		*state = (*(stm32_common.rcc + rcc_apb2enr) & (1UL << (d - (unsigned int)apb2_begin))) == 0UL ? 0UL : 1LU;
	}
	else if (d == (unsigned int)pctl_rtc) {
		*state = (*(stm32_common.rcc + rcc_bdcr) & (1UL << 15)) == 0UL ? 0UL : 1LU;
	}
	else if (d == (unsigned int)pctl_hsi48) {
		*state = ((*(stm32_common.rcc + rcc_crrcr) & (1UL << 1)) == 0UL) ? 0UL : 1UL;
	}
	else {
		return -EINVAL;
	}

	return EOK;
}


static void _stm32_rccMsiToHsi(void)
{
	u32 t;

	/* Enable HSI */
	*(stm32_common.rcc + rcc_cr) |= 1UL << 8;
	hal_cpuDataMemoryBarrier();

	while ((*(stm32_common.rcc + rcc_cr) & (1UL << 10)) == 0U) {
	}

	/* Change system clk to HSI */
	t = *(stm32_common.rcc + rcc_cfgr) & ~3U;
	*(stm32_common.rcc + rcc_cfgr) = t | 1U;
	hal_cpuDataMemoryBarrier();

	/* Wait for HSI selection */
	while (((*(stm32_common.rcc + rcc_cfgr) >> 2) & 3U) != 1U) {
	}

	/* Disable MSI */
	*(stm32_common.rcc + rcc_cr) &= ~1U;
	hal_cpuDataMemoryBarrier();

	while ((*(stm32_common.rcc + rcc_cr) & (1U << 1)) != 0U) {
	}
}


static void _stm32_rccHsiToMsi(void)
{
	/* Enable MSI */
	*(stm32_common.rcc + rcc_cr) |= 1U;
	hal_cpuDataMemoryBarrier();

	while ((*(stm32_common.rcc + rcc_cr) & (1U << 1)) == 0U) {
	}

	/* Change system clk to MSI */
	*(stm32_common.rcc + rcc_cfgr) &= ~3U;
	hal_cpuDataMemoryBarrier();

	/* Wait for MSI selection */
	while (((*(stm32_common.rcc + rcc_cfgr) >> 2) & 3U) != 0U) {
	}

	/* Disable HSI */
	*(stm32_common.rcc + rcc_cr) &= ~(1UL << 8);
	hal_cpuDataMemoryBarrier();
}


int _stm32_rccSetCPUClock(u32 hz)
{
	u8 range;
	u32 t;

	if (hz <= 100U * 1000U) {
		range = 0;
		hz = 100U * 1000U;
	}
	else if (hz <= 200U * 1000U) {
		range = 1;
		hz = 200U * 1000U;
	}
	else if (hz <= 400U * 1000U) {
		range = 2;
		hz = 400U * 1000U;
	}
	else if (hz <= 800U * 1000U) {
		range = 3;
		hz = 800U * 1000U;
	}
	else if (hz <= 1000U * 1000U) {
		range = 4;
		hz = 1000U * 1000U;
	}
	else if (hz <= 2000U * 1000U) {
		range = 5;
		hz = 2000U * 1000U;
	}
	else if (hz <= 4000U * 1000U) {
		range = 6;
		hz = 4000U * 1000U;
	}
	else if (hz <= 8000U * 1000U) {
		range = 7;
		hz = 8000U * 1000U;
	}
	else if (hz <= 16000U * 1000U) {
		range = 8;
		hz = 16000U * 1000U;
	}
	/* TODO - we need to change flash wait states to handle below frequencies
		else if (hz <= 24000 * 1000) {
			range = 9;
			hz = 24000 * 1000;
		}
		else if (hz <= 32000 * 1000) {
			range = 10;
			hz = 32000 * 1000;
		}
		else if (hz <= 48000 * 1000) {
			range = 11;
			hz = 48000 * 1000;
		}
	*/
	else {
		/* Not supported */
		return -EINVAL;
	}

	if (hz > 6000U * 1000U) {
		_stm32_pwrSetCPUVolt(1);
	}

	if (hz == 16U * 1000U * 1000U) {
		/* We can use HSI */
		_stm32_rccMsiToHsi();

		/* Use HSI after STOP2 wakeup */
		*(stm32_common.rcc + rcc_cfgr) |= 1UL << 15;
		hal_cpuDataMemoryBarrier();
	}
	else {
		/* Enable MSI (doesn't hurt if already enabled) */
		*(stm32_common.rcc + rcc_cr) |= 1U;
		hal_cpuDataMemoryBarrier();

		/* Wait for MSI ready */
		while ((*(stm32_common.rcc + rcc_cr) & 2U) == 0U) {
		}

		/* Set MSI range */
		t = *(stm32_common.rcc + rcc_cr) & ~(0xfU << 4);
		*(stm32_common.rcc + rcc_cr) = t | (u32)range << 4 | (1UL << 3);
		hal_cpuDataMemoryBarrier();

		_stm32_rccHsiToMsi();

		/* Can use Vcore range 2 only below 6 MHz */
		if (hz <= 6000U * 1000U) {
			_stm32_pwrSetCPUVolt(2);
		}

		/* Use MSI after STOP2 wakeup */
		*(stm32_common.rcc + rcc_cfgr) &= ~(1UL << 15);
		hal_cpuDataMemoryBarrier();
	}

	stm32_common.cpuclk = hz;

	return EOK;
}


u32 _stm32_rccGetCPUClock(void)
{
	return stm32_common.cpuclk;
}


void _stm32_rccClearResetFlags(void)
{
	*(stm32_common.rcc + rcc_csr) |= 1UL << 23;
}


/* RTC */


void _stm32_rtcUnlockRegs(void)
{
	/* Set DBP bit */
	*(stm32_common.pwr + pwr_cr1) |= 1UL << 8;

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
	*(stm32_common.pwr + pwr_cr1) &= ~(1UL << 8);
}


/* PWR */


void _stm32_pwrSetCPUVolt(u8 range)
{
	u32 t;

	if (range != 1U && range != 2U) {
		return;
	}

	t = *(stm32_common.pwr + pwr_cr1) & ~(3UL << 9);
	*(stm32_common.pwr + pwr_cr1) = t | ((u32)range << 9);

	/* Wait for VOSF flag */
	while ((*(stm32_common.pwr + pwr_sr2) & (1UL << 10)) != 0U) {
		;
	}
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
time_t _stm32_pwrEnterLPStop(time_t us)
{
	unsigned int t;
	int restoreMsi = 0;

	/* Set internal regulator to default range as we're switching to HSI */
	_stm32_pwrSetCPUVolt(1);

	if (((*(stm32_common.rcc + rcc_cfgr) >> 2) & 3U) == 0U) {
		/* Errata ES0335 rev 17 2.2.4 - initiate STOP mode on HSI if MSI is selected */
		restoreMsi = 1;
		_stm32_rccMsiToHsi();
	}

	/* Enter Stop2 on deep-sleep */
	t = *(stm32_common.pwr + pwr_cr1) & ~(0x7U);
	*(stm32_common.pwr + pwr_cr1) = t | 2U;
	hal_cpuDataMemoryBarrier();

	/* Set SLEEPDEEP bit of Cortex System Control Register */
	_hal_scsDeepSleepSet(1);

	timer_setAlarm(us);

	/* Enter Stop mode */
	__asm__ volatile("\
		dmb; \
		wfi; \
		nop; ");

	/* Reset SLEEPDEEP bit of Cortex System Control Register */
	_hal_scsDeepSleepSet(0);

	if (restoreMsi != 0) {
		/* Restore pre-sleep MSI clock */
		_stm32_rccHsiToMsi();
	}

	/* Can use Vcore range 2 only below 6 MHz */
	if (stm32_common.cpuclk <= 6U * 1000U * 1000U) {
		_stm32_pwrSetCPUVolt(2);
	}

	return 0;
}


/* EXTI */


int _stm32_extiMaskInterrupt(u32 line, u8 state)
{
	u32 t;
	volatile u32 *base = (line < 32U) ? (stm32_common.exti + exti_imr1) : (stm32_common.exti + exti_imr2);

	if (line > 40U) {
		return -EINVAL;
	}
	else if (line >= 32U) {
		line -= 32U;
	}
	else {
		/* No action required*/
	}

	t = *base & ~((state == 0U ? 1UL : 0UL) << line);
	*base = t | (state == 0U ? 0UL : 1UL) << line;

	return EOK;
}


int _stm32_extiMaskEvent(u32 line, u8 state)
{
	u32 t;
	volatile u32 *reg = (line < 32U) ? (stm32_common.exti + exti_emr1) : (stm32_common.exti + exti_emr2);

	if (line > 40U) {
		return -EINVAL;
	}
	else if (line >= 32U) {
		line -= 32U;
	}
	else {
		/* No action required */
	}

	t = *reg & ~((state == 0U ? 1UL : 0UL) << line);
	*reg = t | (state == 0U ? 0UL : 1UL) << line;

	return EOK;
}


int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge)
{
	volatile u32 *p;
	const int reglut[2][2] = { { exti_ftsr1, exti_rtsr1 }, { exti_ftsr2, exti_rtsr2 } };

	if (line > 40U) {
		return -EINVAL;
	}

	p = stm32_common.exti + reglut[line >= 32U][edge == 0U ? 0U : 1U];

	if (line >= 32U) {
		line -= 32U;
	}

	*p &= ~((state == 0U ? 1UL : 0UL) << line);
	*p |= (state == 0U ? 0UL : 1UL) << line;

	return EOK;
}


int _stm32_extiSoftInterrupt(u32 line)
{
	if (line > 40U) {
		return -EINVAL;
	}

	if (line < 32U) {
		*(stm32_common.exti + exti_swier1) |= 1UL << line;
	}
	else {
		*(stm32_common.exti + exti_swier2) |= 1UL << (line - 32U);
	}

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


int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd)
{
	volatile u32 *base;
	u32 t;

	if (d > (unsigned int)pctl_gpioi || pin > 15U) {
		return -EINVAL;
	}

	base = stm32_common.gpio[d - (unsigned int)pctl_gpioa];

	t = *(base + gpio_moder) & ~(0x3U << (pin << 1));
	*(base + gpio_moder) = t | ((u32)mode & 0x3U) << (pin << 1);

	t = *(base + gpio_otyper) & ~(1U << pin);
	*(base + gpio_otyper) = t | ((u32)otype & 1U) << pin;

	t = *(base + gpio_ospeedr) & ~(0x3U << (pin << 1));
	*(base + gpio_ospeedr) = t | ((u32)ospeed & 0x3U) << (pin << 1);

	t = *(base + gpio_pupdr) & ~(0x03U << (pin << 1));
	*(base + gpio_pupdr) = t | ((u32)pupd & 0x3U) << (pin << 1);

	if (pin < 8U) {
		t = *(base + gpio_afrl) & ~(0xfU << (pin << 2));
		*(base + gpio_afrl) = t | ((u32)af & 0xfU) << (pin << 2);
	}
	else {
		t = *(base + gpio_afrh) & ~(0xfU << ((pin - 8U) << 2));
		*(base + gpio_afrh) = t | ((u32)af & 0xfU) << ((pin - 8U) << 2);
	}

	if (mode == 0x3U) {
		*(base + gpio_ascr) |= 1UL << pin;
	}
	else {
		*(base + gpio_ascr) &= ~(1UL << pin);
	}

	return EOK;
}


int _stm32_gpioSet(unsigned int d, u8 pin, u8 val)
{
	volatile u32 *base;
	u32 t;

	if (d > (unsigned int)pctl_gpioi || pin > 15U) {
		return -EINVAL;
	}

	base = stm32_common.gpio[d - (unsigned int)pctl_gpioa];

	t = *(base + gpio_odr) & ~((val == 0U ? 1UL : 0UL) << pin);
	*(base + gpio_odr) = t | (val == 0U ? 0UL : 1UL) << pin;

	return EOK;
}


int _stm32_gpioSetPort(unsigned int d, u16 val)
{
	volatile u32 *base;

	if (d > (unsigned int)pctl_gpioi) {
		return -EINVAL;
	}

	base = stm32_common.gpio[d - (unsigned int)pctl_gpioa];
	*(base + gpio_odr) = val;

	return EOK;
}


int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val)
{
	volatile u32 *base;

	if (d > (unsigned int)pctl_gpioi || pin > 15U) {
		return -EINVAL;
	}

	base = stm32_common.gpio[d - (unsigned int)pctl_gpioa];
	*val = (*(base + gpio_idr) & (1UL << pin)) == 0U ? 0U : 1U;

	return EOK;
}


int _stm32_gpioGetPort(unsigned int d, u16 *val)
{
	volatile u32 *base;

	if (d > (unsigned int)pctl_gpioi) {
		return -EINVAL;
	}

	base = stm32_common.gpio[d - (unsigned int)pctl_gpioa];
	*val = *(base + gpio_idr);

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
	u32 t, i;
	static const int gpio2pctl[] = { pctl_gpioa, pctl_gpiob, pctl_gpioc,
		pctl_gpiod, pctl_gpioe, pctl_gpiof, pctl_gpiog, pctl_gpioh, pctl_gpioi };

	/* Base addresses init */
	stm32_common.rcc = (void *)0x40021000;
	stm32_common.pwr = (void *)0x40007000;
	stm32_common.rtc = (void *)0x40002800;
	stm32_common.exti = (void *)0x40010400;
	stm32_common.syscfg = (void *)0x40010000;
	stm32_common.iwdg = (void *)0x40003000;
	stm32_common.gpio[0] = (void *)0x48000000; /* GPIOA */
	stm32_common.gpio[1] = (void *)0x48000400; /* GPIOB */
	stm32_common.gpio[2] = (void *)0x48000800; /* GPIOC */
	stm32_common.gpio[3] = (void *)0x48000c00; /* GPIOD */
	stm32_common.gpio[4] = (void *)0x48001000; /* GPIOE */
	stm32_common.gpio[5] = (void *)0x48001400; /* GPIOF */
	stm32_common.gpio[6] = (void *)0x48001800; /* GPIOG */
	stm32_common.gpio[7] = (void *)0x48001c00; /* GPIOH */
	stm32_common.gpio[8] = (void *)0x48002000; /* GPIOI */
	stm32_common.flash = (void *)0x40022000;

	_hal_scsInit();

	/* Enable System configuration controller */
	(void)_stm32_rccSetDevClock(pctl_syscfg, 1);

	/* Enable power module */
	(void)_stm32_rccSetDevClock(pctl_pwr, 1);

	(void)_stm32_rccSetCPUClock(16U * 1000U * 1000U);

	/* Disable all interrupts */
	*(stm32_common.rcc + rcc_cier) = 0;

	hal_cpuDataMemoryBarrier();

	/* GPIO init */
	for (i = 0; i < sizeof(stm32_common.gpio) / sizeof(stm32_common.gpio[0]); ++i) {
		(void)_stm32_rccSetDevClock((unsigned int)gpio2pctl[i], 1);
	}

	/* Set DBP bit */
	*(stm32_common.pwr + pwr_cr1) |= 1UL << 8;
	hal_cpuDataMemoryBarrier();

	/* Enable LSE clock source, set it as RTC source and set medium xtal drive strength */
	t = *(stm32_common.rcc + rcc_bdcr) & ~((3UL << 24) | (3UL << 15) | (3UL << 8) | 0x7fU);
	*(stm32_common.rcc + rcc_bdcr) = t | (1UL << 25) | (1UL << 15) | (1UL << 8) | (1UL << 3) | 1U;
	hal_cpuDataMemoryBarrier();

	/* And wait for it to turn on */
	while ((*(stm32_common.rcc + rcc_bdcr) & (1U << 1)) == 0U) {
		;
	}

	/* Select system clock for ADC */
	*(stm32_common.rcc + rcc_ccipr) |= 0x3UL << 28;

	hal_cpuDataMemoryBarrier();

	/* Initialize RTC */
	/* Unlock RTC */
	_stm32_rtcUnlockRegs();

	/* Turn on RTC */
	(void)_stm32_rccSetDevClock(pctl_rtc, 1);
	*(stm32_common.rcc + rcc_bdcr) |= 1UL << 15;

	hal_cpuDataMemoryBarrier();

	/* Set INIT bit */
	*(stm32_common.rtc + rtc_isr) |= 1U << 7;
	while ((*(stm32_common.rtc + rtc_isr) & (1U << 6)) == 0U) {
		;
	}

	/* Set RTC prescaler (it has to be done this way) */
	t = *(stm32_common.rtc + rtc_prer) & ~(0x7fUL << 16);
	*(stm32_common.rtc + rtc_prer) = t | (0xfUL << 16);
	t = *(stm32_common.rtc + rtc_prer) & ~0x7fffU;
	*(stm32_common.rtc + rtc_prer) = t | 0x7ffU;

	/* Reset RTC interrupt bits WUTIE & WUTE */
	*(stm32_common.rtc + rtc_cr) &= ~((1UL << 14) | (1UL << 10));

	/* Turn on shadow register bypass */
	*(stm32_common.rtc + rtc_cr) |= 1U << 5;

	/* Select RTC/16 wakeup clock */
	*(stm32_common.rtc + rtc_cr) &= ~0x7U;

	/* Clear INIT bit */
	*(stm32_common.rtc + rtc_isr) &= ~(1U << 7);
	_stm32_rtcLockRegs();

	/* Clear pending interrupts */
	*(stm32_common.exti + exti_pr1) |= 0xffffffU;
	*(stm32_common.exti + exti_pr2) |= 0xffffffU;

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

#ifdef NDEBUG
	*(u32 *)0xe0042004 = 0U;
#endif

	/* Disable FPU */
	_hal_scsFPUSet(0);

	/* Enable internal wakeup line */
	*(stm32_common.pwr + pwr_cr3) |= 1UL << 15;

	/* Flash in power-down during low power modes */
	*(stm32_common.flash + flash_acr) |= 1UL << 14;

	/* LSE as clock source for all LP peripherals */
	*(stm32_common.rcc + rcc_ccipr) |= (0x3UL << 20) | (0x3UL << 18) | (0x3UL << 10);

	(void)_stm32_rccSetDevClock(pctl_lptim1, 1);

	/* Unmask event */
	(void)_stm32_extiMaskEvent(32, 1);

	/* Set rising edge trigger */
	(void)_stm32_extiSetTrigger(32, 1, 1);

	/* Clear DBP bit */
	*(stm32_common.pwr + pwr_cr1) &= ~(1UL << 8);

	return;
}
