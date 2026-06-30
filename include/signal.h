/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Signals
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _PH_SIGNAL_H_
#define _PH_SIGNAL_H_

enum { signal_kill = 1, signal_segv, signal_illegal, signal_cancel = 32 };

#define NSIG 32

#endif
