/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for i.MX RT117x
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_

#define SIZE_INTERRUPTS 217

#define TIMER_US2CYC(x) (x)
#define TIMER_CYC2US(x) (x)

#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv7m/imxrt/syspage.h"
#include "include/syspage.h"
#include "include/arch/armv7m/imxrt/11xx/imxrt1170.h"
#include "imxrt117x.h"

#define HAL_NAME_PLATFORM "NXP i.MX RT117x "

#define GPT1_BASE 0x400ec000
#define GPT2_BASE 0x400f0000
#define GPT3_BASE 0x400f4000
#define GPT4_BASE 0x400f8000
#define GPT5_BASE 0x400fc000
#define GPT6_BASE 0x40100000
#define GPT_BASE  GPT1_BASE

#define GPT1_IRQ gpt1_irq
#define GPT2_IRQ gpt2_irq
#define GPT3_IRQ gpt3_irq
#define GPT4_IRQ gpt4_irq
#define GPT5_IRQ gpt5_irq
#define GPT6_IRQ gpt6_irq
#define GPT_IRQ  GPT1_IRQ

#define GPT_BUS_CLK       pctl_clk_gpt1
#define GPT_FREQ_MHZ      24
#define GPT_OSC_PRESCALER 8
#define GPT_PRESCALER     (GPT_FREQ_MHZ / GPT_OSC_PRESCALER)
#endif

// #MPUTEST: GPIO CONFIG
#ifndef MPUTEST_PIN0
#define MPUTEST_PIN0 0  // GPIO_AD_1
#endif
#ifndef MPUTEST_PIN1
#define MPUTEST_PIN1 1  // GPIO_AD_2
#endif

enum { dr = 0,
	gdir,
	psr,
	icr1,
	icr2,
	imr,
	isr,
	edge_sel,
	dr_set = 33,
	dr_clr,
	dr_toggle };
enum { gpio_in = 0,
	gpio_out };


#define CM7_GPIO3_BASE ((volatile u32 *)0x4200C000)

#define MPUTEST_GPIO_SET(pin) *(CM7_GPIO3_BASE + dr_set) = 1u << pin
#define MPUTEST_GPIO_CLR(pin) *(CM7_GPIO3_BASE + dr_clr) = 1u << pin

#endif
