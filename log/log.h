/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Kernel log buffer
 *
 * Copyright 2021 Phoenix Systems
 * Author: Maciej Purski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#ifndef _LOG_H_
#define _LOG_H_

#include "proc/msg.h"


size_t log_write(const char *data, size_t len);


/* Has to be called after log_write to wakeup klog readers.
 * Not safe - performs i.a. proc_respond! */
void log_scrub(void);


/* Same as log_scrub, but give up if lock is taken */
void log_scrubTry(void);


/* Bypass log, change log_write mode to writing directly to the console
 * Debug feature, allows direct and instant message printing */
void log_disable(void);


void log_msgHandler(msg_t *msg, oid_t oid, msg_rid_t rid);


void _log_init(void);


#endif /* _LOG_H_ */
