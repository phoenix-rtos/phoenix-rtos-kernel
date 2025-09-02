/*
 * Phoenix-RTOS
 *
 * Operating system loader
 *
 * sciclient module
 *
 * Copyright 2025 Phoenix Systems
 * Author: Rafał Mikielis
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "sciclient.h"

#include "include/errno.h"
#include "include/threads.h"
#include "hal/string.h"
#include "lib/printf.h"
#include "hal/spinlock.h"

#define ERROR_MSG_BUF       40

/* ========================================================================== */
/* 							Function Declarations                             */
/* ========================================================================== */

static void HW_REG32_WR(u32 regAddr, u32 regPayload);
static u32  HW_REG32_RD(u32 regAddr);
static void Sciclient_prepareHeader(const Sciclient_ReqPrm_t *pReqPrm);
static void Sciclient_send(const Sciclient_ReqPrm_t *pReqPrm, const u8 *pSecHdr);
static int Sciclient_recv(Sciclient_RespPrm_t *pResp);
static void Sciclient_init(void);
static inline u32 Sciclient_getThreadStatusAddress(u8 threadID);
static inline u32 Sciclient_getThreadDataAddress(u8 threadID);
static int Sciclient_verifyThread(u8 threadID);
static int Sciclient_validateResp(const u8* pReq, const u8 *pResp);
static void Sciclient_terminate(spinlock_ctx_t *sc);
static int Sciclient_request_service(const Sciclient_ReqPrm_t *pReq, Sciclient_RespPrm_t *pResp);

/* ========================================================================== */
/*							 Global Variables                                */
/* ========================================================================== */

/**
 * Handle for #Sciclient_service function
 */
struct {
	/** tx and rx threads numbers for firmware communication */
	u32 txThread, rxThread;

	/** Sequence ID of the current request **/
	u32 currSeqId;
	
	/** Operation mode for the Sciclient Service API. Refer to
	* Sciclient_ServiceOperationMode for valid values. */
	u32 opModeFlag;

	/** Value to check if SCI client was initialized */
	u32  initialized;

	/** Variable to check whether Core context is secure/non-secure. This has
	 * to be given by the user via configParams. Default value is 0.
	 */
	u32 isSecureMode;
	spinlock_t sp;

	/** Buffer for last error sciclient encounters
	 */
	char errMsg[ERROR_MSG_BUF];

} sciclient_common;


/* Static Header for Security Messages. 
 * For GP devices sec_header is filled with 0s.
 */
static struct tisci_sec_header gSciclient_secHeader;

/* ========================================================================== */
/*		 Utility Function Definitions - for sciclient internal use            */
/* ========================================================================== */
static inline void HW_REG32_WR(u32 regAddr, u32 regPayload)  // macro ????
{
	*(u32 *)regAddr = regPayload;
}

static inline u32 HW_REG32_RD(u32 regAddr)  // macro ????
{
	return *(volatile u32*)(regAddr);
}

/**
 * Check if thread has any errors or pending messages.
 * For TX threads, MSB of threadID is set.
 * Differentiation between TX/RX is necessary since they read
 * status register differently.
 * */
static int Sciclient_verifyThread(u8 threadID)
{
	u32 status;
	u8 thread = threadID & 0x7F;
	int err = EOK;

	status = HW_REG32_RD(Sciclient_getThreadStatusAddress(thread));

	if (status & SEC_PROX_STAT_ERR) {   
		SciApp_sprintf(&sciclient_common.errMsg[0], "Thread %u has an error", thread);
		return -EIO;
	}
	
	if (threadID & BIT(7)) {
		err = (status & SEC_PROX_STAT_MSG_CNT)? EOK : -EBUSY; 
		if (err == -EBUSY) {
			SciApp_sprintf(&sciclient_common.errMsg[0], "Thread %u has no TX credits", thread); 
		}
	} else {
		err = (status & SEC_PROX_STAT_MSG_CNT)? -EBUSY : EOK;
		if (err == -EBUSY) {
			SciApp_sprintf(&sciclient_common.errMsg[0], "Thread %u has pend msg", thread);  
		}
	}

	return err;
}

