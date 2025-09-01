/*
 * Phoenix-RTOS
 *
 * NAVSS header
 *
 * Copyright 2025 Phoenix Systems
 * Author: Rafał Mikielis
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef NAVSS_H
#define NAVSS_H

#include "hal/cpu.h"

/* ========================================================================== */
/*                          Typedefs and definitions                          */
/* ========================================================================== */
#define Navss_print(fmt, ...)	lib_printf("sciclient: "fmt"\n", ##__VA_ARGS__)

#define BITS(val, shift)	(val << shift)

/** Register addresses */
#define PROX_DATA_BASE_ADDR					(0x2A500200U)
#define PROX_CTL_REG_BASE_ADDR 	 			(0x2A500000U)
#define MCU_NAVSS0_UDMASS_RINGACC0_FIFOS	(0x2B000000)

/** Constant values */
#define RA_CHANNEL_SIZE		(4096U)
#define RA_MAX_MSG_SIZE		(512U)
#define PROX_MAX_MSG_SIZE	(512U)

#define PROX_CTL_REG_QUEUE	BITS(0xFFFF, 0)
#define PROX_CTL_REG_MODE	BITS(0x3, 16)
#define PROX_CTL_REG_SIZE   BITS(0x3, 24)

/** Default parameters */
#define DEFAULT_PROXY_INDEX		(13U)
#define DEFAULT_RA_INDEX_GP		(156U)

/** typedefs */
typedef enum {
	headAccess = 0,
	tailAccess,
	headPeek,
	tailPeek
} queueAccessMode_t;

typedef enum {
	size4B  = 0,
	size8B,
	size16B,
	size32B,
	size64B,
	size128B,
	size256B
} queueElementSize_t;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/** Send/receive data to/from proxy thread
 */
void Navss_proxy_send(u16 proxyInd, u8 *data, u32 size);
void Navss_proxy_recv(u16 proxyInd, u8 *data, u32 size);

/** Map proxy to RA channel (FIFO)
 */
void Navss_map_proxy_ra(u32 proxyInd, u32 raInd);

/** Initialize proxy (must be done before data access).
 *  This function also assigns proxy to RA channel (raInd).
 */
void Navss_proxy_init(u16 proxyInd, u16 raInd, queueElementSize_t size);

#endif 	// NAVSS_H