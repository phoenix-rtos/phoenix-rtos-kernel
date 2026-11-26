/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling for ARM GIC v1 or v2
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_INTERRUPTS_GICV2_H_
#define _PH_INTERRUPTS_GICV2_H_


#define SGI_FLT_USE_LIST   0 /* Send SGI to CPUs according to targetList */
#define SGI_FLT_OTHER_CPUS 1 /* Send SGI to all CPUs except the one that called this function */
#define SGI_FLT_THIS_CPU   2 /* Send SGI to the CPU that called this function */


/* Type of interrupt's configuration */
enum {
	gicv2_cfg_reserved = 0,
	gicv2_cfg_high_level = 1,
	gicv2_cfg_rising_edge = 3,
};


/* Returns one of gicv2_cfg_* for a given interrupt number. Must be implemented in platform-specific code. */
int _interrupts_gicv2_classify(unsigned int irqn);


void interrupts_setCPU(unsigned int irqn, unsigned int cpuID);

void interrupts_enableIRQ(unsigned int irqn);

void hal_cpuSendSGI(u8 targetFilter, u8 targetList, u8 intID);


#endif /* _INTERRUPTS_GICV2_H_ */
