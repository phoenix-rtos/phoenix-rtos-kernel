/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2012-2015, 2017 Phoenix Systems
 * Copyright 2001, 2006-2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Jacek Popko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_PROC_H_
#define _PROC_PROC_H_

#include "hal/hal.h"
#include "threads.h"
#include "process.h"
#include "lock.h"
#include "msg.h"
#include "name.h"
#include "resource.h"
#include "mutex.h"
#include "cond.h"
#include "userintr.h"
#include "ports.h"
#include "futex.h"

extern int _proc_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
