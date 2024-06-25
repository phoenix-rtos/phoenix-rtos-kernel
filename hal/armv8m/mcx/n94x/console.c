/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console (MCXN94x UART)
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "mcxn94x.h"

#include "hal/console.h"
#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/armv8m/armv8m.h"
#include <board_config.h>


void hal_consolePrint(int attr, const char *s)
{
	/* MCXTODO */
}


void hal_consolePutch(const char c)
{
	/* MCXTODO */
}


void _hal_consoleInit(void)
{
	/* MCXTODO */
}
