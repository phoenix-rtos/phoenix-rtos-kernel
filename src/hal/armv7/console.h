/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (STM32 USART)
 *
 * Copyright 2016 Phoenix Systems
 * Author: Artur Wodejko, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONSOLE_H_
#define _HAL_CONSOLE_H_

/* Console attributes */
#define ATTR_NORMAL  0x07
#define ATTR_BOLD    0x0f
#define ATTR_USER    0x01


#ifndef __ASSEMBLY__

extern void hal_consoleClear(int attr);


extern void hal_consolePrint(int attr, const char *s);


extern void _hal_consoleInit(void);

#endif

#endif
