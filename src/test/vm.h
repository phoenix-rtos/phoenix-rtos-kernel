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

#ifndef _TEST_VM_H_
#define _TEST_VM_H_

struct _vm_map_t;


struct _vm_object_t;


extern void test_vm_alloc(void);


extern void test_vm_anons(struct _vm_map_t *kmap, struct _vm_object_t *kernel);


extern void test_vm_mmap(void);


extern void test_vm_zalloc(void);


extern void test_vm_kmalloc(void);


extern void test_vm_kmallocsim(void);


#endif
