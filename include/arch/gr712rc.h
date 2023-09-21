/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GR712RC basic peripherals control functions
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_GR712RC_H_
#define _PHOENIX_ARCH_GR712RC_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

/* Clock gating unit devices */

enum { cgudev_eth = 0, cgudev_spw0, cgudev_spw1, cgudev_spw2, cgudev_spw3, cgudev_spw4, cgudev_spw5, cgudev_can,
	cgudev_ccsdsEnc = 9, cgudev_ccsdsDec, cgudev_milStd1553 };


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_iomux = 0, pctl_cguctrl, pctl_reboot } type;

	union {
		struct {
			unsigned char pin;
			unsigned char opt;
			unsigned char pullup;
			unsigned char pulldn;
		} iocfg;

		struct {
			union {
				enum { disable = 0, enable } state;
				int stateVal;
			};
			unsigned int cgudev;
		} cguctrl;

		struct {
			unsigned int magic;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;

/* clang-format on */


#endif
