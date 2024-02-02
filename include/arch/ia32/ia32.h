/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IA32 basic peripherals control functions
 *
 * Copyright 2018, 2019, 2020 Phoenix Systems
 * Author: Aleksander Kaminski, Kamil Amanowicz, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_IA32_H_
#define _PHOENIX_ARCH_IA32_H_

#define PCI_ANY            0
#define PCI_VENDOR_INTEL   0x8086

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

typedef struct {
	unsigned short vendor;
	unsigned short device;
	unsigned short subvendor;
	unsigned short subdevice;
	unsigned short cl;
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
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_pci = 0, pctl_busmaster, pctl_reboot } type;

	union {
		struct {
			pci_id_t id;
			pci_dev_t dev;
			void* caps;
		} pci;

		struct {
			pci_dev_t dev;
			int enable;
		} busmaster;

		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
	};
} platformctl_t;


#endif