/** Function to prepare regular TISCI header.
*/
static void Sciclient_prepareHeader(const Sciclient_ReqPrm_t *pReqPrm)
{
	struct tisci_header *th = (struct tisci_header *)pReqPrm->pReqPayload;
	th->type = pReqPrm->messageType;
	th->flags = pReqPrm->flags;
	th->host = TISCI_HOST_ID_MCU_0_R5_2;
	th->seq = sciclient_common.currSeqId;

	sciclient_common.currSeqId = (sciclient_common.currSeqId + 1) %
								 SCICLIENT_MAX_QUEUE_SIZE;
}

/** Functions to get secure proxy registers addresses relatively to thread ID.
 */
static inline u32 Sciclient_getThreadDataAddress(u8 threadID) 
{
	return (u32)(SEC_PROX_DATA_BASE_ADDR + (threadID * 0x1000));
}

static inline u32 Sciclient_getThreadStatusAddress(u8 threadID) 
{
	return (u32)(SEC_PROX_STAT_BASE_ADDR + (threadID * 0x1000));
}


static void Sciclient_init(void)
{
	if (sciclient_common.initialized != INIT_MAGIC_VALUE) {
		sciclient_common.opModeFlag = SCICLIENT_SERVICE_OPERATION_MODE_POLLED;
		/** Initialize currSeqId. Make sure currSeqId is never 0 */
		sciclient_common.currSeqId = 1;

		/** configuring secure proxy for DM communication */
		sciclient_common.txThread = TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID;
		sciclient_common.rxThread = TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID;

		/** spinlock registration */
		hal_spinlockCreate(&sciclient_common.sp, "sciclient");

		/* Set initialization variable */
		sciclient_common.initialized= INIT_MAGIC_VALUE;
		hal_memset(&sciclient_common.errMsg[0], 0, ERROR_MSG_BUF);
	}

	return;
}

void Sciclient_deinit(void)
{
	/** TODO: destroy spinlock */
}

static int Sciclient_request_service(const Sciclient_ReqPrm_t *pReq, Sciclient_RespPrm_t *pResp)
{
	spinlock_ctx_t sc;

	Sciclient_init();
	
	hal_spinlockSet(&sciclient_common.sp, &sc);

	/** verify if TX and RX threads are ready for message exchange */
	if (Sciclient_verifyThread(TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID + TISCI_TX_THREAD_OFFSET) || 
		Sciclient_verifyThread(TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID)) {
		Sciclient_terminate(&sc);
		return -EBUSY;
	}
	
	/** TISCI message sending */
	Sciclient_send(pReq, NULL);

	/** TISCI message receiving */
	if (Sciclient_recv(pResp)) {
		Sciclient_terminate(&sc);
		return -EAGAIN;
	}

	hal_spinlockClear(&sciclient_common.sp, &sc);
	
	if (Sciclient_validateResp((const u8*)pReq->pReqPayload, (const u8*)pResp->pRespPayload)) {
		Sciclient_terminate(&sc);
		return -EAGAIN;     
	}
	
	return EOK;
}

static void Sciclient_send(const Sciclient_ReqPrm_t *pReqPrm, const u8 *pSecHdr)
{
	u8 i;
	u16 numWords;
	volatile u32 threadAddr;
	u32 payload32;
	int numBytes;
	const u8 *msg = NULL;

	threadAddr = Sciclient_getThreadDataAddress(TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID);

	Sciclient_prepareHeader(pReqPrm);

	if (pSecHdr != NULL) {
		/* Write security header first. Word aligned so operating on words */
		numWords = sizeof(gSciclient_secHeader)/sizeof(u32);
		msg = pSecHdr;
		for (i = 0; i < numWords; i++) {
			hal_memcpy((void *)&payload32, (const void *)msg, sizeof(u32));
			HW_REG32_WR(threadAddr, payload32);
			threadAddr += sizeof(u32);
			msg += sizeof(u32);
		}
	}

	/* Write TISCI header and message payload */
	numWords = (pReqPrm->reqPayloadSize + 3U) / sizeof(u32);
	numBytes = pReqPrm->reqPayloadSize;
	msg = pReqPrm->pReqPayload;

	for (i = 0; i < numWords; i++) {
		int pSize = (numBytes - (int)sizeof(u32) >= 0)? sizeof(u32) : numBytes & 0x3;
		numBytes -= pSize;
		payload32 = 0;
		hal_memcpy((void *)&payload32, (const void *)msg, (size_t)pSize);
		HW_REG32_WR(threadAddr, payload32);
		threadAddr += sizeof(u32);
		msg += pSize;

#ifdef SECURE_PROXY_DBG
		SciApp_print("tx threadAddr = 0x%08x", threadAddr - 0x4);
		SciApp_print("tx Payload %d = 0x%08x, pSize = %d", i, payload32, pSize);
#endif 
	}

	/* Write to the last register of TX thread to trigger msg send */
	if ((sizeof(gSciclient_secHeader) + pReqPrm->reqPayloadSize) <= (SCICLIENT_MSG_MAX_SIZE - sizeof(u32))) {
		threadAddr = Sciclient_getThreadDataAddress(TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID) + 
					 SCICLIENT_MSG_MAX_SIZE - sizeof(u32);
		payload32 = (u32)0;
		HW_REG32_WR(threadAddr, payload32);              
	}
}

