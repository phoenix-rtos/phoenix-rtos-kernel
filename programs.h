/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Empty file holding programs
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROGRAMS_H_
#define _PROGRAMS_H_

#include HAL


typedef struct _cpio_newc_t {
	char c_magic[6];
	char c_ino[8];
	char c_mode[8];
	char c_uid[8];
	char c_gid[8];
	char c_nlink[8];
	char c_mtime[8];
	char c_filesize[8];
	char c_devmajor[8];
	char c_devminor[8];
	char c_rdevmajor[8];
	char c_rdevminor[8];
	char c_namesize[8];
	char c_check[8];
	char name[];
} cpio_newc_t;


extern int programs_decode(vm_map_t *kmap, vm_object_t *kernel);


#endif
