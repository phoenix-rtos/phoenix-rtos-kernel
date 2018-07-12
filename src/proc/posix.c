/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility module
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski, Michal Miroslaw
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "threads.h"
#include "cond.h"
#include "resource.h"
#include "name.h"
#include "posix.h"

#define MAX_FD_COUNT 32

#define set_errno(x) (((x) < 0) ? -1 : 0)

//#define TRACE(str, ...) lib_printf("posix %x: " str "\n", proc_current()->process->id, ##__VA_ARGS__)
#define TRACE(str, ...)


enum { ftRegular, ftPipe, ftFifo, ftInetSocket, ftUnixSocket, ftTty };

enum { pxBufferedPipe, pxPipe, pxPTY };


typedef struct {
	oid_t oid;
	unsigned refs;
	off_t offset;
	unsigned status;
	lock_t lock;
	char type;
} open_file_t;


typedef struct {
	open_file_t *file;
	unsigned flags;
} fildes_t;


typedef struct {
	rbnode_t linkage;
	process_t *process;

	lock_t lock;
	int maxfd;
	fildes_t *fds;
} process_info_t;


struct {
	rbtree_t pid;
	lock_t lock;
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


static void splitname(char *path, char **base, char **dir)
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
	while (proc_lockSet(&f->lock) < 0);
	if (!--f->refs) {
		proc_close(f->oid, f->status);
		proc_lockDone(&f->lock);
		vm_kfree(f);
	}
	else {
		proc_lockClear(&f->lock);
	}
}


static int posix_newFile(process_info_t *p, int fd)
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
	int pid;

	if (!(pid = proc_vfork())) {
		proc_copyexec();
		/* Not reached */
	}

	return pid;
}


int posix_exec(void)
{
	TRACE("exec");

	process_info_t *p;
	open_file_t *f;
	int fd;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	for (fd = 0; fd <= p->maxfd; ++fd) {
		if ((f = p->fds[fd].file) != NULL && p->fds[fd].flags & O_CLOEXEC) {
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
	TRACE("open(%s, %d, %d)", filename, oflag, mode);
	oid_t oid, dev, pipesrv;
	int fd = 0, err;
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

		if (fd > p->maxfd)
			break;

		if ((f = p->fds[fd].file = vm_kmalloc(sizeof(open_file_t))) == NULL)
			break;

		proc_lockInit(&f->lock);

		do {
			if (proc_lookup(filename, &oid) == EOK) {
				/* pass */
			}
			else if (oflag & O_CREAT) {
				GETFROMSTACK(ustack, mode_t, mode, 2);

				if (posix_create(filename, 1 /* otFile */, mode, dev, &oid) < 0)
					break;
			}
			else {
				break;
			}

			if ((err = proc_open(oid, oflag)) < 0)
				break;

			/* TODO: truncate, append */

			p->fds[fd].flags = oflag & O_CLOEXEC;

			if (!err) {
				hal_memcpy(&f->oid, &oid, sizeof(oid));
			}
			else {
				f->oid.port = oid.port;
				f->oid.id = err;
			}

			f->refs = 1;
			f->offset = 0;
			/* TODO: check for other types */
			f->type = oid.port == pipesrv.port ? ftPipe : ftRegular;
			f->status = oflag & ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_CLOEXEC);

			proc_lockClear(&p->lock);
			return fd;
		} while (0);

		p->fds[fd].file = NULL;
		proc_lockDone(&f->lock);
		vm_kfree(f);

	} while (0);

	proc_lockClear(&p->lock);
	return -1;
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

		posix_fileDeref(f);
		p->fds[fildes].file = NULL;
		proc_lockClear(&p->lock);
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

		if ((rcnt = proc_read(f->oid, f->offset, buf, nbyte, f->status)) < 0)
			break;

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
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

		if ((rcnt = proc_write(f->oid, f->offset, buf, nbyte, f->status)) < 0)
			break;

		proc_lockSet(&f->lock);
		f->offset += rcnt;
		proc_lockClear(&f->lock);

		proc_lockClear(&p->lock);
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
	proc_lockSet(&p->lock);

	do {
		if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
			break;

		if (proc_create(pipesrv.port, pxBufferedPipe, O_RDONLY | O_WRONLY, oid, pipesrv, NULL, &oid) < 0)
			break;

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
	proc_lockSet(&p->lock);

	do {
		if ((proc_lookup("/dev/posix/pipes", &pipesrv)) < 0)
			break;

		if (proc_create(pipesrv.port, pxPipe, 0, oid, pipesrv, NULL, &oid) < 0)
			break;

		/* link pipe in posix server */
		if (proc_link(oid, oid, pathname) < 0)
			break;

		/* create pipe in filesystem */
		if (posix_create(pathname, 2 /* otDev */, mode, oid, &file) < 0)
			break;

		proc_lockClear(&p->lock);
		return 0;
	} while (0);

	proc_lockClear(&p->lock);
	return -1;
}


int posix_link(const char *path1, const char *path2)
{
	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(path2);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, path2, namelen + 1);

	splitname(name, &basename, &dirname);

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
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
	proc_lockClear(&p->lock);
	return err;
}


