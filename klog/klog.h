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
#ifndef _DMESG_H_
#define _DMESG_H_


extern int klog_write(const char *data, size_t len);


extern void _klog_init(void);


extern void _klog_initSrv(void);


#endif /* _DMESG_H_ */
