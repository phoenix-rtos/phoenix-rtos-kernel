#include HAL
#include "../include/posix.h"
#include "../proc/proc.h"
#include "posix.h"


#define US_PORT (-1) /* FIXME */


#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGIOT     SIGABRT
#define SIGEMT     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGBUS    10
#define SIGSEGV   11
#define SIGSYS    12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGURG    16
#define SIGSTOP   17
#define SIGTSTP   18
#define SIGCONT   19
#define SIGCHLD   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGIO     23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGINFO   29
#define SIGUSR1   30
#define SIGUSR2   31

#define NSIG 32

#define SIG_ERR (-1)
#define SIG_DFL (-2)
#define SIG_IGN (-3)

#define HOST_NAME_MAX 255


enum { ftRegular, ftPipe, ftFifo, ftInetSocket, ftUnixSocket, ftTty };


/* FIXME: share with posixsrv */
enum { pxBufferedPipe, pxPipe, pxPTY };


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


typedef struct _process_info_t {
	rbnode_t linkage;
	int process;
	int parent;
	int refs;
	int exitcode;

	thread_t *wait;

	struct _process_info_t *children;
	struct _process_info_t *zombies;
	struct _process_info_t *next, *prev;

	pid_t pgid;
	lock_t lock;
	int maxfd;
	fildes_t *fds;
} process_info_t;


/* SIOCGIFCONF ioctl special case: arg is structure with pointer */
struct ifconf {
	int ifc_len;    /* size of buffer */
	char *ifc_buf;  /* buffer address */
};

/* SIOADDRT and SIOCDELRT ioctls special case: arg is structure with pointer */
struct rtentry
{
	struct sockaddr rt_dst;
    struct sockaddr rt_gateway;
    struct sockaddr rt_genmask;
    short           rt_flags;
    short           rt_metric;
    char            *rt_dev;
    unsigned long   rt_mss;
    unsigned long   rt_window;
    unsigned short  rt_irtt;
};

extern void splitname(char *path, char **base, char **dir);


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


extern int unix_socketpair(int domain, int type, int protocol, int sv[2]);


extern int unix_shutdown(unsigned socket, int how);


extern int unix_link(unsigned socket);


extern int unix_unlink(unsigned socket);


extern int unix_setsockopt(unsigned socket, int level, int optname, const void *optval, socklen_t optlen);


extern void unix_sockets_init(void);
