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

#ifndef _SYSPAGE_H_
#define _SYSPAGE_H_

#include "hal/hal.h"
#include "include/syspage.h"


/* Map's functions */

extern size_t syspage_mapSize(void);


extern const syspage_map_t *syspage_mapList(void);


extern const syspage_map_t *syspage_mapIdResolve(unsigned int id);


extern const syspage_map_t *syspage_mapAddrResolve(addr_t addr);


extern const syspage_map_t *syspage_mapNameResolve(const char *name);


/* Prog's functions */

extern size_t syspage_progSize(void);


extern syspage_prog_t *syspage_progList(void);


extern const syspage_prog_t *syspage_progIdResolve(unsigned int id);


extern const syspage_prog_t *syspage_progNameResolve(const char *name);


/* General functions */

extern void syspage_progShow(void);


extern void syspage_init(void);


#endif
