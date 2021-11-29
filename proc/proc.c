/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2012-2015, 2017 Phoenix Systems
 * Copyright 2001, 2006-2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../hal/hal.h"
#include "../include/errno.h"
#include "proc.h"


int _proc_init(vm_map_t *kmap, vm_object_t *kernel)
{
	_threads_init(kmap, kernel);
	_process_init(kmap, kernel);
	_port_init();
	_msg_init(kmap, kernel);
	_name_init();
	_userintr_init();

	return EOK;
}
