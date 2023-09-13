/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX system files
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_FILE_H_
#define _PHOENIX_POSIX_FILE_H_


enum { otDir = 0, otFile, otDev, otSymlink, otUnknown };


enum { atMode = 0, atUid, atGid, atSize, atType, atPort, atPollStatus, atEventMask, atCTime, atMTime, atATime, atLinks, atDev };


enum { mtMount = 0xf50, mtUmount, mtSync };


typedef struct {
	long id;
	unsigned int mode;
	char fstype[16];
} mount_msg_t;


#endif
