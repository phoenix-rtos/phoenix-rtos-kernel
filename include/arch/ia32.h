/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IA32 basic peripherals control functions
 *
 * Copyright 2018, 2019 Phoenix Systems
 * Author: Aleksander Kaminski, Kamil Amanowicz
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
	unsigned short class_code;
} pci_id_t;


typedef struct {
	unsigned long base;
	unsigned long limit;
	unsigned char flags;
} pci_resource_t;


/* capability list entry */
typedef struct {
	unsigned char id;
	unsigned char next;
	unsigned char len;
	unsigned char data[];
} pci_cap_t;


/* capability list */
typedef struct {
	unsigned char data[192];
} pci_cap_list_t;


typedef struct {
	/* pci device id combo */
	unsigned char bus;
	unsigned char device;
	unsigned char function;

	/* mandatory header members */
	unsigned short device_id;
	unsigned short vendor_id;
	unsigned short status;
	unsigned short command;
	unsigned short class_code; /* class and subclass */
	unsigned char type; /* header type */

	/* optional header members */
	unsigned char progif; /* register level programming interface */
	unsigned char revision;
	unsigned char irq; /* irq line */
	unsigned char irq_pin;
	unsigned char bist; /* built-in self test */
	unsigned char latency_tmr; /* latency timer */
	unsigned char cache_line_size;

	/* device header */
	pci_resource_t resources[6]; /* base address registers */
	unsigned long cis_ptr; /* cardbus cis pointer */
	unsigned long exp_rom_addr; /* expansion ROM address */
	unsigned short subsystem_id;
	unsigned short subsystem_vendor_id;
	unsigned char max_latency;
	unsigned char min_grant; /* burst lenght period in 1/4 micro seconds (assuming 33 MHz clock) */
	unsigned char cap_ptr; /* capabilities list head pointer */
} pci_device_t;


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_pci = 0, pctl_busmaster } type;

	union {
		struct {
			pci_id_t id;
			pci_device_t dev;
			pci_cap_list_t *cap_list;
		} pci;

		struct {
			pci_device_t dev;
			int enable;
		} busmaster;
	};
} __attribute__((packed)) platformctl_t;

#endif
