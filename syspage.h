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

#ifndef _PH_SYSPAGE_INTERNAL_H_
#define _PH_SYSPAGE_INTERNAL_H_

#include "hal/hal.h"
#include "include/syspage.h"


/* Map's functions */

size_t syspage_mapSize(void);


const syspage_map_t *syspage_mapList(void);


const syspage_map_t *syspage_mapIdResolve(unsigned int id);


const syspage_map_t *syspage_mapAddrResolve(addr_t addr);


const syspage_map_t *syspage_mapNameResolve(const char *name);


/* Prog's functions */

size_t syspage_progSize(void);


syspage_prog_t *syspage_progList(void);


const syspage_prog_t *syspage_progIdResolve(unsigned int id);


const syspage_prog_t *syspage_progNameResolve(const char *name);


/* Scheduler configuration */

syspage_sched_window_t *syspage_schedulerWindowList(void);


/* Partition configuration */

syspage_part_t *syspage_partitionList(void);


/* General functions */

void syspage_progShow(void);


void syspage_init(void);


#endif
