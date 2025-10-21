/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for VM subsystem
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress ALL "tests don't need to comply with MISRA" */

#ifndef _TEST_VM_H_
#define _TEST_VM_H_

struct _vm_map_t;


struct _vm_object_t;


void test_vm_alloc(void);


void test_vm_mmap(void);


void test_vm_zalloc(void);


void test_vm_kmalloc(void);


void test_vm_kmallocsim(void);


#endif

/* parasoft-end-suppress ALL */
