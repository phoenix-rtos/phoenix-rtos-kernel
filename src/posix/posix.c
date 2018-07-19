/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../proc/proc.h"

#include "posix.h"
#include "posix_private.h"

#define MAX_FD_COUNT 32

//#define TRACE(str, ...) lib_printf("posix %x: " str "\n", proc_current()->process->id, ##__VA_ARGS__)
#define TRACE(str, ...)

/* NOTE: temporarily disable locking, need to be tested */
#define proc_lockSet(x) (void)0
#define proc_lockClear(x) (void)0


struct {
	rbtree_t pid;
	lock_t lock;
	id_t fresh;
} posix_common;


static char *strrchr(const char *s, int c)
{
	const char *p = NULL;

	do {
		if (*s == c)
			p = s;
	}
	while (*(s++));

	return (char *)p;
}


void splitname(char *path, char **base, char **dir)
{
	char *slash;

	slash = strrchr(path, '/');

	if (slash == NULL) {
		*dir = ".";
		*base = path;
	}
	else if (slash == path) {
		*base = path + 1;
		*dir = "/";
	}
	else {
		*dir = path;
		*base = slash + 1;
		*slash = 0;
	}
}


static void posix_fileDeref(open_file_t *f)
{
	proc_lockSet(&f->lock);
	if (!--f->refs) {
		if (f->type != ftUnixSocket)
			proc_close(f->oid, f->status);
		proc_lockDone(&f->lock);
		vm_kfree(f);
	}
	else {
		proc_lockClear(&f->lock);
	}
}


static int posix_getOpenFile(int fd, open_file_t **f)
{
	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	if (fd < 0 || fd > p->maxfd)
		return -EBADF;

	proc_lockSet(&p->lock);
	if ((*f = p->fds[fd].file) == NULL) {
		proc_lockClear(&p->lock);
		return -EBADF;
	}
	proc_lockClear(&p->lock);

	return 0;
}


int posix_newFile(process_info_t *p, int fd)
{
	open_file_t *f;

	while (p->fds[fd].file != NULL && fd++ < p->maxfd);

	if (fd > p->maxfd)
		return -ENFILE;

	if ((f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL)
		return -ENOMEM;

	proc_lockInit(&f->lock);
	f->refs = 1;
	f->offset = 0;

	return fd;
}


static int pinfo_cmp(rbnode_t *n1, rbnode_t *n2)
{
	process_info_t *p1 = lib_treeof(process_info_t, linkage, n1);
	process_info_t *p2 = lib_treeof(process_info_t, linkage, n2);

	if (p1->process->id < p2->process->id)
		return -1;

	else if (p1->process->id > p2->process->id)
		return 1;

	return 0;
}


process_info_t *pinfo_find(unsigned int pid)
{
	process_t p;
	process_info_t pi, *r;

	p.id = pid;
	pi.process = &p;

	proc_lockSet(&posix_common.lock);
	r = lib_treeof(process_info_t, linkage, lib_rbFind(&posix_common.pid, &pi.linkage));
	proc_lockClear(&posix_common.lock);

	return r;
}


int posix_clone(int ppid)
{
	TRACE("clone(%x)", ppid);

	process_info_t *p, *pp;
	process_t *proc;
	int i;
	oid_t console;
	open_file_t *f;

	proc = proc_current()->process;

	if ((p = vm_kmalloc(sizeof(process_info_t))) == NULL)
		return -ENOMEM;

	hal_memset(&console, 0, sizeof(oid_t));
	proc_lockInit(&p->lock);

	if ((pp = pinfo_find(ppid)) != NULL) {
		TRACE("clone: got parent");
		proc_lockSet(&pp->lock);
		p->maxfd = pp->maxfd;
	}
	else {
		p->maxfd = MAX_FD_COUNT - 1;
	}

	p->process = proc;

	if ((p->fds = vm_kmalloc((p->maxfd + 1) * sizeof(fildes_t))) == NULL) {
		vm_kfree(p);
		return -ENOMEM;
	}

	if (pp != NULL) {
		hal_memcpy(p->fds, pp->fds, (pp->maxfd + 1) * sizeof(fildes_t));

		for (i = 0; i <= p->maxfd; ++i) {
			if ((f = p->fds[i].file) != NULL) {
				proc_lockSet(&f->lock);
				++f->refs;
				proc_lockClear(&f->lock);
			}
		}

		proc_lockClear(&pp->lock);
	}
	else {
		hal_memset(p->fds, 0, (p->maxfd + 1) * sizeof(fildes_t));

		for (i = 0; i < 3; ++i) {
			if ((f = p->fds[i].file = vm_kmalloc(sizeof(open_file_t))) == NULL)
				return -ENOMEM;

			proc_lockInit(&f->lock);
			f->refs = 1;
			f->offset = 0;
			f->type = ftTty;
			p->fds[i].flags = 0;
			hal_memcpy(&f->oid, &console, sizeof(oid_t));
		}

		p->fds[0].file->status = O_RDONLY;
		p->fds[1].file->status = O_WRONLY;
		p->fds[2].file->status = O_WRONLY;
	}

	proc_lockSet(&posix_common.lock);
	lib_rbInsert(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	return EOK;
}


int posix_fork()
{
	TRACE("fork()");

	int pid;

	if (!(pid = proc_vfork())) {
		proc_copyexec();
		/* Not reached */
	}

	return pid;
}


int posix_exec(void)
{
	TRACE("exec()");

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL && p->fds[fd].flags & FD_CLOEXEC) {
			posix_fileDeref(f);
			p->fds[fd].file = NULL;
		}
	}
	proc_lockClear(&p->lock);

	return 0;
}


