/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "syspage.h"
#include "pmap.h"

/* Syspage */
syspage_t * const syspage = (void *)(VADDR_KERNEL + 0x20);
