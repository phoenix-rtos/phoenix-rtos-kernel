/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONSOLE_H_
#define _PH_HAL_CONSOLE_H_

/* Console attributes */
#define ATTR_NORMAL 0x03
#define ATTR_BOLD   0x0f
#define ATTR_USER   0x07

/* clang-format off */
#define CONSOLE_CYAN   "\033" "[36m"
#define CONSOLE_NORMAL "\033" "[0m"
#define CONSOLE_BOLD   "\033" "[1m"
/* clang-format on */


void hal_consolePrint(int attr, const char *s);


void hal_consolePutch(char c);


void _hal_consoleInit(void);

#endif
