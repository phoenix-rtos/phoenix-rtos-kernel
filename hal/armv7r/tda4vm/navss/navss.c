/*
 * Phoenix-RTOS
 *
 * NAVSS APIs
 *
 * Copyright 2025 Phoenix Systems
 * Author: Rafał Mikielis
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "navss.h"

#include "include/errno.h"
#include "hal/string.h"
#include "lib/printf.h"

//#define NAVSS_DBG

/* ========================================================================== */
/*						    Function Declarations                             */
/* ========================================================================== */
static inline void HW_REG32_WR(u32 regAddr, u32 regPayload);
static inline u32 HW_REG32_RD(u32 regAddr);
static inline u32 Navss_ProxyDataAddress(u16 threadID, u32 size);
static void Navss_proxy_mode(u16 proxyInd, queueAccessMode_t mode);

/* ========================================================================== */
/*   						  Global Variables                                */
/* ========================================================================== */
u64 gProxyInit = 0ULL;

/* ========================================================================== */
/*						 Function Definitions                                 */
/* ========================================================================== */
static inline void HW_REG32_WR(u32 regAddr, u32 regPayload)
{
	*(u32 *)regAddr = regPayload;
}

static inline u32 HW_REG32_RD(u32 regAddr)
{
	return *(volatile u32*)(regAddr);
}

/**
 Functions to get proxy registers addresses relatively to thread ID.
 */
static inline u32 Navss_ProxyDataAddress(u16 threadID, u32 size)
{
	return (u32)(PROX_DATA_BASE_ADDR + (threadID * 0x1000) + (PROX_MAX_MSG_SIZE - size));
}

static void Navss_proxy_mode(u16 proxyInd, queueAccessMode_t mode)
{
	u32 tempRegVal;
	u32 proxyAddr = PROX_CTL_REG_BASE_ADDR + (proxyInd * 0x1000);

	tempRegVal  = HW_REG32_RD(proxyAddr);
	tempRegVal &= ~(PROX_CTL_REG_MODE);
	tempRegVal |= (mode << 16);
	HW_REG32_WR(proxyAddr, tempRegVal);
}

void Navss_proxy_init(u16 proxyInd, u16 raInd, queueElementSize_t size)
{
	if ((gProxyInit & (1 << proxyInd)) == 0) {
		u32 tempRegVal;
		u32 proxyAddr = PROX_CTL_REG_BASE_ADDR + (proxyInd * 0x1000);
		
		tempRegVal  = HW_REG32_RD(proxyAddr);
		tempRegVal &= ~(PROX_CTL_REG_SIZE);
		tempRegVal |= (size << 24);
		HW_REG32_WR(proxyAddr, tempRegVal);

		Navss_map_proxy_ra(proxyInd, raInd);

		gProxyInit |= (1 << proxyInd);
	}
}

void Navss_proxy_send(u16 proxyInd, u8 *data, u32 size)
{
	u8 i;
	u32 threadAddr, payload32;
	u8 numWords;
	u8 *msg;
	
	Navss_proxy_mode(proxyInd, tailAccess);

	threadAddr = Navss_ProxyDataAddress(proxyInd, size);
	numWords = (size + 3U) / sizeof(u32);
	msg = data;

	for (i = 0; i < numWords; i++) {
		payload32 = 0;
		hal_memcpy((void *)&payload32, (const void *)msg, sizeof(u32));
		HW_REG32_WR(threadAddr, payload32);
		threadAddr += sizeof(u32);
		msg += sizeof(u32);

#ifdef NAVSS_DBG
		Navss_print("tx threadAddr = 0x%08x", threadAddr - 0x4);
		Navss_print("tx Payload %d = 0x%08x", i, payload32);
#endif
	}
}

void Navss_proxy_recv(u16 proxyInd, u8 *data, u32 size)
{
	u8 i;
	u32 threadAddr, payload32;
	u8 numWords;
	
	Navss_proxy_mode(proxyInd, headAccess);

	threadAddr = Navss_ProxyDataAddress(proxyInd, size);
	numWords = (size + 3U) / sizeof(u32);

	for (i = 0; i < numWords; i++) {
		payload32 = HW_REG32_RD(threadAddr);
		hal_memcpy((void *)data, (const void *)&payload32, sizeof(u32));
		threadAddr += sizeof(u32);
		data += sizeof(u32);

#ifdef NAVSS_DBG
		Navss_print("rx threadAddr = 0x%08x", threadAddr - 0x4);
		Navss_print("rx Payload %d = 0x%08x", i, payload32);
#endif
	}
}

void Navss_map_proxy_ra(u32 proxyInd, u32 raInd)
{
	u32 tempRegVal;
	u32 proxyAddr = PROX_CTL_REG_BASE_ADDR + (proxyInd * 0x1000);
	
	tempRegVal  = HW_REG32_RD(proxyAddr);
	tempRegVal &= ~(PROX_CTL_REG_QUEUE);
	tempRegVal |= (raInd & PROX_CTL_REG_QUEUE);
	HW_REG32_WR(proxyAddr, tempRegVal);
}
