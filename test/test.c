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
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

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

/* parasoft-end-suppress ALL "tests don't need to comply with MISRA" */
