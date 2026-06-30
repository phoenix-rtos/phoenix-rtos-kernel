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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_IA32_CONSOLE_H_
#define _PH_HAL_IA32_CONSOLE_H_


#ifdef HAL_CONSOLE_VGA


void hal_consoleVGAPrint(int attr, const char *s);


void hal_consoleVGAPutch(char c);


void _hal_consoleVGAInit(void);


#endif


#ifdef HAL_CONSOLE_SERIAL


void hal_consoleSerialPrint(int attr, const char *s);


void hal_consoleSerialPutch(char c);


void _hal_consoleSerialInit(void);


#endif


#endif
