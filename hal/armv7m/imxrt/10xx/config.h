/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for i.MX RT10xx
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

#define SIZE_INTERRUPTS 167

#define TIMER_US2CYC(x) (x)
#define TIMER_CYC2US(x) (x)

#ifndef __ASSEMBLY__
#include "include/arch/armv7m/imxrt/syspage.h"
#include "include/arch/armv7m/imxrt/10xx/imxrt10xx.h"
#include "imxrt10xx.h"

#define HAL_NAME_PLATFORM "NXP i.MX RT10xx "

#define GPT1_BASE 0x401ec000
#define GPT2_BASE 0x401f0000
#define GPT_BASE  GPT2_BASE

#define GPT1_IRQ gpt1_irq
#define GPT2_IRQ gpt2_irq
#define GPT_IRQ  GPT2_IRQ

#define GPT_BUS_CLK       pctl_clk_gpt2_bus
#define GPT_FREQ_MHZ      24
#define GPT_OSC_PRESCALER 8
#define GPT_PRESCALER     (GPT_FREQ_MHZ / GPT_OSC_PRESCALER)
#endif

#endif
