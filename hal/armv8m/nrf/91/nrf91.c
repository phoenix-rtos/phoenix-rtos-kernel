/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * nRF91 basic peripherals control functions
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "nrf91.h"

#include "../../../cpu.h"
#include "../../armv8m.h"
#include "../../../../include/errno.h"


static struct {
	volatile u32 *scb;
	volatile u32 *power;
	volatile u32 *clock;
	volatile u32 *gpio;
	u32 cpuclk;
	spinlock_t pltctlSp;
} nrf91_common;


/* clang-format off */
enum { power_tasks_constlat = 30, power_tasks_lowpwr, power_inten = 192, power_intenset, power_intenclr, power_status = 272};


enum { clock_tasks_hfclkstart = 0, clock_inten = 192, clock_intenset, clock_intenclr, clock_hfclkrun = 258, clock_hfclkstat };


enum { gpio_out = 1, gpio_outset, gpio_outclr, gpio_in, gpio_dir, gpio_dirsetout, gpio_dirsetin, gpio_cnf = 128 };


enum { syst_csr = 4, syst_rvr, syst_cvr, syst_calib };


enum { fpu_cpacr = 34, fpu_fpccr = 141, fpu_fpcar, fpu_fpdscr };
/* clang-format on */


/* platformctl syscall */


/* TODO: add platformctl implementation */
int hal_platformctl(void *ptr)
{
	return -ENOSYS;
}


void _nrf91_platformInit(void)
{
	hal_spinlockCreate(&nrf91_common.pltctlSp, "pltctl");
}


/* SysTick */


int _nrf91_systickInit(u32 interval)
{
	u64 load = ((u64)interval * nrf91_common.cpuclk) / 1000000;
	if (load > 0x00ffffff) {
		return -EINVAL;
	}

	*(nrf91_common.scb + syst_rvr) = (u32)load;
	*(nrf91_common.scb + syst_cvr) = 0u;

	/* Enable systick */
	*(nrf91_common.scb + syst_csr) |= 0x7u;

	return EOK;
}


/* GPIO */


int _nrf91_gpioConfig(u8 pin, u8 dir, u8 pull)
{
	if (pin > 31) {
		return -1;
	}

	if (dir == gpio_output) {
		*(nrf91_common.gpio + gpio_dirsetout) = (1u << pin);
		hal_cpuDataMemoryBarrier();
	}
	else if (dir == gpio_input) {
		*(nrf91_common.gpio + gpio_dirsetin) = (1u << pin);
		hal_cpuDataMemoryBarrier();
		/* connect input buffer */
		*(nrf91_common.gpio + gpio_cnf + pin) &= ~0x2;
	}

	if (pull) {
		*(nrf91_common.gpio + gpio_cnf + pin) = (pull << 2);
	}

	return 0;
}


int _nrf91_gpioSet(u8 pin, u8 val)
{
	if (pin > 31) {
		return -1;
	}

	if (val == gpio_high) {
		*(nrf91_common.gpio + gpio_outset) = (1u << pin);
		hal_cpuDataMemoryBarrier();
	}
	else if (val == gpio_low) {
		*(nrf91_common.gpio + gpio_outclr) = (1u << pin);
		hal_cpuDataMemoryBarrier();
	}

	return 0;
}


/* SCB */


void _nrf91_scbSetPriorityGrouping(u32 group)
{
	u32 t;

	/* Get register value and clear bits to set */
	t = *(nrf91_common.scb + scb_aircr) & ~0xffff0700;

	/* Set AIRCR.PRIGROUP to 3: 16 priority groups and 16 subgroups
	   The value is same as for armv7m4-stm32l4x6 target
	   Setting various priorities is not supported on Phoenix-RTOS, so it's just default value */
	*(nrf91_common.scb + scb_aircr) = t | 0x5fa0000 | ((group & 7) << 8);
}


u32 _nrf91_scbGetPriorityGrouping(void)
{
	return (*(nrf91_common.scb + scb_aircr) & 0x700) >> 8;
}


void _nrf91_scbSetPriority(s8 excpn, u32 priority)
{
	volatile u8 *ptr;

	ptr = &((u8 *)(nrf91_common.scb + scb_shp1))[excpn - 4];

	/* We set only group priority field */
	*ptr = (priority << 4) & 0xff;
}


u32 _nrf91_scbGetPriority(s8 excpn)
{
	volatile u8 *ptr;

	ptr = &((u8 *)(nrf91_common.scb + scb_shp1))[excpn - 4];

	return *ptr >> 4;
}


/* CPU info */


unsigned int _nrf91_cpuid(void)
{
	return *(nrf91_common.scb + scb_cpuid);
}


void _nrf91_init(void)
{
	nrf91_common.scb = (void *)0xe000e000;
	nrf91_common.power = (void *)0x50005000;
	nrf91_common.clock = (void *)0x50005000;
	nrf91_common.gpio = (void *)0x50842500;

	/* Based on nRF9160 product specification there is fixed cpu frequency */
	nrf91_common.cpuclk = 64 * 1000 * 1000;

	/* Enable low power mode */
	*(nrf91_common.power + power_tasks_lowpwr) = 1u;
	hal_cpuDataMemoryBarrier();

	/* Disable all power interrupts */
	*(nrf91_common.power + power_intenclr) = 0x64u;

	/* Disable all clock interrupts */
	*(nrf91_common.power + power_intenclr) = 0x3u;

	hal_cpuDataMemoryBarrier();

	*(nrf91_common.clock + clock_tasks_hfclkstart) = 1u;
	/* Wait until HXFO start and clear event flag */
	while (*(nrf91_common.clock + clock_hfclkrun) != 1u) {
	}
	*(nrf91_common.clock + clock_hfclkrun) = 0u;
	hal_cpuDataMemoryBarrier();

	/* Enable UsageFault, BusFault and MemManage exceptions */
	*(nrf91_common.scb + scb_shcsr) |= (1u << 16) | (1u << 17) | (1u << 18);

	/* Disable FPU */
	*(nrf91_common.scb + fpu_cpacr) = 0u;
	*(nrf91_common.scb + fpu_fpccr) = 0u;
}