int posix_unlink(const char *pathname)
{
	process_info_t *p;
	oid_t oid, dir;
	int err;
	char *name, *basename, *dirname;
	int namelen;

	namelen = hal_strlen(pathname);
	name = vm_kmalloc(namelen + 1);
	hal_memcpy(name, pathname, namelen + 1);

	splitname(name, &basename, &dirname);

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);

	do {
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
	proc_lockClear(&p->lock);
	return err;
}


off_t posix_lseek(int fildes, off_t offset, int whence)
{
	return -1;
}


int posix_ftruncate(int fildes, off_t length)
{
	return -1;
}


static int socksrvcall(msg_t *msg)
{
	oid_t oid;
	int err;

	if ((err = proc_lookup(PATH_SOCKSRV, &oid)) < 0)
		return set_errno(err);

	if ((err = proc_send(oid.port, msg)) < 0)
		return set_errno(err);

	return 0;
}


static ssize_t sockcall(int socket, msg_t *msg)
{
	process_info_t *p;
	open_file_t *f;
	sockport_resp_t *smo = (void *)msg->o.raw;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	if ((f = p->fds[socket].file) == NULL)
		return set_errno(-EBADF);

	if (f->type != ftInetSocket)
		return set_errno(-ENOTSOCK);

	if ((err = proc_send(f->oid.port, msg)) < 0)
		return set_errno(err);

	err = smo->ret;
	return set_errno(err);
}


static ssize_t socknamecall(int socket, msg_t *msg, struct sockaddr *address, socklen_t *address_len)
{
	sockport_resp_t *smo = (void *)msg->o.raw;
	ssize_t err;

	if ((err = sockcall(socket, msg)) < 0)
		return err;

	if (smo->sockname.addrlen > *address_len)
		smo->sockname.addrlen = *address_len;

	hal_memcpy(address, smo->sockname.addr, smo->sockname.addrlen);
	*address_len = smo->sockname.addrlen;

	return err;
}


static ssize_t sockdestcall(int socket, msg_t *msg, const struct sockaddr *address, socklen_t address_len)
{
	sockport_msg_t *smi = (void *)msg->i.raw;

	if (address_len > sizeof(smi->send.addr))
		return set_errno(-EINVAL);

	smi->send.addrlen = address_len;
	hal_memcpy(smi->send.addr, address, address_len);

	return sockcall(socket, msg);
}


int posix_accept(int socket, struct sockaddr *address, socklen_t *address_len)
{
	process_info_t *p;
	ssize_t err;
	msg_t msg;
	oid_t oid;
	sockport_msg_t *smi = (void *)msg.i.raw;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&oid, 0, sizeof(oid));
	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmAccept;
	smi->send.flags = 0;

	if ((err = socknamecall(socket, &msg, address, address_len)) < 0)
		return set_errno(err);

	oid.port = err;

	if ((err = posix_newFile(p, 0)) < 0) {
		msg.type = mtClose;
		proc_send(oid.port, &msg);
		return set_errno(err);
	}

	p->fds[err].file->type = ftInetSocket;
	hal_memcpy(&p->fds[err].file->oid, &oid, sizeof(oid));

	return err;
}


