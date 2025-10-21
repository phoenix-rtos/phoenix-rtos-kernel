/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IA32 basic peripherals control functions
 *
 * Copyright 2018, 2019, 2020, 2024 Phoenix Systems
 * Author: Aleksander Kaminski, Kamil Amanowicz, Lukasz Kosinski, Adam Greloch
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_ARCH_IA32_H_
#define _PH_ARCH_IA32_H_

#define PCI_ANY          0U
#define PCI_VENDOR_INTEL 0x8086U

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

typedef struct {
	unsigned short vendor;
	unsigned short device;
	unsigned short subvendor;
	unsigned short subdevice;
	unsigned short cl;
	unsigned short progif;
} __attribute__((packed)) pci_id_t;


typedef struct {
	unsigned long base;
	unsigned long limit;
	unsigned char flags;
} __attribute__((packed)) pci_resource_t;


typedef struct {
	unsigned char id;
	unsigned char next;
	unsigned char len;
	unsigned char data[];
} pci_cap_t;


typedef struct {
	/* Device ID */
	unsigned char bus;
	unsigned char dev;
	unsigned char func;

	/* Mandatory header members */
	unsigned short device;
	unsigned short vendor;
	unsigned short status;
	unsigned short command;
	unsigned short cl;
	unsigned char type;

	/* Optional header members */
	unsigned char progif;
	unsigned char revision;
	unsigned char irq;

	/* Device header */
	unsigned short subvendor;
	unsigned short subdevice;
	pci_resource_t resources[6];
} __attribute__((packed)) pci_dev_t;


typedef struct {
	pci_dev_t dev;
	short osOwned;
	short eecp;
} pci_usbownership_t;


typedef struct {
	pci_dev_t dev;
	/* clang-format off */
	enum { pci_cfg_interruptdisable, pci_cfg_memoryspace, pci_cfg_busmaster} cfg;
	/* clang-format on */
	short enable;
} pci_pcicfg_t;


typedef struct {
	/* clang-format off */
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_pci = 0, pctl_pcicfg, pctl_usbownership, pctl_reboot, pctl_graphmode } type;
	/* clang-format on */

	union {
		struct {
			pci_id_t id;
			pci_dev_t dev;
			void *caps;
		} pci;

		pci_pcicfg_t pcicfg;

		pci_usbownership_t usbownership;

		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;

		struct {
			unsigned short width;
			unsigned short height;
			unsigned short bpp;
			unsigned short pitch;
			unsigned long framebuffer; /* addr_t */
		} graphmode;
	};
} platformctl_t;


#endif
