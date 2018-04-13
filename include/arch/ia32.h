/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IA32 basic peripherals control functions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_IA32_H_
#define _PHOENIX_ARCH_IA32_H_

#define PCI_ANY            0
#define PCI_VENDOR_INTEL   0x8086


typedef struct {
	unsigned short vendor;
	unsigned short device;
	unsigned short subvendor;
	unsigned short subdevice;
	unsigned short cl;
} pci_id_t;


typedef struct {
	unsigned long base;
	unsigned long limit;
	unsigned char flags;
} pci_resource_t;


typedef struct {
	unsigned char b; /* bus */
	unsigned char d; /* device */
	unsigned char f; /* function */
	unsigned short device;
	unsigned short vendor;
	unsigned short status;
	unsigned short command;
	unsigned short cl; /* class and subclass */
	unsigned char progif;
	unsigned char revision;
	unsigned char irq;
	unsigned char type;
	pci_resource_t resources[6];
} pci_device_t;


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_pci = 0, pctl_busmaster } type;

	union {
		struct {
			pci_id_t id;
			pci_device_t dev;
		} pci;

		struct {
			pci_device_t dev;
			int enable;
		} busmaster;
	};
} __attribute__((packed)) platformctl_t;

#endif
