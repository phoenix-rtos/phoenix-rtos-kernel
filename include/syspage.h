/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Syspage
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_H_
#define _PH_SYSPAGE_H_


enum { mAttrRead = 0x01, mAttrWrite = 0x02, mAttrExec = 0x04, mAttrShareable = 0x08,
	   mAttrCacheable = 0x10, mAttrBufferable = 0x20 };


enum { console_default = 0, console_com0, console_com1, console_com2, console_com3, console_com4, console_com5, console_com6,
	   console_com7, console_com8, console_com9, console_com10, console_com11, console_com12, console_com13, console_com14,
	   console_com15, console_vga0 };


typedef struct _mapent_t {
	struct _mapent_t *next, *prev;
	enum { hal_entryReserved = 0, hal_entryTemp, hal_entryAllocated, hal_entryInvalid } type;

	addr_t start;
	addr_t end;
} __attribute__((packed)) mapent_t;


typedef struct _syspage_sched_window_t {
	struct _syspage_sched_window_t *next, *prev;

	time_t stop;
	unsigned char id;
} __attribute__((packed)) syspage_sched_window_t;


typedef struct _syspage_part_t {
	struct _syspage_part_t *next, *prev;

	char *name;

	size_t allocMapSz;
	unsigned char *allocMaps;

	size_t accessMapSz;
	unsigned char *accessMaps;

	unsigned int schedWindowsMask;

	hal_syspage_part_t hal;
} syspage_part_t;


typedef struct _syspage_prog_t {
	struct _syspage_prog_t *next, *prev;

	addr_t start;
	addr_t end;

	syspage_part_t *partition;

	char *argv;

	size_t imapSz;
	unsigned char *imaps;

	size_t dmapSz;
	unsigned char *dmaps;
} __attribute__((packed)) syspage_prog_t;


typedef struct _syspage_map_t {
	struct _syspage_map_t *next, *prev;

	mapent_t *entries;

	addr_t start;
	addr_t end;

	unsigned int attr;
	unsigned char id;

	char *name;
} __attribute__((packed)) syspage_map_t;


typedef struct {
	hal_syspage_t hs; /* Specific syspage structure defines per architecture */
	size_t size;      /* Syspage size                                        */

	addr_t pkernel; /* Physical address of kernel's beginning */

	syspage_map_t *maps;   /* Maps list    */
	syspage_part_t *partitions; /* Partitions list*/
	syspage_sched_window_t *schedWindows;
	syspage_prog_t *progs; /* Programs list*/

	unsigned int console; /* Console ID defines in hal */
} __attribute__((packed)) syspage_t;

#endif
