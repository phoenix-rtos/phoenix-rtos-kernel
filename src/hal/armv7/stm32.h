/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32 basic peripherals control functions
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_STM32_H_
#define _HAL_STM32_H_

#include "cpu.h"
#include "pmap.h"
#include "spinlock.h"
#include "../../../include/arch/stm32l1.h"


enum { ahb_begin = pctl_gpioa, ahb_end = pctl_fsmc, apb2_begin = pctl_syscfg, apb2_end = pctl_usart1,
	apb1_begin = pctl_tim2, apb1_end = pctl_comp, misc_begin = pctl_rtc, misc_end = pctl_hsi };


/* STM32 Interrupt numbers */
enum { wwdq_irq = 16, pvd_irq, tamper_stamp_irq, rtc_wkup_irq, flash_irq, rcc_irq,
	exti0_irq, exti1_irq, exti2_irq, exti3_irq, exti4_irq, dma1ch1_irq, dma1ch2_irq,
	dma1ch3_irq, dma1ch4_irq, dma1ch5_irq, dma1ch6_irq, dma1ch7_irq, adc1_irq,
	usbhp_irq, usblp_irq, dac_irq, comp_irq, exti9_5_irq, lcd_irq, tim9_irq, tim10_irq,
	tim11_irq, tim2_irq, tim3_irq, tim4_irq, i2c1_ev_irq, i2c1_er_irq, i2c2_ev_irq,
	i2c2_er_irq, spi1_irq, spi2_irq, usart1_irq, usart2_irq, usart3_irq, exti15_10_irq,
	rtc_alrm_irq, usb_fs_wkup_irq, tim6_irq, tim7_irq, sdio_irq, tim5_irq, spi3_irq,
	uart4_irq, uart5_irq, dma2ch1_irq, dma2ch2_irq, dma2ch3_irq, dma2ch4_irq, dma2ch5_irq,
	comp_acq_irq = 72 };


/* platformctl syscall */
extern int hal_platformctl(void *);


extern void _stm32_platformInit(void);


/* Sets peripheral clock */
extern int _stm32_rccSetDevClock(unsigned int d, u32 hz);


/* Sets CPU clock to the closest smaller MSI freqency */
extern int _stm32_rccSetCPUClock(u32 hz);


extern int _stm32_rccGetDevClock(unsigned int d, u32 *hz);


extern u32 _stm32_rccGetCPUClock(void);


extern void _stm32_rccClearResetFlags(void);


extern int _stm32_rccIsIWDGResetFlag(void);


extern int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd);


extern int _stm32_gpioSet(unsigned int d, u8 pin, u8 val);


extern int _stm32_gpioSetPort(unsigned int d, u16 val);


extern int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val);


extern int _stm32_gpioGetPort(unsigned int d, u16 *val);


/* Range = 0 - forbidden, 1 - 1.8V, 2 - 1.5V, 3 - 1.2V */
extern void _stm32_pwrSetCPUVolt(u8 range);


extern void _stm32_pwrEnterLPRun(u32 state);


extern int _stm32_pwrEnterLPStop(void);


extern void _stm32_rtcUnlockRegs(void);


extern void _stm32_rtcLockRegs(void);


extern void _stm32_rtcSetAlarm(u32 ms);


extern u32 _stm32_rtcGetms(void);


extern void _stm32_scbSetPriorityGrouping(u32 group);


extern u32 _stm32_scbGetPriorityGrouping(void);


extern void _stm32_scbSetPriority(s8 excpn, u32 priority);


extern u32 _stm32_scbGetPriority(s8 excpn);


extern void _stm32_nvicSetIRQ(s8 irqn, u8 state);


extern u32 _stm32_nvicGetPendingIRQ(s8 irqn);


extern void _stm32_nvicSetPendingIRQ(s8 irqn, u8 state);


extern u32 _stm32_nvicGetActive(s8 irqn);


extern void _stm32_nvicSetPriority(s8 irqn, u32 priority);


extern u8 _stm32_nvicGetPriority(s8 irqn);


extern void _stm32_nvicSystemReset(void);


extern int _stm32_extiMaskInterrupt(u32 line, u8 state);


extern int _stm32_extiMaskEvent(u32 line, u8 state);


extern int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge);


extern int _stm32_syscfgExtiLineConfig(u8 port, u8 pin);


extern int _stm32_extiSoftInterrupt(u32 line);


extern u32 _stm32_extiGetPending(void);


extern int _stm32_extiClearPending(u32 line);


extern int _stm32_systickInit(u32 interval);


extern void _stm32_systickSet(u8 state);


extern u32 _stm32_systickGet(void);


extern void _stm32_mpuReadRegion(u8 region, mpur_t *reg);


extern void _stm32_mpuEnableRegion(u8 region, u8 state);


extern void _stm32_mpuUpdateRegion(mpur_t *reg);


extern unsigned int _stm32_cpuid(void);


extern void _stm32_wdgReload(void);


extern int _stm32_systickInit(u32 interval);


extern void _stm32_init(void);


#endif
