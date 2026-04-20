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

#include "perf/trace-ipc.h"
#include "syscalls.h"


#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;

/* TODO: move utcb init/deinit to threads */

void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
}
