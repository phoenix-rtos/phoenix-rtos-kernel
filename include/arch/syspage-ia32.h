/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for ia32
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Lukasz Kosinski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSPAGE_IA32_H_
#define _PHOENIX_SYSPAGE_IA32_H_

#define ACPI_NONE 0
#define ACPI_RSDP 1
#define ACPI_XSDP 2

typedef struct {
	struct {
		unsigned short size;
		unsigned int addr;
	} __attribute__((packed)) gdtr;
	unsigned short pad1;
	struct {
		unsigned short size;
		unsigned int addr;
	} __attribute__((packed)) idtr;
	unsigned short pad2;
	unsigned int pdir;
	unsigned int ptable;
	unsigned int stack;
	unsigned int stacksz;

	unsigned int ebda;
	unsigned int acpi_version;
	unsigned int localApicAddr;
	unsigned int madt;
	unsigned int madtLength;
	unsigned int fadt;
	unsigned int fadtLength;
} __attribute__((packed)) hal_syspage_t;


#endif
