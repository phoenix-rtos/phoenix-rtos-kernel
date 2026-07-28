/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * file attributes
 *
 * Copyright 2017-2018, 2024 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk, Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_FILE_H_
#define _PH_FILE_H_

/* clang-format off */

enum { atMode = 0, atUid, atGid, atSize, atBlocks, atIOBlock, atType, atPort, atPollStatus, atEventMask, atCTime,
	atMTime, atATime, atLinks, atDev };

/* clang-format on */

#endif
