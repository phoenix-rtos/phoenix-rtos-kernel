/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * posixsrv's events
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_EVENTS_H_
#define _PHOENIX_EVENTS_H_

/* clang-format off */

enum { evAdd = 0x1, evDelete = 0x2, evEnable = 0x4, evDisable = 0x8, evOneshot = 0x10, evClear = 0x20, evDispatch = 0x40 };

/* clang-format on */

#endif
