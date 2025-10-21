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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_INTERRUPTS_GICV2_H_
#define _PH_INTERRUPTS_GICV2_H_


/* Type of interrupt's configuration */
enum {
	gicv2_cfg_reserved = 0,
	gicv2_cfg_high_level = 1,
	gicv2_cfg_rising_edge = 3,
};


/* Returns one of gicv2_cfg_* for a given interrupt number. Must be implemented in platform-specific code. */
int _interrupts_gicv2_classify(unsigned int irqn);


void interrupts_setCPU(unsigned int irqn, unsigned int cpuMask);


#endif /* _INTERRUPTS_GICV2_H_ */
