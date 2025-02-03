/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Consoles for ia32.
 *
 * Copyright 2025 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IA32_CONSOLE_H_
#define _HAL_IA32_CONSOLE_H_


#ifdef HAL_CONSOLE_VGA


extern void hal_consoleVGAPrint(int, const char *);


extern void hal_consoleVGAPutch(char);


extern void _hal_consoleVGAInit(void);


#endif


#ifdef HAL_CONSOLE_SERIAL


extern void hal_consoleSerialPrint(int, const char *);


extern void hal_consoleSerialPutch(char);


extern void _hal_consoleSerialInit(void);


#endif


#endif
