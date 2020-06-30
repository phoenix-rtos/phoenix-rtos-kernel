/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Test subsystem
 *
 * Copyright 2017 Phoenix Systems
 * Author: Adrian Kepka
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "test.h"

#include "vm.h"
#include "proc.h"
#include "rb.h"
#include "msg.h"


void test_run(void)
{
	test_proc_threads1();
//	test_vm_alloc();
//	test_vm_kmalloc();
//	test_rb();
//	test_msg();
}
