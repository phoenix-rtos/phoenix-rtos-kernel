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


extern int log_write(const char *data, size_t len);


extern void _log_start(void);


extern void _log_init(void);


#endif /* _LOG_H_ */
