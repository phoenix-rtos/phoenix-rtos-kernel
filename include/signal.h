/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Signals
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PHOENIX_SIGNAL_H_
#define _PHOENIX_SIGNAL_H_

#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef void (*sighandler_t)(int);


#define SIGNULL      0
#define SIGHUP       1
#define SIGINT       2
#define SIGQUIT      3
#define SIGILL       4
#define SIGTRAP      5
#define SIGABRT      6
#define SIGIOT       SIGABRT
#define SIGEMT       7
#define SIGFPE       8
#define SIGKILL      9
#define SIGBUS       10
#define SIGSEGV      11
#define SIGSYS       12
#define SIGPIPE      13
#define SIGALRM      14
#define SIGTERM      15
#define SIGURG       16
#define SIGSTOP      17
#define SIGTSTP      18
#define SIGCONT      19
#define SIGCHLD      20
#define SIGTTIN      21
#define SIGTTOU      22
#define SIGIO        23
#define SIGXCPU      24
#define SIGXFSZ      25
#define SIGVTALRM    26
#define SIGPROF      27
#define SIGWINCH     28
#define SIGINFO      29
#define SIGUSR1      30
#define SIGUSR2      31
#define PH_SIGCANCEL 32

#define NSIG 32

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t) - 1)
#define SIG_ERR ((sighandler_t) - 2)


enum { SIG_BLOCK,
	SIG_SETMASK,
	SIG_UNBLOCK };


#define SA_NOCLDSTOP (1 << 0)
#define SA_NOCLDWAIT (1 << 1)
#define SA_NODEFER   (1 << 2)
#define SA_ONSTACK   (1 << 3)
#define SA_RESETHAND (1 << 4)
#define SA_RESTART   (1 << 5)
#define SA_RESTORER  (1 << 6)
#define SA_SIGINFO   (1 << 7)


typedef int sigset_t;
typedef int sig_atomic_t;


union sigval {
	int sival_int;
	void *sival_ptr;
};


typedef struct {
	int si_signo;
	int si_code;
	pid_t si_pid;
	uid_t si_uid;
	void *si_addr;
	int si_status;
	union sigval si_value;
} siginfo_t;


struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int, siginfo_t *, void *);
	};
	sigset_t sa_mask;
	int sa_flags;
};

#ifdef __cplusplus
}
#endif


#endif
