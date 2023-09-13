/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX signals
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SIGNAL_H_
#define _PHOENIX_POSIX_SIGNAL_H_


typedef void (*sighandler_t)(int); /* Signal handler type */


#define SIG_DFL ((sighandler_t)0)  /* Request for default signal handling */
#define SIG_ERR ((sighandler_t)-1) /* Returned by signal() in case of error */
#define SIG_IGN ((sighandler_t)-2) /* Request for signal to be ignored */


#define SIGHUP    1  /* Hangup */
#define SIGINT    2  /* Terminal interrupt signal */
#define SIGQUIT   3  /* Terminal quit signal */
#define SIGILL    4  /* Illegal instruction */
#define SIGTRAP   5  /* Trace/breakpoint trap */
#define SIGABRT   6  /* Process abort signal */
#define SIGEMT    7  /* Emulator trap */
#define SIGFPE    8  /* Erroneous arithmetic operation */
#define SIGKILL   9  /* Kill (cannot be caught or ignored) */
#define SIGBUS    10 /* Access to an undefined portion of a memory object */
#define SIGSEGV   11 /* Invalid memory reference */
#define SIGSYS    12 /* Bad system call */
#define SIGPIPE   13 /* Write on a pipe with no one to read it */
#define SIGALRM   14 /* Alarm clock */
#define SIGTERM   15 /* Termination signal */
#define SIGURG    16 /* High bandwidth data is available at a socket */
#define SIGSTOP   17 /* Stop executing (cannot be caught or ignored) */
#define SIGTSTP   18 /* Terminal stop signal */
#define SIGCONT   19 /* Continue executing, if stopped */
#define SIGCHLD   20 /* Child process terminated, stopped or continued */
#define SIGTTIN   21 /* Background process attempting read */
#define SIGTTOU   22 /* Background process attempting write */
#define SIGIO     23 /* Ready to perform input/output, used with asynchronous I/O */
#define SIGXCPU   24 /* CPU time limit exceeded */
#define SIGXFSZ   25 /* File size limit exceeded */
#define SIGVTALRM 26 /* Virtual timer expired */
#define SIGPROF   27 /* Profiling timer expired */
#define SIGWINCH  28 /* Window (terminal) size has changed */
#define SIGINFO   29 /* Information request */
#define SIGUSR1   30 /* User-defined signal 1 */
#define SIGUSR2   31 /* User-defined signal 2 */
#define NSIG      32 /* Number of signals */

#define SIGIOT SIGABRT


#endif
