/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Files
 *
 * Copyright 2019 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_FILE_H_
#define _PROC_FILE_H_

#include HAL
#include "../../include/types.h"
#include "../../include/stat.h"
#include "process.h"

typedef struct _fildes_t fildes_t;


typedef struct _file_t file_t;


typedef struct stat file_stat_t;


extern int proc_fileOid(struct _process_t *process, int fd, oid_t *oid);


extern int proc_fileOpen(int dirfd, const char *path, int flags, mode_t mode);


extern int proc_fileClose(int fildes);


extern ssize_t proc_fileRead(int fildes, char *buf, size_t nbyte);


extern ssize_t proc_fileWrite(int fildes, const char *buf, size_t nbyte);


extern int proc_fileDup(int fildes, int fildes2, int flags);


extern int proc_fileLink(int fildes, const char *path, int dirfd, const char *name, int flags);


extern int proc_fileUnlink(int dirfd, const char *path, int flags);


extern int proc_fileSeek(int fildes, off_t *offset, int whence);


extern int proc_fileTruncate(int fildes, off_t length);


extern int proc_fileControl(int fildes, int cmd, long arg);


extern int proc_fileStat(int fildes, const char *path, file_stat_t *buf, int flags);


extern int proc_fileChmod(int fildes, mode_t mode);


extern int proc_fileIoctl(int fildes, unsigned long request, const char *indata, size_t insz, char *outdata, size_t outsz);


extern int proc_filesDestroy(struct _process_t *process);


extern int proc_filesSetRoot(const oid_t *oid, mode_t mode);


extern void _file_init(void);

#endif
