/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX sockets local interfaces
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_SOCKIF_H_
#define _PHOENIX_POSIX_SOCKIF_H_

#include "socket.h"


#define IFHWADDRLEN 6        /* Hardware (MAC) address length in bytes */
#define IFNAMSIZ    16       /* Interface name length in bytes */
#define IF_NAMESIZE IFNAMSIZ /* POSIX compliant alias */


/* Device mapping structure */
struct ifmap {
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};


/* Interface request structure, used for socket ioctl's */
struct ifreq {
	union {
		char ifrn_name[IFNAMSIZ];
	} ifr_ifrn;

	union {
		struct sockaddr ifru_addr;
		struct sockaddr ifru_dstaddr;
		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
		short ifru_flags;
		int ifru_ivalue;
		int ifru_mtu;
		struct ifmap ifru_map;
		char ifru_slave[IFNAMSIZ];
		char ifru_newname[IFNAMSIZ];
		char *ifru_data;
	} ifr_ifru;
};


#define ifr_name      ifr_ifrn.ifrn_name      /* Interface name */
#define ifr_hwaddr    ifr_ifru.ifru_hwaddr    /* MAC address */
#define	ifr_addr      ifr_ifru.ifru_addr      /* Address */
#define	ifr_dstaddr   ifr_ifru.ifru_dstaddr   /* Other end of p-p lnk */
#define	ifr_broadaddr ifr_ifru.ifru_broadaddr /* Broadcast address */
#define	ifr_netmask   ifr_ifru.ifru_netmask   /* Interface net mask */
#define	ifr_flags     ifr_ifru.ifru_flags     /* Flags */
#define	ifr_metric    ifr_ifru.ifru_ivalue    /* Metric */
#define	ifr_mtu       ifr_ifru.ifru_mtu       /* Mtu */
#define ifr_map       ifr_ifru.ifru_map       /* Device map */
#define ifr_slave     ifr_ifru.ifru_slave     /* Slave device */
#define	ifr_data      ifr_ifru.ifru_data      /* For use by interface */
#define ifr_ifindex   ifr_ifru.ifru_ivalue    /* Interface index*/
#define ifr_bandwidth ifr_ifru.ifru_ivalue    /* Link bandwidth */
#define ifr_qlen      ifr_ifru.ifru_ivalue    /* Queue length */
#define ifr_newname   ifr_ifru.ifru_newname   /* New name */


/* Interface configuration for machine, used for SIOCGIFCONF request */
struct ifconf {
	int ifc_len;

	union {
		char *ifcu_buf;
		struct ifreq *ifcu_req;
	} ifc_ifcu;
};


#define ifc_buf ifc_ifcu.ifcu_buf /* Buffer address */
#define ifc_req ifc_ifcu.ifcu_req /* Array of structures */


#endif
