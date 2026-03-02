/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Socket defines
 *
 * Copyright 2018 Phoenix Systems
 * Author: Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SOCKDEFS_H_
#define _PH_SOCKDEFS_H_

#define SOCK_NONBLOCK 0x8000U
#define SOCK_CLOEXEC  0x4000U
#define SOCK_LARGEBUF 0x2000U

#define SOL_IPV6 41  /* IPPROTO_IPV6 */
#define SOL_RAW  255 /* IPPROTO_RAW */

#endif
