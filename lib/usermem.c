/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - user memory CPU faults handling
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "lib/usermem.h"
#include "include/errno.h"


int usermem_memcpy(void *dst, const void *src, size_t l)
{
	USERMEM_TRY(
			{
				hal_memcpy(dst, src, l);
			},
			{
				return -EFAULT;
			});

	return EOK;
}


int usermem_strnlen(const char *str, size_t n)
{
	volatile size_t len = 0;
	volatile int err = EOK;
	size_t m = n;

#ifdef NOMMU
	const syspage_map_t *map = syspage_mapAddrResolve((addr_t)str);
	if (map == NULL) {
		return -EFAULT;
	}
	if ((map->end != 0U) && ((((addr_t)str + n) > map->end) || (((addr_t)str + n) < (addr_t)str))) {
		m = map->end - (addr_t)str;
	}
	/* parasoft-suppress-next-line MISRAC2012-RULE_14_3-ac "Overflow check" */
	else if ((map->end == 0U) && (((addr_t)str + (addr_t)n) < (addr_t)str)) {
		m = (addr_t)(-1) - (addr_t)str + 1U;
	}
	else {
		/* No action required */
	}
#else
	if ((ptr_t)str + n < (ptr_t)str) {
		return -EFAULT;
	}
	if ((ptr_t)str < VADDR_USR_MAX && (ptr_t)str + n > VADDR_USR_MAX) {
		m = VADDR_USR_MAX - (ptr_t)str;
	}
#endif
	USERMEM_TRY(
			{
				/* FIXME: introduce hal_strnlen */
				while (str[len] != '\0') {
					len++;
					if (len >= n) {
						err = -E2BIG;
						break;
					}
					if (len >= m) {
						err = -EFAULT;
						break;
					}
				}
			},
			{
				err = -EFAULT;
			});


	return (err == EOK) ? (int)len : err;
}


int usermem_strndup(const char *str, size_t n, char **duplicate)
{
	int err;
	size_t len;

	err = usermem_strnlen(str, n);

	if (err < 0) {
		return err;
	}
	len = (size_t)err + 1U; /* Include null terminator */

	*duplicate = vm_kmalloc(len);
	if (*duplicate == NULL) {
		return -ENOMEM;
	}

	err = usermem_memcpy(*duplicate, str, len);
	if (err < 0) {
		vm_kfree(*duplicate);
		return err;
	}

	if ((*duplicate)[len - 1U] != '\0') {
		vm_kfree(*duplicate);
		return -EFAULT;
	}

	return (int)len;
}
