/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * UNIX socket address
 *
 * Copyright 2026 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "vm/vm.h"

#include "uaddr.h"


uaddr_t *uaddr_alloc(const char *path)
{
	uaddr_t *addr;
	size_t len = hal_strlen(path) + 1U;

	addr = vm_kmalloc(sizeof(uaddr_t) + len);
	if (addr == NULL) {
		return NULL;
	}

	addr->refs = 1;
	addr->len = (socklen_t)(sizeof(sa_family_t) + len);
	hal_memcpy(addr->path, path, len);

	return addr;
}


uaddr_t *uaddr_ref(uaddr_t *addr)
{
	if (addr != NULL) {
		(void)lib_atomicIncrement(&addr->refs);
	}

	return addr;
}


void uaddr_put(uaddr_t *addr)
{
	if ((addr != NULL) && (lib_atomicDecrement(&addr->refs) == 0)) {
		vm_kfree(addr);
	}
}


void uaddr_copy(const uaddr_t *addr, struct sockaddr *address, socklen_t *address_len)
{
	sa_family_t family = (sa_family_t)AF_UNIX;
	socklen_t len = (addr != NULL) ? addr->len : (socklen_t)sizeof(family);
	socklen_t copy = min(*address_len, len);
	char *dst = (char *)address;

	if (copy > (socklen_t)sizeof(family)) {
		hal_memcpy(dst, &family, sizeof(family));
		hal_memcpy(dst + sizeof(family), addr->path, copy - sizeof(family));
	}
	else {
		hal_memcpy(dst, &family, copy);
	}

	*address_len = len;
}
