/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GR716 basic peripherals control functions
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_GR716_H_
#define _PHOENIX_ARCH_GR716_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

enum { cgu_primary = 0, cgu_secondary };

/* Clock gating unit (primary) devices */
enum { cgudev_spi2ahb = 0, cgudev_i2c2ahb, cgudev_grpwrx, cgudev_grpwtx, cgudev_ftmctrl, cgudev_spimctrl0, cgudev_spimctrl1,
	cgudev_spictrl0, cgudev_spictrl1, cgudev_i2cmst0, cgudev_i2cmst1, cgudev_i2clsv0, cgudev_i2clsv1, cgudev_grdacadc,
	cgudev_grpwm1, cgudev_grpwm2, cgudev_apbuart0, cgudev_apbuart1, cgudev_apbuart2, cgudev_apbuart3, cgudev_apbuart4,
	cgudev_apbuart5, cgudev_ioDisable = 23, cgudev_l3stat, cgudev_ahbuart, cgudev_memprot, cgudev_asup, cgudev_grspwtdp,
	cgudev_spi4s, cgudev_nvram
};

/* Clock gating unit (secondary) devices */
enum { cgudev_grdmac0 = 0, cgudev_grdmac1, cgudev_grdmac2, cgudev_grdmac3, cgudev_gr1553b, cgudev_grcan0, cgudev_grcan1,
	cgudev_grspw, cgudev_grdac0, cgudev_grdac1, cgudev_grdac2, cgudev_grdac3, cgudev_gradc0, cgudev_gradc1, cgudev_gradc2,
	cgudev_gradc3, cgudev_gradc4, cgudev_gradc5, cgudev_gradc6, cgudev_gradc7, cgudev_gpioseq0, cgudev_gpioseq1
};


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
			unsigned char cgu;
			unsigned int cgudev;
		} cguctrl;

		struct {
			unsigned int magic;
		} reboot;
	};
} __attribute__((packed)) platformctl_t;

/* clang-format on */


#endif
