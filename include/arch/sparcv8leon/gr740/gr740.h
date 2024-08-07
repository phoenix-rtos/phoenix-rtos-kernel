/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GR740 basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_GR740_H_
#define _PHOENIX_ARCH_GR740_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

/* Clock gating unit devices */
enum { cgudev_greth0 = 0, cgudev_greth1, cgudev_spwrouter, cgudev_pci, cgudev_milStd1553, cgudev_can, cgudev_leon4stat,
	cgudev_apbuart0, cgudev_apbuart1, cgudev_spi, cgudev_promctrl };


/* Pin mux config */
enum { iomux_gpio = 0, iomux_alternateio, iomux_promio };


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_iomux = 0, pctl_cguctrl, pctl_ambapp, pctl_reboot } type;

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
			} v;
			unsigned int cgudev;
		} cguctrl;

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