int posix_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmBind;

	return sockdestcall(socket, &msg, address, address_len);
}


int posix_connect(int socket, const struct sockaddr *address, socklen_t address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmConnect;

	return sockdestcall(socket, &msg, address, address_len);
}


int posix_getpeername(int socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetPeerName;

	return socknamecall(socket, &msg, address, address_len);
}


int posix_getsockname(int socket, struct sockaddr *address, socklen_t *address_len)
{
	msg_t msg;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetSockName;

	return socknamecall(socket, &msg, address, address_len);
}


int posix_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	ssize_t ret;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmGetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.o.data = optval;
	msg.o.size = *optlen;

	ret = sockcall(socket, &msg);

	if (ret < 0)
		return ret;

	*optlen = ret;
	return 0;
}


int posix_listen(int socket, int backlog)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmListen;
	smi->listen.backlog = backlog;

	return sockcall(socket, &msg);
}


ssize_t posix_recvfrom(int socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmRecv;
	smi->send.flags = flags;
	msg.o.data = message;
	msg.o.size = length;

	return socknamecall(socket, &msg, src_addr, src_len);
}


ssize_t posix_sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSend;
	smi->send.flags = flags;
	msg.i.data = (void *)message;
	msg.i.size = length;

	return sockdestcall(socket, &msg, dest_addr, dest_len);
}


int posix_socket(int domain, int type, int protocol)
{
	process_info_t *p;
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSocket;
	smi->socket.domain = domain;
	smi->socket.type = type;
	smi->socket.protocol = protocol;

	if ((err = socksrvcall(&msg)) < 0)
		return err;

	if (msg.o.lookup.err < 0)
		return set_errno(msg.o.lookup.err);

	if ((err = posix_newFile(p, 0)) < 0)
		return set_errno(err);

	p->fds[err].file->type = ftInetSocket;
	return err;
}


int posix_shutdown(int socket, int how)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmShutdown;
	smi->send.flags = how;

	return sockcall(socket, &msg);
}


int posix_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen)
{
	msg_t msg;
	sockport_msg_t *smi = (void *)msg.i.raw;

	hal_memset(&msg, 0, sizeof(msg));
	msg.type = sockmSetOpt;
	smi->opt.level = level;
	smi->opt.optname = optname;
	msg.i.data = (void *)optval;
	msg.i.size = optlen;

	return sockcall(socket, &msg);
}


static int posix_fcntlDup(int fd, int fd2, int cloexec)
{
	process_info_t *p;
	int err;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -1;

	proc_lockSet(&p->lock);
	while (p->fds[fd].file != NULL && fd++ < p->maxfd);

	if ((err = _posix_dup2(p, fd, fd2)) == fd && cloexec)
		p->fds[fd].flags = FD_CLOEXEC;

	proc_lockClear(&p->lock);
	return err;
}


static int posix_fcntlSetFd(int fd, unsigned flags)
{
	process_info_t *p;
	int err = EOK;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

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
	process_info_t *p;
	open_file_t *f;
	int err = EOK;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	proc_lockSet(&p->lock);
	if ((f = p->fds[fd].file) != NULL) {
		if (f->type == ftInetSocket)
			err = _sock_setfl(f, val);
		else
			f->status = val;
	}
	else {
		err = -EBADF;
	}
	proc_lockClear(&p->lock);
	return err;
}


static int posix_fcntlGetFl(int fd)
{
	process_info_t *p;
	open_file_t *f;
	int err = EOK;

	if ((p = pinfo_find(proc_current()->process->id)) == NULL)
		return -ENOSYS;

	proc_lockSet(&p->lock);

	if ((f = p->fds[fd].file) != NULL)
		err = f->type == ftInetSocket ? _sock_getfl(f) : f->status;

	else
		err = -EBADF;

	proc_lockClear(&p->lock);
	return err;
}


int posix_fcntl(int fd, unsigned int cmd, char *ustack)
{
	TRACE("fcntl(%d, %u, %u)", fd, cmd, arg);

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


void posix_init(void)
{
	proc_lockInit(&posix_common.lock);
	lib_rbInit(&posix_common.pid, pinfo_cmp, NULL);
}
