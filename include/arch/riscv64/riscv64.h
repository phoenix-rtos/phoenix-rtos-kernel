/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * RISCV64 basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_RISCV64_H_
#define _PHOENIX_ARCH_RISCV64_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_reboot = 0 } type;

	union {
		struct {
			unsigned int magic;
		} reboot;
	} task;
} __attribute__((packed)) platformctl_t;

/* clang-format on */


#endif
