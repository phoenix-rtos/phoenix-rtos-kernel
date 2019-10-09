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

#include HAL
#include "../../include/errno.h"
#include "proc.h"
#include "event.h"


int _proc_init(vm_map_t *kmap, vm_object_t *kernel)
{
	_threads_init(kmap, kernel);
	_process_init(kmap, kernel);
	_port_init();
	_msg_init(kmap, kernel);
	_userintr_init();
	_file_init();
	_event_init();

	return EOK;
}