int posix_exit(process_t *process)
{
	TRACE("exit(%x)", process->id);

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL)
			posix_fileDeref(f);
	}

	proc_lockSet(&posix_common.lock);
	lib_rbRemove(&posix_common.pid, &p->linkage);
	proc_lockClear(&posix_common.lock);

	vm_kfree(p->fds);
	proc_lockDone(&p->lock);
	vm_kfree(p);

	return 0;

}


static int posix_create(const char *filename, int type, mode_t mode, oid_t dev, oid_t *oid)
{
	TRACE("posix_create(%s, %d)", filename, mode);

	int err;
	oid_t dir;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(filename);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, filename, namelen + 1);

	splitname(name, &basename, &dirname);

	do {
		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_create(dir.port, type, mode, dev, dir, basename, oid)) < 0)
			break;

		return EOK;
	} while (0);

	vm_kfree(name);
	return err;
}


/* TODO: handle O_CREAT and O_EXCL */
int posix_open(const char *filename, int oflag, char *ustack)
{
	TRACE("open(%s, %d, %d)", filename, oflag);
	oid_t oid, dev, pipesrv;
	int fd = 0, err = 0;
	process_info_t *p;
	open_file_t *f;
	mode_t mode;

	if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
		; /* that's fine */

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&dev, 0, sizeof(oid_t));
	hal_memset(&pipesrv, -1, sizeof(oid_t));

	proc_lockSet(&p->lock);

	do {
		while (p->fds[fd].file != NULL && fd++ < p->maxfd);

		if (fd > p->maxfd || (f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			err = -EBADF;
			break;
		}

		proc_lockInit(&f->lock);
		proc_lockClear(&p->lock);

		do {
			if (proc_lookup(filename, &oid) == EOK) {
				/* pass */
			}
			else if (oflag & O_CREAT) {
				GETFROMSTACK(ustack, mode_t, mode, 2);

				if (posix_create(filename, 1 /* otFile */, mode, dev, &oid) < 0) {
					err = -EIO;
					break;
				}
			}
			else {
				err = -ENOENT;
				break;
			}

			if (oid.port != US_PORT && (err = proc_open(oid, oflag)) < 0) {
				err = -EIO;
				break;
			}

			/* TODO: truncate, append */

			proc_lockSet(&p->lock);
			p->fds[fd].flags = oflag & O_CLOEXEC ? FD_CLOEXEC : 0;
			proc_lockClear(&p->lock);

			if (!err) {
				hal_memcpy(&f->oid, &oid, sizeof(oid));
			}
			else {
				/* multiplexer, e.g. /dev/ptmx */
				f->oid.port = oid.port;
				f->oid.id = err;
			}

			f->refs = 1;

			/* TODO: check for other types */
			if (oid.port == US_PORT)
				f->type = ftUnixSocket;
			else if (oid.port == pipesrv.port)
				f->type = ftPipe;
			else
				f->type = ftRegular;

			if (oflag & O_APPEND)
				f->offset = proc_size(f->oid);
			else
				f->offset = 0;

			f->status = oflag & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC);

			return fd;
		} while (0);

		proc_lockSet(&p->lock);
		p->fds[fd].file = NULL;
		proc_lockDone(&f->lock);
		vm_kfree(f);

	} while (0);

	proc_lockClear(&p->lock);
	return set_errno(err);
}


