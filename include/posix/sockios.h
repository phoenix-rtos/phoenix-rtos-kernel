/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX sockets I/O controls
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SOCKIOS_H_
#define _PHOENIX_POSIX_SOCKIOS_H_

#include "sockif.h"
#include "sockrt.h"
#include "../ioctl.h"


/* Socket configuration controls */
#define SIOCGIFNAME    _IOWR('S', 0x10, struct ifreq)  /* Get name of interface with given index */
#define SIOCGIFCONF    _IOWR('S', 0x12, struct ifconf) /* Get iface list */
#define SIOCGIFFLAGS   _IOWR('S', 0x13, struct ifreq)  /* Get interface flags */
#define SIOCSIFFLAGS   _IOW('S', 0x14, struct ifreq)   /* Set interface flags */
#define SIOCGIFADDR    _IOWR('S', 0x15, struct ifreq)  /* Get device address */
#define SIOCSIFADDR    _IOW('S', 0x16, struct ifreq)   /* Set device address */
#define SIOCGIFDSTADDR _IOWR('S', 0x17, struct ifreq)  /* Get remote address (point-to-point interfaces) */
#define SIOCSIFDSTADDR _IOW('S', 0x18, struct ifreq)   /* Set remote address (point-to-point interfaces) */
#define SIOCGIFBRDADDR _IOWR('S', 0x19, struct ifreq)  /* Get broadcast address */
#define SIOCSIFBRDADDR _IOW('S', 0x1a, struct ifreq)   /* Set broadcast address */
#define SIOCGIFNETMASK _IOWR('S', 0x1b, struct ifreq)  /* Get network mask */
#define SIOCSIFNETMASK _IOW('S', 0x1c, struct ifreq)   /* Set network mask */
#define SIOCGIFMETRIC  _IOWR('S', 0x1d, struct ifreq)  /* Get metric */
#define SIOCSIFMETRIC  _IOW('S', 0x1e, struct ifreq)   /* Set metric */
#define SIOCGIFMTU     _IOWR('S', 0x21, struct ifreq)  /* Get MTU size */
#define SIOCSIFMTU     _IOW('S', 0x22, struct ifreq)   /* Set MTU size */
#define SIOCSIFHWADDR  _IOW('S', 0x24, struct ifreq)   /* Set interface MAC address */
#define SIOCGIFHWADDR  _IOWR('S', 0x27, struct ifreq)  /* Get interface MAC address */
#define SIOCADDMULTI   _IOWR('S', 0x31, struct ifreq)  /* Add multicast address */
#define SIOCDELMULTI   _IOWR('S', 0x32, struct ifreq)  /* Remove multicast address */
#define SIOCGIFINDEX   _IOWR('S', 0x33, struct ifreq)  /* Get index of interface with given name */


/* Routing table calls  */
#define SIOCADDRT _IOW('S', 0x44, struct rtentry) /* Add routing table entry */
#define SIOCDELRT _IOW('S', 0x45, struct rtentry) /* Delete routing table entry */


/* Unused but needed by busybox ifconfig */
#define SIOCGIFTXQLEN _IOWR('S', 0x42, struct ifreq) /* Get the tx queue length */
#define SIOCSIFTXQLEN _IOWR('S', 0x43, struct ifreq) /* Set the tx queue length */


#endif