static int Sciclient_recv(Sciclient_RespPrm_t *pResp)
{
	u8 i;
	int status = EOK;
	int numBytes, numWords;
	u32 payload32;
	u32 threadAddr;
	u32 timeout = pResp->timeout;
	u8 *pRespPayload = pResp->pRespPayload;

	while ((HW_REG32_RD(Sciclient_getThreadStatusAddress(TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID)) &
		  SEC_PROX_STAT_MSG_CNT) == 0) {
		if (timeout > 0) {
			timeout--;
		} else {
			SciApp_sprintf(&sciclient_common.errMsg[0], "SCISERVER did not respond on time");           
			return -EBUSY;
		}
	}

	/** copy message from Secure Proxy register to local buffer */
	numWords = (pResp->respPayloadSize + 3U) / sizeof(u32);
	numBytes = pResp->respPayloadSize;

	for (i = 0; i < numWords; i++) {
		int pSize = (numBytes - (int)sizeof(u32) >= 0)? sizeof(u32) : numBytes & 0x3;
		numBytes -= pSize;
		threadAddr = Sciclient_getThreadDataAddress(TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID) +
					 (0x4 * i);
		payload32 = HW_REG32_RD(threadAddr);
		hal_memcpy((void *)pRespPayload, (const void *)&payload32, pSize);
		pRespPayload += pSize;      
		
#ifdef SECURE_PROXY_DBG
		SciApp_print("rx threadAddr = 0x%08x", threadAddr);
		SciApp_print("rx Payload %d = 0x%08x, pSize = %d", i, payload32, pSize);
#endif      
	}

	/** read from last byte to clear RX proxy */
	if (numWords < SCICLIENT_MSG_MAX_SIZE / sizeof(u32)) {
		threadAddr = Sciclient_getThreadDataAddress(TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID) +
					 SCICLIENT_MSG_MAX_SIZE - sizeof(u32);
		HW_REG32_RD(threadAddr);

#ifdef SECURE_PROXY_DBG
		SciApp_print("rx threadAddr of last byte = 0x%08x", threadAddr);
#endif          
	}

	return status;
}

static int Sciclient_validateResp(const u8* msgReq, const u8 *msgResp)
{
	const struct tisci_header *req = (const struct tisci_header *)msgReq;
	const struct tisci_header *resp = (const struct tisci_header *)msgResp;
		
	if (req->type == resp->type &&
		req->seq == resp->seq   &&
		req->host == resp->host &&
		resp->flags & TISCI_MSG_FLAG_ACK) {
		return EOK;
	}   else {
#ifdef SECURE_PROXY_DBG 
		SciApp_print("validate type: %u vs %u", req->type, resp->type);
		SciApp_print("validate seq: %u vs %u", req->seq, resp->seq);
		SciApp_print("validate type: %u vs %u", req->host, resp->host);
		SciApp_print("validate flags %u", resp->flags & TISCI_MSG_FLAG_ACK);
#endif      
		SciApp_sprintf(&sciclient_common.errMsg[0], "RX message validation failed");    
	}

	return -EAGAIN;
}

static void Sciclient_terminate(spinlock_ctx_t *sc)
{
	if (sciclient_common.sp.lock) {
		hal_spinlockClear(&sciclient_common.sp, sc);
	}

	SciApp_print("%s", &sciclient_common.errMsg[0]);
}

/* ========================================================================== */
/* 		   User Function Definitions - providing TISCI services 		      */
/* ========================================================================== */

