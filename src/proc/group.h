/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process groups and sessions
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_GROUP_H_
#define _PROC_GROUP_H_

extern void proc_groupLeave(process_t *process);


extern int proc_groupInit(process_t *process, process_t *parent);


extern pid_t proc_getsid(process_t *p, pid_t pid);


extern pid_t proc_getpgid(process_t *p, pid_t pid);


extern int proc_setpgid(process_t *p, pid_t pid, pid_t pgid);


extern pid_t proc_setsid(process_t *p);

#endif