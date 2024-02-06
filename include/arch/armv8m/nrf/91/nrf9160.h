/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Macros and enums for NRF9160 related code
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_NRF9160_H_
#define _PHOENIX_ARCH_NRF9160_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */
/* nRF9160 peripheral id's - same as irq numbers */
enum { spu_irq = 3, regulators_irq, clock_irq = 5, power_irq = 5, ctrlapperi_irq, spi0_irq = 8, twi0_irq = 8, uarte0_irq = 8,
spi1_irq = 9, twi1_irq = 9, uarte1_irq = 9, spi2_irq = 10, twi2_irq = 10, uarte2_irq = 10, spi3_irq = 11, twi3_irq = 11, uarte3_irq = 11,
gpiote0_irq = 13, saadc_irq, timer0_irq, timer1_irq, timer2_irq, rtc0_irq = 20, rtc1_irq, ddpic_irq = 23, wdt_irq,
egu0_irq = 27, egu1_irq, egu2_irq, egu3_irq, egu4_irq, egu5_irq, pwm0_irq, pwm1_irq, pwm2_irq, pwm3_irq, pdm_irq = 38, 
i2s_irq = 40, ipc_irq = 42, fpu_irq = 44, gpiote1_irq = 49, kmu_irq = 57, nvmc_irq = 57, vmc_irq, cc_host_rgf_irq = 64,
cryptocell_irq = 64, gpio_irq = 66 };
/* clang-format on */


typedef struct {
	enum { pctl_set = 0,
		pctl_get } action;
	enum { pctl_reboot = 0 } type;

	struct {
		unsigned int magic;
		unsigned int reason;
	} reboot;

} __attribute__((packed)) platformctl_t;


#endif
