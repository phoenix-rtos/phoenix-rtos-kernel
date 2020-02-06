/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * UNIX pipes
 *
 * Copyright 2020 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_PIPE_H_
#define _PROC_PIPE_H_

struct _pipe_t;


extern ssize_t pipe_read(struct _pipe_t *pipe, void *data, size_t size);


extern ssize_t pipe_write(struct _pipe_t *pipe, const void *data, size_t size);


extern int pipe_close(struct _pipe_t *pipe, int read, int write);


extern int pipe_closeNamed(obdes_t *obdes, int read, int write);


extern int pipe_ioctl(struct _pipe_t *pipe, unsigned cmd, const void *in_buf, size_t in_size, void *out_buf, size_t out_size);


extern int pipe_init(struct _pipe_t *pipe, size_t size);


extern int pipe_create(process_t *process, size_t size, int fds[2], int flags);


extern int proc_pipeCreate(int fds[2], int flags);


extern int pipe_get(obdes_t *obdes, struct _pipe_t **result, int flags);


extern int pipe_open(struct _pipe_t *pipe);


extern int pipe_poll(struct _pipe_t *pipe, poll_head_t *poll, wait_note_t *note);

#endif