int Tisci_sys_reset(void)
{
	spinlock_ctx_t sc;

	Sciclient_init();

	hal_spinlockSet(&sciclient_common.sp, &sc);

	/** verify TX thread */
	if (Sciclient_verifyThread(TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID + 0x80) < EOK) {
		hal_spinlockClear(&sciclient_common.sp, &sc);
		return -EBUSY;
	}

	struct tisci_msg_sys_reset_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.domain = DOMGRP_00;
	
	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_SYS_RESET,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};

	Sciclient_send(&pReq, NULL);
	hal_spinlockClear(&sciclient_common.sp, &sc);

	return EOK;
}


int Tisci_msg_version(void)
{       
	/** TISCI messages preparing */
	struct tisci_msg_version_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_VERSION,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};
	
	struct tisci_msg_version_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};
	
	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}

	/** Process received message - service dependent part */
	SciApp_print("DMSC Firmware Version %s", (char *) msgResp.str);
	SciApp_print("Firmware revision 0x%x", msgResp.version);
	
	return EOK;
}

/* ========================================================================== */
/* 							PM Clock APIs                                     */
/* ========================================================================== */

int Tisci_clk_get(u32 device, u8 clk, u32 clk32, int *clkState)
{
	struct tisci_msg_get_clock_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_GET_CLOCK,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_get_clock_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}

	/** Process received message - service dependent part */
	*clkState |= (msgResp.programmed_state << 16);
	*clkState |= (msgResp.current_state);

	return EOK;
}

int Tisci_clk_set(u32 device, u8 clk, u32 clk32, u8 state)
{
	struct tisci_msg_set_clock_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;
	msgReq.state = state;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_SET_CLOCK,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_set_clock_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	return EOK;
}

int Tisci_clk_get_freq(u32 device, u8 clk, u32 clk32, u64 *freq_hz)
{
	struct tisci_msg_get_freq_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_GET_FREQ,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_get_freq_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   
	
	/** Process received message - service dependent part */
	*freq_hz = msgResp.freq_hz;

	return EOK;
}

int Tisci_clk_query_freq(u32 device, u64 min_freq_hz, u64 target_freq_hz, 
							   u64 max_freq_hz, u8 clk, u32 clk32, u64 *freq_hz)
{
	struct tisci_msg_query_freq_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;
	msgReq.min_freq_hz = min_freq_hz;
	msgReq.target_freq_hz = target_freq_hz;
	msgReq.max_freq_hz = max_freq_hz;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_QUERY_FREQ,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_query_freq_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	/** Process received message - service dependent part */
	*freq_hz = msgResp.freq_hz;

	return EOK;
}

int Tisci_clk_set_freq(u32 device, u64 min_freq_hz, u64 target_freq_hz, 
							 u64 max_freq_hz,u8 clk, u32 clk32)
{
	struct tisci_msg_set_freq_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;
	msgReq.min_freq_hz = min_freq_hz;
	msgReq.target_freq_hz = target_freq_hz;
	msgReq.max_freq_hz = max_freq_hz;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_SET_FREQ,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_set_freq_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   
	
	return EOK;
}

int Tisci_clk_get_parent(u32 device, u8 clk, u32 clk32, int *clkParent)
{
	struct tisci_msg_get_clock_parent_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_GET_CLOCK_PARENT,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_get_clock_parent_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}

	*clkParent = (msgResp.parent == 255)? msgResp.parent32 : msgResp.parent;

	return EOK;
}

int Tisci_clk_set_parent(u32 device, u8 clk, u32 clk32, u8 parent, u32 parent32)
{
	struct tisci_msg_set_clock_parent_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;
	msgReq.parent = parent;
	msgReq.parent32 = parent32;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_SET_CLOCK_PARENT,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_set_clock_parent_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	return EOK;
}

int Tisci_clk_get_parent_num(u32 device, u8 clk, u32 clk32, int *clkParentsNum)
{
	struct tisci_msg_get_num_clock_parents_req msgReq;
	msgReq.device = device;
	msgReq.clk = clk;
	msgReq.clk32 = clk32;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_GET_NUM_CLOCK_PARENTS,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_get_num_clock_parents_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   
	
	*clkParentsNum = (msgResp.num_parents == 255)? msgResp.num_parentint32_t : msgResp.num_parents;

	return EOK;
}

/* ========================================================================== */
/* 						   Resource Manager APIs                              */
/* ========================================================================== */