int posix_close(int fildes)
{
	TRACE("close(%d)", fildes);
	open_file_t *f;
	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		p->fds[fildes].file = NULL;
		proc_lockClear(&p->lock);

		posix_fileDeref(f);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_read(int fildes, void *buf, size_t nbyte)
{
	TRACE("read(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	process_info_t *p;
	int rcnt;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		proc_lockClear(&p->lock);

		if (f->type == ftUnixSocket) {
			if ((rcnt = unix_recvfrom(f->oid.id, buf, nbyte, 0, NULL, NULL)) < 0)
				return set_errno(rcnt);
		}
		else if ((rcnt = proc_read(f->oid, f->offset, buf, nbyte, f->status)) < 0) {
			return set_errno(-EIO);
		}

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		return rcnt;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_write(int fildes, void *buf, size_t nbyte)
{
	TRACE("write(%d, %p, %u)", fildes, buf, nbyte);

	open_file_t *f;
	process_info_t *p;
	int rcnt;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		proc_lockClear(&p->lock);

		if (f->type == ftUnixSocket) {
			if ((rcnt = unix_sendto(f->oid.id, buf, nbyte, 0, NULL, 0)) < 0)
				return set_errno(rcnt);
		}
		if ((rcnt = proc_write(f->oid, f->offset, buf, nbyte, f->status)) < 0) {
			return set_errno(-EIO);
		}

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		return rcnt;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_dup(int fildes)
{
	TRACE("dup(%d)", fildes);

	process_info_t *p;
	int newfd = 0;
	open_file_t *f;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		while (p->fds[newfd].file != NULL && newfd++ < p->maxfd);

		if (newfd > p->maxfd)
			break;

		p->fds[newfd].file = f;
		p->fds[newfd].flags = 0;

		proc_lockSet(&f->lock);
		f->refs++;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
		return newfd;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


/* FIXME: handle fildes == fildes2 */
int _posix_dup2(process_info_t *p, int fildes, int fildes2)
{
	open_file_t *f, *f2;

	if (fildes < 0 || fildes > p->maxfd)
		return -1;

	if (fildes2 < 0 || fildes2 > p->maxfd)
		return -1;

	if ((f = p->fds[fildes].file) == NULL)
		return -1;

	if ((f2 = p->fds[fildes2].file) != NULL) {
		p->fds[fildes2].file = NULL;
		posix_fileDeref(f2);
	}

	p->fds[fildes2].file = f;
	p->fds[fildes2].flags = 0;

	proc_lockSet(&f->lock);
	f->refs++;
	proc_lockClear(&f->lock);

	return fildes2;
}


int posix_dup2(int fildes, int fildes2)
{
	TRACE("dup2(%d, %d)", fildes, fildes2);

	process_info_t *p;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	fildes2 = _posix_dup2(p, fildes, fildes2);
	proc_lockClear(&p->lock);

	return fildes2;
}


int posix_pipe(int fildes[2])
{
	TRACE("pipe(%p)", fildes);

	process_info_t *p;
	open_file_t *fi, *fo;
	oid_t oid;
	oid_t pipesrv;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&oid, 0, sizeof(oid));

	if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
		return set_errno(-ENOSYS);

	if (proc_create(pipesrv.port, pxBufferedPipe, O_RDONLY | O_WRONLY, oid, pipesrv, NULL, &oid) < 0)
		return set_errno(-EIO);

	proc_lockSet(&p->lock);

	do {
		fildes[0] = 0;
		while (p->fds[fildes[0]].file != NULL && fildes[0]++ < p->maxfd);

		fildes[1] = fildes[0] + 1;
		while (p->fds[fildes[1]].file != NULL && fildes[1]++ < p->maxfd);

		if (fildes[0] > p->maxfd || fildes[1] > p->maxfd)
			break;

		p->fds[fildes[0]].flags = p->fds[fildes[1]].flags = 0;

		if ((fo = p->fds[fildes[0]].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			/* FIXME: destroy pipe */
			break;
		}

		proc_lockInit(&fo->lock);
		hal_memcpy(&fo->oid, &oid, sizeof(oid));
		fo->refs = 1;
		fo->offset = 0;
		fo->type = ftPipe;
		fo->status = O_RDONLY;

		if ((fi = p->fds[fildes[1]].file = vm_kmalloc(sizeof(open_file_t))) == NULL) {
			p->fds[fildes[0]].file = NULL;
			proc_lockDone(&fo->lock);
			vm_kfree(fo);
			/* FIXME: destroy pipe */
			break;
		}

		proc_lockInit(&fi->lock);
		hal_memcpy(&fi->oid, &oid, sizeof(oid));
		fi->refs = 1;
		fi->offset = 0;
		fi->type = ftPipe;
		fi->status = O_WRONLY;

		proc_lockClear(&p->lock);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_mkfifo(const char *pathname, mode_t mode)
{
	TRACE("mkfifo(%s, %x)", pathname, mode);

	process_info_t *p;
	oid_t oid, file;
	oid_t pipesrv;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&oid, 0, sizeof(oid));

	if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
		return set_errno(-ENOSYS);

	if (proc_create(pipesrv.port, pxPipe, 0, oid, pipesrv, NULL, &oid) < 0)
		return set_errno(-EIO);

	/* link pipe in posix server */
	if (proc_link(oid, oid, pathname) < 0)
		return set_errno(-EIO);

	/* create pipe in filesystem */
	if (posix_create(pathname, 2 /* otDev */, mode, oid, &file) < 0)
		return set_errno(-EIO);

	proc_lockClear(&p->lock);
	return 0;
}


int posix_link(const char *path1, const char *path2)
{
	TRACE("link(%s, %s)", path1, path2);

	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(path2);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, path2, namelen + 1);

	splitname(name, &basename, &dirname);

	do {
		if ((p = pinfo_find(proc_current()->process->id)) == NULL) {
			err = -ENOSYS;
			break;
		}

		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_lookup(path1, &oid)) < 0)
			break;

		if ((err = proc_link(dir, oid, basename)) < 0)
			break;

		if (dir.port != oid.port) {
			/* Signal link to device */
			/* FIXME: refcount here? */
			if ((err = proc_link(oid, oid, path2)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return set_errno(err);
}


int posix_unlink(const char *pathname)
{
	TRACE("unlink(%s)", pathname);

	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(pathname);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, pathname, namelen + 1);

	splitname(name, &basename, &dirname);

	do {
		if ((p = pinfo_find(proc_current()->process->id)) == NULL) {
			err = -ENOSYS;
			break;
		}

		if ((err = proc_lookup(dirname, &dir)) < 0)
			break;

		if ((err = proc_lookup(pathname, &oid)) < 0)
			break;

		if ((err = proc_unlink(dir, oid, basename)) < 0)
			break;

		if (dir.port != oid.port) {
			/* Signal unlink to device */
			/* FIXME: refcount here? */
			if ((err = proc_unlink(oid, oid, pathname)) < 0)
				break;
		}

		err = EOK;
	} while (0);

	vm_kfree(name);
	return set_errno(err);
}


off_t posix_lseek(int fildes, off_t offset, int whence)
{
	TRACE("seek(%d, %d, %d)", fildes, offset, whence);

	open_file_t *f;
	process_info_t *p;
	int scnt, sz;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
		if (fildes < 0 || fildes > p->maxfd)
			break;

		if ((f = p->fds[fildes].file) == NULL)
			break;

		proc_lockClear(&p->lock);

		if (whence == SEEK_END)
			sz = proc_size(f->oid);

		proc_lockSet(&f->lock);
		switch (whence) {

		case SEEK_SET:
			f->offset = offset;
			scnt = offset;
			break;

		case SEEK_CUR:
			f->offset += offset;
			scnt = f->offset;
			break;

		case SEEK_END:
			f->offset = sz + offset;
			scnt = f->offset;
			break;

		default:
			scnt = -EINVAL;
			break;
		}
		proc_lockClear(&f->lock);

		return scnt;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_ftruncate(int fildes, off_t length)
{
	return -1;
}


int posix_fstat(int fd, struct stat *buf)
{
	TRACE("fstat(%d)", fd);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(fd, &f))) {
		switch (f->type) {
		case ftRegular:
			buf->st_mode = S_IFREG;
			break;
		case ftPipe:
		case ftFifo:
			buf->st_mode = S_IFIFO;
			break;
		case ftInetSocket:
		case ftUnixSocket:
			buf->st_mode = S_IFSOCK;
			break;
		case ftTty:
			buf->st_mode = S_IFCHR;
			break;
		default:
			buf->st_mode = 0;
			break;
		}

		buf->st_uid = 0;
		buf->st_gid = 0;
		buf->st_size = proc_size(f->oid);
		buf->st_dev = f->oid.port;
		buf->st_ino = (int)f->oid.id; /* FIXME */
	}

	return set_errno(err);
}


static int posix_fcntlDup(int fd, int fd2, int cloexec)
{
	process_info_t *p;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if (fd < 0 || fd > p->maxfd || fd2 < 0 || fd > p->maxfd)
		return -EBADF;

	proc_lockSet(&p->lock);
	while (p->fds[fd2].file != NULL && fd2++ < p->maxfd);

	if ((err = _posix_dup2(p, fd, fd2)) == fd2 && cloexec)
		p->fds[fd2].flags = FD_CLOEXEC;

	proc_lockClear(&p->lock);
	return err;
}


static int posix_fcntlSetFd(int fd, unsigned flags)
{
	process_info_t *p;
	int err = EOK;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	if (fd < 0 || fd > p->maxfd)
		return -EBADF;

	proc_lockSet(&p->lock);
	if (p->fds[fd].file != NULL)
		p->fds[fd].flags = flags;
	else
		err = -EBADF;
	proc_lockClear(&p->lock);
	return err;
}


static int posix_fcntlGetFd(int fd)
{
	process_info_t *p;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	if (fd < 0 || fd > p->maxfd)
		return -EBADF;

	proc_lockSet(&p->lock);
	if (p->fds[fd].file != NULL)
		err = p->fds[fd].flags;
	else
		err = -EBADF;
	proc_lockClear(&p->lock);
	return err;
}


static int _sock_getfl(open_file_t *f)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetFl;

	return proc_send(f->oid.port, &msg);
}


static int _sock_setfl(open_file_t *f, int val)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetFl;
	smi->send.flags = val;

	return proc_send(f->oid.port, &msg);
}


static int posix_fcntlSetFl(int fd, int val)
{
	open_file_t *f;
	int err = EOK;

	if (!(err = posix_getOpenFile(fd, &f))) {
		if (f->type == ftInetSocket)
			err = _sock_setfl(f, val);
		else
			f->status = val;
	}

	return err;
}


static int posix_fcntlGetFl(int fd)
{
	open_file_t *f;
	int err = EOK;

	if (!(err = posix_getOpenFile(fd, &f))) {
		if (f->type == ftInetSocket)
			err = _sock_getfl(f);
		else {
			err = f->status;
		}
	}

	return err;
}


int posix_fcntl(int fd, unsigned int cmd, char *ustack)
{
	TRACE("fcntl(%d, %u)", fd, cmd);

	int err = -EINVAL, fd2;
	unsigned long arg;
	int cloexec = 0;

	switch (cmd) {
	case F_DUPFD_CLOEXEC:
		cloexec = 1;
	case F_DUPFD:
		GETFROMSTACK(ustack, int, fd2, 2);
		err = posix_fcntlDup(fd, fd2, cloexec);
		break;

	case F_GETFD:
		err = posix_fcntlGetFd(fd);
		break;

	case F_SETFD:
		GETFROMSTACK(ustack, unsigned long, arg, 2);
		err = posix_fcntlSetFd(fd, arg);
		break;

	case F_GETFL:
		err = posix_fcntlGetFl(fd);
		break;

	case F_SETFL:
		GETFROMSTACK(ustack, unsigned int, arg, 2);
		err = posix_fcntlSetFl(fd, arg);
		break;

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_GETOWN:
	case F_SETOWN:
	default:
		break;
	}

	return set_errno(err);
}


int posix_ioctl(int fildes, int request, char *ustack)
{
	TRACE("ioctl(%d, %d)", fildes, request);

	open_file_t *f;
	int err;
	msg_t msg;

	if (!(err = posix_getOpenFile(fildes, &f))) {
		switch (request) {
			/* TODO: handle POSIX defined requests */
		default:
			hal_memset(&msg, 0, sizeof(msg));
			msg.type = mtDevCtl;
			GETFROMSTACK(ustack, void *, msg.i.data, 2);
			GETFROMSTACK(ustack, size_t, msg.i.size, 3);
			GETFROMSTACK(ustack, void *, msg.o.data, 4);
			GETFROMSTACK(ustack, size_t, msg.o.size, 5);

			err = proc_send(f->oid.port, &msg);
		}
	}

	return set_errno(err);
}


int posix_socket(int domain, int type, int protocol)
{
	TRACE("socket(%d, %d, %d)", domain, type, protocol);

	process_info_t *p;
	int err, fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if ((fd = posix_newFile(p, 0)) < 0)
		return set_errno(-EMFILE);

	switch (domain) {
	case AF_UNIX:
		if ((err = unix_socket(domain, type, protocol)) >= 0) {
			p->fds[fd].file->type = ftUnixSocket;
			p->fds[fd].file->oid.port = -1;
			p->fds[fd].file->oid.id = err;
		}
		break;
	case AF_INET:
		if ((err = inet_socket(domain, type, protocol)) >= 0) {
			p->fds[fd].file->type = ftInetSocket;
			p->fds[fd].file->oid.port = err;
			p->fds[fd].file->oid.id = 0;
		}
		break;
	default:
		err = -EAFNOSUPPORT;
		break;
	}

	if (err < 0)
		return set_errno(err);

	return fd;
}


int posix_accept4(int socket, struct sockaddr *address, socklen_t *address_len, int flags)
{
	TRACE("accept4(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	process_info_t *p;
	open_file_t *f;
	int err, fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if ((fd = posix_newFile(p, 0)) < 0)
		return set_errno(-EMFILE);

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			if ((err = inet_accept(f->oid.port, address, address_len)) >= 0) {
				p->fds[fd].file->type = ftInetSocket;
				p->fds[fd].file->oid.port = err;
				p->fds[fd].file->oid.id = 0;
			}
			break;
		case ftUnixSocket:
			if ((err = unix_accept(f->oid.id, address, address_len)) >= 0) {
				p->fds[fd].file->type = ftUnixSocket;
				p->fds[fd].file->oid.port = -1;
				p->fds[fd].file->oid.id = err;
			}
			break;
		default:
			err = -ENOTSOCK;
			break;
		}

		if (err >= 0 && flags) {
			if (flags & SOCK_NONBLOCK)
				f->status |= O_NONBLOCK;
			if (flags & SOCK_CLOEXEC)
				posix_fcntlSetFd(socket, FD_CLOEXEC);
		}
	}

	if (err < 0)
		return set_errno(err);

	return fd;
}


int posix_accept(int socket, struct sockaddr *address, socklen_t *address_len)
{
	return posix_accept4(socket, address, address_len, 0);
}


int posix_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	TRACE("bind(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_bind(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_bind(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	TRACE("connect(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_connect(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_connect(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getpeername(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getpeername(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_getpeername(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	TRACE("getsockname(%d, %s)", socket, address == NULL ? NULL : address->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getsockname(f->oid.port, address, address_len);
			break;
		case ftUnixSocket:
			err = unix_getsockname(f->oid.id, address, address_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	TRACE("getsockopt(%d, %d, %d)", socket, level, optname);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_getsockopt(f->oid.port, level, optname, optval, optlen);
			break;
		case ftUnixSocket:
			err = unix_getsockopt(f->oid.id, level, optname, optval, optlen);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_listen(int socket, int backlog)
{
	TRACE("listen(%d, %d)", socket, backlog);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_listen(f->oid.port, backlog);
			break;
		case ftUnixSocket:
			err = unix_listen(f->oid.id, backlog);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	TRACE("recvfrom(%d, %d, %s)", socket, length, src_addr == NULL ? NULL : src_addr->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_recvfrom(f->oid.port, message, length, flags, src_addr, src_len);
			break;
		case ftUnixSocket:
			err = unix_recvfrom(f->oid.id, message, length, flags, src_addr, src_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	TRACE("sendto(%d, %s, %d, %s)", socket, message, length, dest_addr == NULL ? NULL : dest_addr->sa_data);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_sendto(f->oid.port, message, length, flags, dest_addr, dest_len);
			break;
		case ftUnixSocket:
			err = unix_sendto(f->oid.id, message, length, flags, dest_addr, dest_len);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_shutdown(int socket, int how)
{
	TRACE("shutdown(%d, %d)", socket, how);

	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_shutdown(f->oid.port, how);
			break;
		case ftUnixSocket:
			err = unix_shutdown(f->oid.id, how);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	open_file_t *f;
	int err;

	if (!(err = posix_getOpenFile(socket, &f))) {
		switch (f->type) {
		case ftInetSocket:
			err = inet_setsockopt(f->oid.port, level, optname, optval, optlen);
			break;
		case ftUnixSocket:
			err = unix_setsockopt(f->oid.id, level, optname, optval, optlen);
			break;
		default:
			err = -ENOTSOCK;
			break;
		}
	}

	return set_errno(err);
}


/* Just a stub to support busybox' touch */
int posix_utimes(const char *filename, const struct timeval *times)
{
	oid_t oid;

	if (proc_lookup(filename, &oid) < 0)
		return set_errno(-ENOENT);

	return 0;
}


void posix_init(void)
{
	proc_lockInit(&posix_common.lock);
	lib_rbInit(&posix_common.pid, pinfo_cmp, NULL);
	unix_sockets_init();
	posix_common.fresh = 0;
}
