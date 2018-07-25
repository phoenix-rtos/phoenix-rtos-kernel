#include HAL
#include "../../include/posix.h"
#include "../proc/proc.h"
#include "posix.h"


#define set_errno(x) ((x < 0) ? -1 : x)

#define US_PORT (-1) /* FIXME */


typedef struct {
	size_t sz, r, w;
	char full, mark;
	void *data;
} cbuffer_t;


enum { ftRegular, ftPipe, ftFifo, ftInetSocket, ftUnixSocket, ftTty };


/* FIXME: share with posixsrv */
enum { pxBufferedPipe, pxPipe, pxPTY };


enum { pxUnlockpt, pxGrantpt, pxPtsname };


typedef struct {
	int id;
	int type;

	union {
		struct {
		} unlockpt;
		struct {
		} grantpt;
		struct {
		} ptsname;
	};
} posixsrv_devctl_t;


typedef struct {
	oid_t ln;
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


extern void splitname(char *path, char **base, char **dir);


static inline size_t _cbuffer_free(cbuffer_t *buf)
{
	if (buf->w == buf->r)
		return buf->full ? 0 : buf->sz;

	return (buf->r - buf->w + buf->sz) & (buf->sz - 1);
}


static inline size_t _cbuffer_avail(cbuffer_t *buf)
{
	return buf->sz - _cbuffer_free(buf);
}


static inline int _cbuffer_discard(cbuffer_t *buf, size_t sz)
{
	int cnt = min(_cbuffer_free(buf), sz);
	buf->r = (buf->r + cnt) & (buf->sz - 1);
	return cnt;
}


extern int _cbuffer_init(cbuffer_t *buf, size_t sz);


extern int _cbuffer_read(cbuffer_t *buf, void *data, size_t sz);


extern int _cbuffer_write(cbuffer_t *buf, const void *data, size_t sz);


extern int posix_newFile(process_info_t *p, int fd);


extern process_info_t *pinfo_find(unsigned int pid);


extern int inet_accept(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int inet_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len);


extern int inet_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len);


extern int inet_getpeername(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int inet_getsockname(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int inet_getsockopt(unsigned socket, int level, int optname, void *optval, socklen_t *optlen);


extern int inet_listen(unsigned socket, int backlog);


extern ssize_t inet_recvfrom(unsigned socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t inet_sendto(unsigned socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern int inet_socket(int domain, int type, int protocol);


extern int inet_shutdown(unsigned socket, int how);


extern int inet_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen);


extern int unix_accept(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int unix_bind(unsigned socket, const struct sockaddr *address, socklen_t address_len);


extern int unix_connect(unsigned socket, const struct sockaddr *address, socklen_t address_len);


extern int unix_getpeername(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int unix_getsockname(unsigned socket, struct sockaddr *address, socklen_t *address_len);


extern int unix_getsockopt(unsigned socket, int level, int optname, void *optval, socklen_t *optlen);


extern int unix_listen(unsigned socket, int backlog);


extern ssize_t unix_recvfrom(unsigned socket, void *message, size_t length, int flags, struct sockaddr *src_addr, socklen_t *src_len);


extern ssize_t unix_sendto(unsigned socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);


extern int unix_socket(int domain, int type, int protocol);


extern int unix_shutdown(unsigned socket, int how);


extern int unix_link(unsigned socket);


extern int unix_unlink(unsigned socket);


extern int unix_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen);


extern void unix_sockets_init(void);
