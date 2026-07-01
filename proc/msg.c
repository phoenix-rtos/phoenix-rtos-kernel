/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"

#include "syscalls.h"


#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1U))
#define CEIL(x)  (((x) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
}
