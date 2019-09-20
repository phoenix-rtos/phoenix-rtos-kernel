#ifndef _PROC_DESCRIPTOR_H_
#define _PROC_DESCRIPTOR_H_

#include "../../include/types.h"

#define POSIXSRV_MAX_PID ((1LL << 30) - 1)
#define POSIXSRV_MAX_FDS 128

typedef struct _file_ops_t file_ops_t;


typedef struct _node_t {
//	idnode_t linkage;
	const file_ops_t *ops;
	unsigned refs;

	void (*destroy)(struct _node_t *);
} node_t;


typedef struct {
	unsigned refs;
	off_t offset;
	lock_t lock;
	mode_t mode;
	unsigned status;
	const file_ops_t *ops;
	oid_t oid;
	node_t *node;
} file_t;


typedef struct {
	file_t *file;
	unsigned flags;
} fildes_t;


struct _file_ops_t {
	int (*open)(file_t *);
	int (*read)(file_t *, ssize_t *, void *, size_t);
	int (*write)(file_t *, ssize_t *, void *, size_t);
	int (*close)(file_t *);
	int (*seek)(file_t *, long long *, int, long long);
	int (*truncate)(file_t *, int *, off_t);
	int (*ioctl)(file_t *, int *, pid_t, unsigned, void *);
};


int fs_lookup(const char *path, oid_t *node);
int fs_create_special(oid_t dir, const char *name, int id, mode_t mode);
int msg_unlink(oid_t dir, const char *name);
int node_add(node_t *node);
void node_put(node_t *node);


#endif
