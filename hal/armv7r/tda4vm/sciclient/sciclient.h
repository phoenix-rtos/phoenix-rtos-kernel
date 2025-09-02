/*
 * Phoenix-RTOS
 *
 * sciclient module
 *
 * Copyright 2025 Phoenix Systems
 * Author: Rafał Mikielis
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 * 
 *  Copyright (C) 2015-2025 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 
 * All details regarding TISCI messaging under https://software-dl.ti.com/tisci/esd/22_01_02/index.html
 */

#ifndef SCICLIENT_H
#define SCICLIENT_H

#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "include/arch/armv7r/tda4vm/tisci_protocol.h"
#include "include/arch/armv7r/tda4vm/tisci_pm_clock.h"
#include "include/arch/armv7r/tda4vm/tisci_rm.h"


/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define SciApp_print(fmt, ...)			lib_printf("sciclient: "fmt"\n", ##__VA_ARGS__)
#define SciApp_sprintf(out, fmt, ...)	lib_sprintf(out, fmt, ##__VA_ARGS__)

#define INIT_MAGIC_VALUE		(0x84218421)
#define TISCI_TX_THREAD_OFFSET	(0x80U)

#define TISCI_PASS	(0U)

/**sciclient service request type */
#define SCICLIENT_MSG_TX_REQ	(1U)
#define SCICLIENT_MSG_RX_REQ	(0U)

/** sciclient message size */
#define SCICLIENT_MSG_MAX_SIZE	(60U)
#define SCICLIENT_MSG_RSVD		(4U)

/** Sciclient Service API Operation Mode */
#define SCICLIENT_SERVICE_OPERATION_MODE_POLLED           (0U)
#define SCICLIENT_SERVICE_OPERATION_MODE_INTERRUPT        (1U)

/** Sciclient Service API Timeout Values */
#define SCICLIENT_SERVICE_WAIT_FOREVER                    (0xFFFFFFFFU)
#define SCICLIENT_SERVICE_NO_WAIT                         (0x0U)

/**  Context IDs for Sciclient_ConfigPrms_t .
 */
/* R5_0(Non Secure): Cortex R5 context 0 on MCU island */
#define SCICLIENT_CONTEXT_R5_NONSEC (0U)
/* R5_1(Secure): Cortex R5 context 1 on MCU island(Boot) */
#define SCICLIENT_CONTEXT_R5_SEC (1U)

/** Secure Proxy registers*/
#define SEC_PROX_DATA_BASE_ADDR		(0x2A480004U)
#define SEC_PROX_STAT_BASE_ADDR		(0x2A380000U)
#define SEC_PROX_STAT_ERR			((0x1U << 31U))
#define SEC_PROX_STAT_MSG_CNT		((0xFFU))

/**
 * Secure Proxy configurations for MCU_0_R5_2 host 
*/

/** Thread ID macro for MCU_0_R5_2 notify */
#define TISCI_SEC_PROXY_MCU_0_R5_2_READ_NOTIFY_THREAD_ID (10U)
/** Num messages macro for MCU_0_R5_2 notify */
#define TISCI_SEC_PROXY_MCU_0_R5_2_READ_NOTIFY_NUM_MESSAGES (1U)

/** Thread ID macro for MCU_0_R5_2 response */
#define TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_THREAD_ID (11U)
/** Num messages macro for MCU_0_R5_2 response */
#define TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_NUM_MESSAGES (2U)

/** Thread ID macro for MCU_0_R5_2 high_priority */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_THREAD_ID (12U)
/** Num messages macro for MCU_0_R5_2 high_priority */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_HIGH_PRIORITY_NUM_MESSAGES (1U)

/** Thread ID macro for MCU_0_R5_2 low_priority */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_LOW_PRIORITY_THREAD_ID (13U)
/** Num messages macro for MCU_0_R5_2 low_priority */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_LOW_PRIORITY_NUM_MESSAGES (1U)

/** Thread ID macro for MCU_0_R5_2 notify_resp */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_NOTIFY_RESP_THREAD_ID (14U)

/** Num messages macro for MCU_0_R5_2 notify_resp */
#define TISCI_SEC_PROXY_MCU_0_R5_2_WRITE_NOTIFY_RESP_NUM_MESSAGES (1U)

/** Interrupt number for MCU_0_R5_2 response msg */
#define TISCI_SEC_PROXY_MCU_0_R5_2_READ_RESPONSE_INTR (65U)

/** SoC defined domgrp_compatibility (SYSTEM) */
#define DOMGRP_COMPATIBILITY	(0U)
/** SoC defined domgrp 00 (MCU) */
#define DOMGRP_00	((0x01U) << 0U)
/** SoC defined domgrp 01 (MAIN)*/
#define DOMGRP_01	((0x01U) << 1U)

/** TISCI hosts numbers
 */
/** DMSC(Secure): Security Controller */
#define TISCI_HOST_ID_DMSC (0U)
/** DM(Non Secure): Device Management */
#define TISCI_HOST_ID_DM (254U)
/** MCU_0_R5_0(Non Secure): Cortex R5 context 0 on MCU island */
#define TISCI_HOST_ID_MCU_0_R5_0 (3U)
/** MCU_0_R5_1(Secure): Cortex R5 context 1 on MCU island(Boot) */
#define TISCI_HOST_ID_MCU_0_R5_1 (4U)
/** MCU_0_R5_2(Non Secure): Cortex R5 context 2 on MCU island */
#define TISCI_HOST_ID_MCU_0_R5_2 (5U)
/** MCU_0_R5_3(Secure): Cortex R5 context 3 on MCU island */
#define TISCI_HOST_ID_MCU_0_R5_3 (6U)

#define SCICLIENT_MAX_QUEUE_SIZE            (7U)

typedef u32 tisci_msg_type;
typedef u8  tx_flag;

/* ========================================================================== */
/*                          Structure definitions                             */
/* ========================================================================== */

/** Input parameters for #Sciclient_service function.
 */
typedef struct
{
	/**< [IN] Type of message. */
    u16	messageType;
    
	/**< [IN] Flags for messages that are being transmitted. */
    u32	flags;
    
	/**< [IN] Pointer to the payload to be transmitted */
    u8 *pReqPayload;
    
	/**< [IN] Size of the payload to be transmitted (in bytes)*/
    u32	reqPayloadSize;

    /**< [IN] Indicates whether the request is being forwarded to another
     *        service provider. Only to be set internally by sciserver, if
     *        integrated into this build. Unused otherwise. */
    u8	forwardStatus;

} Sciclient_ReqPrm_t;


/** Output parameters for #Sciclient_service function.
 */
typedef struct
{
    /**< [OUT] Flags of response to messages. */	
    u32 flags;

	/**< [IN] Pointer to the received payload. The pointer is an input. The
     *        API will populate this with the firmware response upto the
     *        size mentioned in respPayloadSize. Please ensure respPayloadSize
     *        bytes are allocated.
     */
    u8 *pRespPayload;

	/**< [IN] Size of the response payload(in bytes) */
    u32 respPayloadSize;

	/**< [IN] Timeout(number of iterations) for receiving response
     *(Refer \ref Sciclient_ServiceOperationTimeout) */
    u32   timeout;

} Sciclient_RespPrm_t;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/** TISCI_MSG_SYS_RESET  
 *  default domain to be restarted is MCU domain (DOMGRP_00)*/
int Tisci_sys_reset(void);

/** TISCI_MSG_VERSION  - request SYSFW for its version number. 
 *  Checking if SYSFW and SCISERVER are up and running.
 */
int Tisci_msg_version(void);

/** TISCI_MSG_GET_CLOCK 
 *  Get the state of a clock to or from a hardware block.
 *  Usually every device has input/output clocks number < 255, clk32 is don't matter then.
 *  However for rare cases, this number can be > 255, then we set clk to 255, and clk32
 *  to proper value.
 *  \return programmed_state - this value will be left-shifted by 16 bits
  * 		current_state    - this value will occupy lower 16 bits 
  *  */
int Tisci_clk_get(u32 device, u8 clk, u32 clk32, int *clkState);

 /** TISCI_MSG_SET_CLOCK 
  *  Setup a hardware device’s clock state.
  *  Check TISCI_MSG_GET_CLOCK description for params details.
  *  \param state: TISCI_MSG_VALUE_CLOCK_SW_STATE_UNREQ / TISCI_MSG_VALUE_CLOCK_SW_STATE_REQ
  */
int Tisci_clk_set(u32 device, u8 clk, u32 clk32, u8 state);

/** TISCI_MSG_GET_FREQ 
 *  Get the clock frequency of a specific clock which belongs to a hardware block.
 *  Check TISCI_MSG_GET_CLOCK description for params details.
 * \param freq_hz: pointer to u64 variable which will hold response from SYSFW.
 */
int Tisci_clk_get_freq(u32 device, u8 clk, u32 clk32, u64 *freq_hz);

/** TISCI_MSG_QUERY_FREQ
  * Query to find closest match possible for a target frequency.
  * Check TISCI_MSG_GET_CLOCK description for params details.
  * \param min_freq_hz: The minimum allowable frequency in Hz.
  * \param target_freq_hz: The target clock frequency. A frequency will be found as 
  * close to the target frequency as possible.
  * \param max_freq_hz: The maximum allowable frequency in Hz
  * \param freq_hz: pointer to u64 variable which will hold response from SYSFW
 */
int Tisci_clk_query_freq(u32 device, u64 min_freq_hz, u64 target_freq_hz, 
							   u64 max_freq_hz,u8 clk, u32 clk32, u64 *freq_hz);

 /** TISCI_MSG_SET_FREQ
  *  Setup a clock frequency for a hardware block’s clock.
  *  Check TISCI_MSG_QUERY_FREQ description for params details.
  */
int Tisci_clk_set_freq(u32 device, u64 min_freq_hz, u64 target_freq_hz, 
							 u64 max_freq_hz,u8 clk, u32 clk32);

/** TISCI_MSG_GET_CLOCK_PARENT
 *  Query the clock parent currently configured for a specific clock source 
 *  of a hardware block
 *  */	
int Tisci_clk_get_parent(u32 device, u8 clk, u32 clk32, int *clkParent);	

/** TISCI_MSG_SET_CLOCK_PARENT
 * SoC specific customization for setting up a specific clock parent ID 
 * for the various clock input options for a hardware block’s clock
 * Check TISCI_MSG_GET_CLOCK description for params details.
 */
int Tisci_clk_set_parent(u32 device, u8 clk, u32 clk32, u8 parent, u32 parent32);	

/** TISCI_MSG_GET_NUM_CLOCK_PARENTS
 * Query for the number of parent clock paths available 
 * for a specific hardware block’s clock
 */
int Tisci_clk_get_parent_num(u32 device, u8 clk, u32 clk32, int *clkParentsNum);

/** TISCI_MSG_RM_RING_CFG
 *  The ring cfg TISCI message API is used to configure SoC 
 *  Navigator Subsystem Ring Accelerator rings.
 */
int Tisci_ra_alloc(u32 *addrLo, u8 index, u32 count);

/** TISCI_MSG_RM_PROXY_CFG
 * 	The proxy_cfg TISCI message API is used to configure the channelized firewalls 
 *  of a Navigator Subsystem proxy. he proxy index must be assigned to the host defined 
 *  in the TISCI header via the RM board configuration resource assignment range list.
 */
int Tisci_prx_alloc(u16 proxyInd);

/** TISCI_MSG_RM_PSIL_PAIR/UNPAIR
 *  For TX channel: srcThread is UDMA channel, destThread is peripheral
 *  For RX channel: srcThread is peripheral, UDMA channel is destThread
 */
int Tisci_rm_psil_pair(u16 srcThread, u16 dstThread);
int Tisci_rm_psil_unpair(u16 srcThread, u16 dstThread);

/** TISCI_MSG_RM_PSIL_WRITE/READ
 *  Write a PSI-L thread’s configuration registers via a PSI-L configuration proxy
 */
int Tisci_rm_psil_write(u16 thread, u16 taddr, u32 data);
int Tisci_rm_psil_read(u16 thread, u16 taddr, u32* data);

/** TISCI_MSG_RM_GET_RESOURCE_RANGE
 *  return value will be composed from all received values
 *  range_start << 48
 *  range_num << 32
 *  range_sec_start << 16
 *  range_sec_num << 0  
 */
int Tisci_rm_resource_range(u8 type, u16 subtype, u64 *resp);


/** TISCI_MSG_RM_UDMAP_TX_CH_CFG
 * 	
 *  TODO:
 */

#endif	/**  SCICLIENT_H */