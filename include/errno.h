/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Error definitions
 *
 * Copyright 2012-2015, 2016-2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Jacek Popko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_ERRNO_H_
#define _PH_ERRNO_H_

/* Linux/x86-compatible numbers for easier debugging */

#define EOK             0  /* No error */
#define EPERM           1  /* Operation not permitted */
#define ENOENT          2  /* No such file or directory */
#define ESRCH           3  /* No such process */
#define EINTR           4  /* Interrupted system call */
#define EIO             5  /* I/O error */
#define ENXIO           6  /* No such device or address */
#define E2BIG           7  /* Argument list too long */
#define ENOEXEC         8  /* Exec format error */
#define EBADF           9  /* Bad file number */
#define ECHILD          10 /* No child processes */
#define EAGAIN          11 /* Try again */
#define EWOULDBLOCK     EAGAIN
#define ENOMEM          12  /* Out of memory */
#define EACCES          13  /* Permission denied */
#define EFAULT          14  /* Bad address */
#define ENOTBLK         15  /* Block device required */
#define EBUSY           16  /* Device or resource busy */
#define EEXIST          17  /* File exists */
#define EXDEV           18  /* Cross-device link */
#define ENODEV          19  /* No such device */
#define ENOTDIR         20  /* Not a directory */
#define EISDIR          21  /* Is a directory */
#define EINVAL          22  /* Invalid argument */
#define ENFILE          23  /* File table overflow */
#define EMFILE          24  /* Too many open files */
#define ENOTTY          25  /* Not a typewriter */
#define ETXTBSY         26  /* Text file busy */
#define EFBIG           27  /* File too large */
#define ENOSPC          28  /* No space left on device */
#define ESPIPE          29  /* Illegal seek */
#define EROFS           30  /* Read-only file system */
#define EMLINK          31  /* Too many links */
#define EPIPE           32  /* Broken pipe */
#define EDOM            33  /* Math argument out of domain of func */
#define ERANGE          34  /* Math result not representable */
#define EDEADLK         35  /* Resource deadlock would occur */
#define ENAMETOOLONG    36  /* Name too long */
#define ENOLCK          37  /* No locks available */
#define ENOSYS          38  /* Function not implemented */
#define ENOTEMPTY       39  /* Directory not empty */
#define ELOOP           40  /* Too many levels of symbolic links */
#define ENOMSG          42  /* No message of the desired type */
#define EUNATCH         49  /* Protocol driver not attached */
#define ENODATA         61  /* No data available */
#define ETIME           62  /* Timer expired */
#define ENONET          64  /* Machine is not on the network */
#define EPROTO          71  /* Protocol error */
#define EBADMSG         74  /* Bad message */
#define EOVERFLOW       75  /* Value too large for defined data type */
#define EBADFD          77  /* File descriptor in bad state */
#define EILSEQ          84  /* Invalid or incomplete multibyte or wide character */
#define ENOTSOCK        88  /* Socket operation on non-socket */
#define EDESTADDRREQ    89  /* Destination address required */
#define EMSGSIZE        90  /* Message too long */
#define EPROTOTYPE      91  /* Protocol wrong type for socket */
#define ENOPROTOOPT     92  /* Protocol not available */
#define EPROTONOSUPPORT 93  /* Protocol not supported */
#define EOPNOTSUPP      95  /* Operation not supported */
#define EPFNOSUPPORT    96  /* Protocol family not supported */
#define EAFNOSUPPORT    97  /* Address family not supported by protocol */
#define EADDRINUSE      98  /* Address already in use */
#define EADDRNOTAVAIL   99  /* Cannot assign requested address */
#define ENETDOWN        100 /* Network is down */
#define ENETUNREACH     101 /* Network is unreachable */
#define ENETRESET       102 /* Network dropped connection on reset */
#define ECONNABORTED    103 /* Software caused connection abort */
#define ECONNRESET      104 /* Connection reset by peer */
#define ENOBUFS         105 /* No buffer space available */
#define EISCONN         106 /* Transport endpoint is already connected */
#define ENOTCONN        107 /* Transport endpoint is not connected */
#define ETIMEDOUT       110 /* Connection timed out */
#define ECONNREFUSED    111 /* Connection refused */
#define EHOSTDOWN       112 /* Host is down */
#define EHOSTUNREACH    113 /* No route to host */
#define EALREADY        114 /* Operation already in progress */
#define EINPROGRESS     115 /* Operation now in progress */
#define EUCLEAN         117 /* Structure needs cleaning */
#define EOWNERDEAD      130 /* Owner died */
#define ENOTRECOVERABLE 131 /* State not recoverable */
#define ENOTSUP         EOPNOTSUPP


#endif
