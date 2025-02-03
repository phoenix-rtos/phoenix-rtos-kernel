/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console
 *
 * Copyright 2025 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/console.h"
#include "hal/ia32/console.h"


void hal_consolePrint(int attr, const char *s)
{
#ifdef HAL_CONSOLE_VGA
	hal_consoleVGAPrint(attr, s);
#endif
#ifdef HAL_CONSOLE_SERIAL
	hal_consoleSerialPrint(attr, s);
#endif
}


void hal_consolePutch(char c)
{
#ifdef HAL_CONSOLE_VGA
	hal_consoleVGAPutch(c);
#endif
#ifdef HAL_CONSOLE_SERIAL
	hal_consoleSerialPutch(c);
#endif
}


__attribute__((section(".init"))) void _hal_consoleInit(void)
{
#ifdef HAL_CONSOLE_VGA
	_hal_consoleVGAInit();
#endif
#ifdef HAL_CONSOLE_SERIAL
	_hal_consoleSerialInit();
#endif
}
