/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * LEON3 Generic basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_GENERIC_H_
#define _PHOENIX_ARCH_GENERIC_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_iomux = 0, pctl_ambapp, pctl_reboot } type;

	union {
		struct {
			unsigned char pin;
			unsigned char opt;
			unsigned char pullup;
			unsigned char pulldn;
		} iocfg;

		struct {
			struct _ambapp_dev_t *dev;
			unsigned int *instance;
		} ambapp;

		struct {
			unsigned int magic;
		} reboot;
	} task;
} __attribute__((packed)) platformctl_t;

/* clang-format on */


#endif
