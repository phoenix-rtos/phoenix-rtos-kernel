/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * User server
 *
 * Copyright 2022 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _USRV_H_
#define _USRV_H_

#define USRV_PORT     0
#define USRV_ID_LOG   0
#define USRV_ID_PIPES 1

/* LSB number for unit identifier in oid.id */
#define USRV_ID_BITS 4

extern void _usrv_init(void);


extern void _usrv_start(void);


#endif