int Tisci_ra_alloc(u32 *addrLo, u8 index, u32 count)
{
	struct tisci_msg_rm_ring_cfg_req msgReq;
	msgReq.valid_params = TISCI_MSG_VALUE_RM_RING_ADDR_LO_VALID |
						  TISCI_MSG_VALUE_RM_RING_ADDR_HI_VALID |
						  TISCI_MSG_VALUE_RM_RING_COUNT_VALID   |
						  TISCI_MSG_VALUE_RM_RING_SIZE_VALID    |
						  TISCI_MSG_VALUE_RM_RING_MODE_VALID    |
						  TISCI_MSG_VALUE_RM_RING_ORDER_ID_VALID|
						  TISCI_MSG_VALUE_RM_RING_ASEL_VALID;

	msgReq.nav_id = J721E_DEV_MCU_NAVSS0_RINGACC0;
	msgReq.index = index;
	msgReq.addr_lo = (u32)addrLo;
	msgReq.addr_hi = 0;
	msgReq.count = (u32)count;
	msgReq.mode = TISCI_MSG_VALUE_RM_RING_MODE_MESSAGE;
	msgReq.size = TISCI_MSG_VALUE_RM_RING_SIZE_8B;
	msgReq.asel = 0;
	msgReq.order_id = 0;
	msgReq.virtid = 0;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_RING_CFG,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_ring_cfg_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   
	return EOK;
}

int Tisci_prx_alloc(u16 proxyInd)
{
	struct tisci_msg_rm_proxy_cfg_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.nav_id = J721E_DEV_MCU_NAVSS0_PROXY0;
	msgReq.index = proxyInd;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_PROXY_CFG,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_proxy_cfg_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   
	return EOK;
}

int Tisci_rm_psil_pair(u16 srcThread, u16 dstThread)
{
	struct tisci_msg_rm_psil_pair_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.nav_id = J721E_DEV_MCU_NAVSS0;
	msgReq.src_thread = srcThread;
	msgReq.dst_thread = dstThread;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_PSIL_PAIR,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_psil_pair_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	return EOK;
}

int Tisci_rm_psil_unpair(u16 srcThread, u16 dstThread)
{
	struct tisci_msg_rm_psil_unpair_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.nav_id = J721E_DEV_MCU_NAVSS0;
	msgReq.src_thread = srcThread;
	msgReq.dst_thread = dstThread;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_PSIL_UNPAIR,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_psil_unpair_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	return EOK;
}

int Tisci_rm_psil_write(u16 thread, u16 taddr, u32 data)
{
	struct tisci_msg_rm_psil_write_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.nav_id = J721E_DEV_MCU_NAVSS0;
	msgReq.thread = thread;
	msgReq.taddr = taddr;
	msgReq.data = data;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_PSIL_WRITE,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_psil_write_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	return EOK;
}

int Tisci_rm_psil_read(u16 thread, u16 taddr, u32* data)
{
	struct tisci_msg_rm_psil_read_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.nav_id = J721E_DEV_MCU_NAVSS0;
	msgReq.thread = thread;
	msgReq.taddr = taddr;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_PSIL_WRITE,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_psil_read_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	*data = msgResp.data;

	return EOK;
}


int Tisci_rm_resource_range(u8 type, u16 subtype, u64 *resp)
{
	u64 data;

	struct tisci_msg_rm_get_resource_range_req msgReq;
	hal_memset(&msgReq, 0, sizeof(msgReq));
	msgReq.type = type;
	msgReq.subtype = subtype;
	msgReq.secondary_host = TISCI_MSG_VALUE_RM_UNUSED_SECONDARY_HOST;

	const Sciclient_ReqPrm_t pReq = 
	{
		TISCI_MSG_RM_GET_RESOURCE_RANGE,
		TISCI_MSG_FLAG_AOP,
		(u8 *)&msgReq,
		sizeof(msgReq),
		0
	};      

	struct tisci_msg_rm_get_resource_range_resp msgResp;
	hal_memset(&msgResp, 0, sizeof(msgResp));
	Sciclient_RespPrm_t pResp = 
	{
		0,
		(u8 *)&msgResp,
		sizeof(msgResp),
		SCICLIENT_SERVICE_WAIT_FOREVER
	};  

	if (Sciclient_request_service(&pReq, &pResp)) {
		return -EAGAIN;
	}   

	data =  ((u64)msgResp.range_start << 48) |
			((u64)msgResp.range_num << 32)   |
			((u64)msgResp.range_start_sec << 16) |
			((u64)msgResp.range_num_sec);  

	*resp = data;       
	return EOK; 
}
