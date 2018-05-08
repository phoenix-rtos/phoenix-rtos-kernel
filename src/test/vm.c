/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for VM subsystem
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2005-2006 Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../lib/lib.h"
#include "../vm/vm.h"
#include "../proc/proc.h"
#include "vm.h"


lock_t lock;


void test_vm_alloc(void)
{
	cycles_t b = 0, e, dmax = 0, dmin = (cycles_t)-1;
	page_t *p;
	unsigned int n, seed = 1234456, minsize = (unsigned int)-1, maxsize = 0;
	size_t size;

	lib_printf("test: Page allocator test\n");

	hal_cpuGetCycles(&b);
 	seed = (unsigned int)b;

	for (n = 0; n < 1000000; n++) {

		size = lib_rand(&seed) % (1 << 22);
		minsize = min(minsize, size);
		maxsize = max(maxsize, size);

		hal_cpuGetCycles(&b);
		p = vm_pageAlloc(size, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP);
		hal_cpuGetCycles(&e);

		if (p == NULL) {
			lib_printf("test: Out of memory!");
			break;
		}

 		_page_free(p);

 		lib_printf("\rtest: size=%d, n=%d", size, n);

		if (e - b > dmax)
			dmax = e - b;
		if (e - b < dmin)
			dmin = e - b;
	}

	lib_printf("\n");
	lib_printf("test: n=%d, dmax=%u, dmin=%u, size=%d:%d\n", n, (u32)dmax, (u32)dmin, minsize, maxsize);
	lib_printf("test: ");
	_page_showPages();
	return;
}


void test_vm_mmap(void)
{
	vm_map_t map;

	lib_printf("test: Virtual memory map test\n");
	vm_mmap(&map, (void *)0x123, NULL, SIZE_PAGE, 0, NULL, 0, 0);

	vm_mapDump(&map);
	return;
}


void test_vm_zalloc(void)
{
	vm_zone_t zone;
	void *b;

	lib_printf("test: Zone allocator test\n");

	_vm_zoneCreate(&zone, 128, 1024);

	for (;;) {
		if ((b = _vm_zalloc(&zone, NULL)) == NULL)
			break;

		lib_printf("\rtest: b=%p", b);
	}
	lib_printf("\n");

	return;
}


void test_vm_kmalloc(void)
{
	char *buff[150];
	int i, k;
	size_t kmallocsz, mapallocsz, freesz;
	cycles_t c = 0;
	unsigned int s1, s2, size;

	vm_kmallocGetStats(&kmallocsz);
	vm_mapGetStats(&mapallocsz);
	vm_pageGetStats(&freesz);

	lib_printf("test: Testing kmalloc,   kmalloc=%d, map=%d, free=%dKB\n", kmallocsz, mapallocsz, freesz / 1024);

	hal_cpuGetCycles(&c);
 	s1 = (unsigned int)c;
	s2 = s1 / 2;

	for (i = 0; i < sizeof(buff) / sizeof(buff[0]); i++)
		buff[i] = NULL;

//vm_mapDumpArenas();

	for (k = 0; k < 1000; k++) {
		size = lib_rand(&s1) % (4 * 1024);
		i = lib_rand(&s2) % (sizeof(buff) / sizeof(buff[0]));

		if (buff[i] != NULL) {
			vm_kfree(buff[i]);
			buff[i] = NULL;
		}

		lib_printf("\rtest: [%4d] allocating %5d", k, size);
 		buff[i] = vm_kmalloc(size);
	}
	lib_printf("\n");

	for (i = 0; i < sizeof(buff) / sizeof(buff[0]); i++) {
		if (buff[i] != NULL)
			vm_kfree(buff[i]);
	}

	vm_kmallocGetStats(&kmallocsz);
	vm_mapGetStats(&mapallocsz);
	vm_pageGetStats(&freesz);
	lib_printf("test: Memory after test, kmalloc=%d, map=%d, free=%dKB\n", kmallocsz, mapallocsz, freesz / 1024);

//vm_mapDumpArenas();

	for(;;);
}


static void _test_vm_msgsimthr(void *arg)
{
	char *buff;

	for (;;) {
		if ((buff = vm_kmalloc(44)) == NULL) {
			break;
		}
		hal_memset(buff, 2, 44);
		vm_kfree(buff);
		proc_threadSleep(10000);
	}

	proc_lockSet(&lock);
	lib_printf("test: M, No memory!\n");
	proc_lockClear(&lock);

	for (;;);
}


static void _test_vm_upgrsimthr(void *arg)
{
	char *first, *buff;
	unsigned int i;
	size_t allocsz;

	vm_kmallocGetStats(&allocsz);
	proc_lockSet(&lock);
	lib_printf("test: Simulate kmalloc load [%d]\n", allocsz);
	proc_lockClear(&lock);

//vm_kmallocDump();
//vm_mapDump(NULL);

	for (;;) {
		if ((first = vm_kmalloc(3000)) == NULL)
			break;

		hal_memset(first, 1, 133);

		for (i = 0; i < 10000; i++) {
			vm_kmallocGetStats(&allocsz);
			proc_lockSet(&lock);
			lib_printf("\rtest: U, [%4d] kmalloc.allocsz=%d", i, allocsz);
			proc_lockClear(&lock);

			if ((buff = vm_kmalloc(3000)) == NULL)
				break;
			hal_memset(buff, 0, 133);
			vm_kfree(buff);
			proc_threadSleep(1000);
		}
		vm_kfree(first);

		break;
	}

lib_printf("\n");
//vm_kmallocDump();
//vm_mapDump(NULL);

	vm_kmallocGetStats(&allocsz);
	proc_lockSet(&lock);
	lib_printf("test: U, No memory [%d]!\n", allocsz);
	proc_lockClear(&lock);

	for(;;);
}


void test_vm_kmallocsim(void)
{
	unsigned int i;
	proc_lockInit(&lock);

	proc_threadCreate(0, _test_vm_upgrsimthr, NULL, 0, 512, 0, 0, 0);

	for (i = 0; i < 16; i++)
		proc_threadCreate(0, _test_vm_msgsimthr, NULL, 0, 512, 0, 0, 0);
}
