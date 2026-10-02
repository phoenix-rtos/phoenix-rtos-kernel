/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL STM32 GPIO and EXTI driver
 *
 * Copyright 2020, 2025, 2026 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/armv8m/stm32/stm32.h"

#include "hal/cpu.h"
#include "hal/hal.h"
#include "include/errno.h"

/*
 * Note: the lookups depend on all pctl_gpio* enum values being aligned with alphabet letters in sequence,
 * i.e. pctl_gpiob == (pctl_gpioa + 1), pctl_gpioz == (pctl_gpioa + 25).
 */

#if defined(__CPU_STM32N6)
#include "hal/armv8m/stm32/n6/stm32n6_regs.h"

#define MAX_GPIO pctl_gpioq

static volatile u32 *const stm32_gpioBase[MAX_GPIO - pctl_gpioa + 1] = {
	(volatile u32 *)0x56020000U, /* GPIOA */
	(volatile u32 *)0x56020400U, /* GPIOB */
	(volatile u32 *)0x56020800U, /* GPIOC */
	(volatile u32 *)0x56020c00U, /* GPIOD */
	(volatile u32 *)0x56021000U, /* GPIOE */
	(volatile u32 *)0x56021400U, /* GPIOF */
	(volatile u32 *)0x56021800U, /* GPIOG */
	(volatile u32 *)0x56021c00U, /* GPIOH */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(volatile u32 *)0x56023400U, /* GPION */
	(volatile u32 *)0x56023800U, /* GPIOO */
	(volatile u32 *)0x56023c00U, /* GPIOP */
	(volatile u32 *)0x56024000U, /* GPIOQ */
};

#define EXTI_BASE  ((void *)0x56025000U)
#define EXTI_LINES 78U
#elif defined(__CPU_STM32U3)
#include "hal/armv8m/stm32/u3/stm32u3_regs.h"

#define MAX_GPIO pctl_gpioh

static volatile u32 *const stm32_gpioBase[MAX_GPIO - pctl_gpioa + 1] = {
	(volatile u32 *)0x52020000U, /* GPIOA */
	(volatile u32 *)0x52020400U, /* GPIOB */
	(volatile u32 *)0x52020800U, /* GPIOC */
	(volatile u32 *)0x52020c00U, /* GPIOD */
	(volatile u32 *)0x52021000U, /* GPIOE */
	(volatile u32 *)0x52021400U, /* GPIOF */
	(volatile u32 *)0x52021800U, /* GPIOG */
	(volatile u32 *)0x52021c00U, /* GPIOH */
};

#define EXTI_BASE  ((void *)0x50032000U)
#define EXTI_LINES 23U
#else
#error "Unsupported platform"
#endif

static volatile u32 *const stm32_extiBase = EXTI_BASE;


/* GPIO */


static volatile u32 *_stm32_gpioGetBase(int d)
{
	if ((d < pctl_gpioa) || (d > MAX_GPIO)) {
		return NULL;
	}

	return stm32_gpioBase[d - pctl_gpioa];
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


#if STM32_GPIO_PRIVILEGE
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
#endif


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
		*(stm32_extiBase + offs) |= bit;
	}
	else {
		*(stm32_extiBase + offs) &= ~bit;
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
		*(stm32_extiBase + offs) |= bit;
	}
	else {
		*(stm32_extiBase + offs) &= ~bit;
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
		*(stm32_extiBase + offs) |= bit;
	}
	else {
		*(stm32_extiBase + offs) &= ~bit;
	}

	return EOK;
}


int _stm32_extiSoftInterrupt(u32 line)
{
	u32 offs, bit;
	if (_stm32_extiLineToRegBit(line, &offs, &bit) < 0) {
		return -EINVAL;
	}

	*(stm32_extiBase + exti_swier1 + offs) |= bit;
	return EOK;
}


static u32 _stm32_extiGetMuxValue(int gpioDev)
{
	int num;
	num = gpioDev - pctl_gpioa;
#if defined(__CPU_STM32N6)
	/* This is one of very few cases on STM32N6 where port numbering is continuous, without a hole for missing ports */
	if (gpioDev >= pctl_gpion) {
		num -= (pctl_gpion - pctl_gpioh) - 1;
	}
#endif

	return ((u32)num) & 0xffUL;
}


int _stm32_extiSetGpioMux(int gpioDev, u8 pin)
{
	u32 val, mux;
	u8 shift, offset;
	if ((pin > 15U) || (_stm32_gpioGetBase(gpioDev) == NULL)) {
		return -EINVAL;
	}

	shift = (pin % 4U) * 8U;
	offset = pin / 4U;
	mux = _stm32_extiGetMuxValue(gpioDev);
	val = *(stm32_extiBase + exti_exticr1 + offset) & ~(0xffUL << shift);
	*(stm32_extiBase + exti_exticr1 + offset) = val | (mux << shift);
	return EOK;
}


void _stm32_gpioInit(void)
{
	int i;
	for (i = pctl_gpioa; i <= MAX_GPIO; i++) {
		if (stm32_gpioBase[i - pctl_gpioa] != NULL) {
			(void)_stm32_rccSetDevClock(i, 1, 1);
		}
	}
}
