/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * MPS3 AN536 basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_ARCH_MPS3AN536_H_
#define _PH_ARCH_MPS3AN536_H_


#define PCTL_REBOOT_MAGIC 0xaa55aa55UL


typedef struct {
	/* clang-format off */
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_reboot = 0 } type;
	/* clang-format on */

	union {
		struct {
			unsigned int magic;
		} reboot;
	} task;
} __attribute__((packed)) platformctl_t;


#endif
