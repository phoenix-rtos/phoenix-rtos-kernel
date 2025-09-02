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
 *  Copyright (C) 2018-2025 Texas Instruments Incorporated
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
 */

#ifndef RM_TISCI_RA_H
#define RM_TISCI_RA_H


/** TISCI RM RA messages */
#define TISCI_MSG_RM_RING_CFG   (0x1110U)
#define TISCI_MSG_RM_PROXY_CFG	(0x1300U)

/** TISCI RM PSIL messages */
#define TISCI_MSG_RM_PSIL_PAIR     (0x1280U)
#define TISCI_MSG_RM_PSIL_UNPAIR   (0x1281U)
#define TISCI_MSG_RM_PSIL_READ     (0x1282U)
#define TISCI_MSG_RM_PSIL_WRITE    (0x1283U)

/* ========================================================================== */
/*                           DeviceID definitions                             */
/* ========================================================================== */
#define J721E_DEV_MCU_NAVSS0_PROXY0		(234U)
#define	J721E_DEV_MCU_NAVSS0_RINGACC0	(235U)
#define J721E_DEV_MCU_NAVSS0			(232U)

#define TISCI_MSG_VALUE_RM_UNUSED_SECONDARY_HOST (0xFFu)

/**
 * Proxy subtypes definitions
 */
#define TISCI_RESASG_SUBTYPE_PROXY_PROXIES 	(0x0000U)
#define TISCI_RESASG_SUBTYPES_PROXY_CNT 	(0x0001U)

/**
 * RA subtypes definitions
 */
#define TISCI_RESASG_SUBTYPE_RA_ERROR_OES				 	(0x0000U)
#define TISCI_RESASG_SUBTYPE_RA_GP 							(0x0001U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_RX 					(0x0002U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_TX 					(0x0003U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_TX_EXT 				(0x0004U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_RX_H 					(0x0005U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_RX_UH 				(0x0006U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_TX_H 					(0x0007U)
#define TISCI_RESASG_SUBTYPE_RA_UDMAP_TX_UH 				(0x0008U)
#define TISCI_RESASG_SUBTYPE_RA_VIRTID 						(0x000AU)
#define TISCI_RESASG_SUBTYPE_RA_MONITORS 					(0x000BU)
#define TISCI_RESASG_SUBTYPE_BCDMA_RING_SPLIT_TR_RX_CHAN 	(0x000EU)
#define TISCI_RESASG_SUBTYPE_BCDMA_RING_SPLIT_TR_TX_CHAN 	(0x000FU)
#define TISCI_RESASG_SUBTYPES_RA_CNT 						(0x000DU)

/**
 * The addr_lo parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_ADDR_LO_VALID  (1u << 0u)
/**
 * The addr_hi parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_ADDR_HI_VALID  (1u << 1u)
/**
 * The count parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_COUNT_VALID    (1u << 2u)
/**
 * The mode parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_MODE_VALID     (1u << 3u)
/**
 * The size parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_VALID     (1u << 4u)
/**
 * The order_id parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_ORDER_ID_VALID (1u << 5u)
/**
 * The virtid parameter is valid for RM ring configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_RING_VIRTID_VALID   (1u << 6u)
/**
 * The asel parameter is valid for RM ring configure TISCI message for SoCs
 * that have ASEL capability for rings
 */
#define TISCI_MSG_VALUE_RM_RING_ASEL_VALID     (1U << 7U)

/**
 * Exposed ring mode for @ref tisci_msg_rm_ring_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_RING_MODE_RING      (0x0u)
/**
 * Messaging ring mode for @ref tisci_msg_rm_ring_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_RING_MODE_MESSAGE   (0x1u)
/**
 * Credentials ring mode for @ref tisci_msg_rm_ring_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_RING_MODE_CREDENTIALS (0x2u)
/**
 * QM ring mode for @ref tisci_msg_rm_ring_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_RING_MODE_QM        (0x3u)

/**
 * 4-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_4B        (0x0u)
/**
 * 8-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_8B        (0x1u)
/**
 * 16-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_16B       (0x2u)
/**
 * 32-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_32B       (0x3u)
/**
 * 64-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_64B       (0x4u)
/**
 * 128-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_128B      (0x5u)
/**
 * 256-byte ring element size for @ref tisci_msg_rm_ring_cfg_req::size
 */
#define TISCI_MSG_VALUE_RM_RING_SIZE_256B      (0x6u)

/**
 * The source parameter is valid for RM monitor configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_MON_SOURCE_VALID    (1u << 0U)
/**
 * The mode parameter is valid for RM monitor configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_VALID      (1u << 1U)
/**
 * The queue parameter is valid for RM monitor configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_MON_QUEUE_VALID     (1u << 2U)
/**
 * The data1_val parameter is valid for RM monitor configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_MON_DATA0_VAL_VALID (1u << 3U)
/**
 * The data0_val parameter is valid for RM monitor configure TISCI message
 */
#define TISCI_MSG_VALUE_RM_MON_DATA1_VAL_VALID (1u << 4U)

/**
 * Element count is source for @ref tisci_msg_rm_ring_mon_cfg_req::source
 */
#define TISCI_MSG_VALUE_RM_MON_SRC_ELEM_CNT        (0U)
/**
 * Head packet size is source for @ref tisci_msg_rm_ring_mon_cfg_req::source
 */
#define TISCI_MSG_VALUE_RM_MON_SRC_HEAD_PKT_SIZE   (1U)
/**
 * Accumulated queue size is source for @ref tisci_msg_rm_ring_mon_cfg_req::source
 */
#define TISCI_MSG_VALUE_RM_MON_SRC_ACCUM_Q_SIZE    (2U)

/**
 * Disabled monitor mode for @ref tisci_msg_rm_ring_mon_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_DISABLED       (0U)
/**
 * Push/pop statistics capture mode for @ref tisci_msg_rm_ring_mon_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_PUSH_POP       (1U)
/**
 * Low/high threshold checks mode for @ref tisci_msg_rm_ring_mon_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_THRESHOLD      (2U)
/**
 * Low/high watermarking mode for @ref tisci_msg_rm_ring_mon_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_WATERMARK      (3U)
/**
 * Starvation counter mode for @ref tisci_msg_rm_ring_mon_cfg_req::mode
 */
#define TISCI_MSG_VALUE_RM_MON_MODE_STARVATION     (4U)


/* ============================================= */
/*        TISCI RM RA message structures         */
/* ============================================= */

/** Configures a Navigator Subsystem ring */
struct tisci_msg_rm_ring_cfg_req {
    struct tisci_header hdr;
    unsigned int valid_params;
    unsigned short nav_id;
    unsigned short index;
    unsigned int addr_lo;
    unsigned int addr_hi;
    unsigned int count;
    unsigned char mode;
    unsigned char size;
    unsigned char order_id;
    unsigned short virtid;
    unsigned char asel;
} __attribute__((__packed__));

struct tisci_msg_rm_ring_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem ring monitor. */
struct tisci_msg_rm_ring_mon_cfg_req {
    struct tisci_header hdr;
    unsigned int valid_params;
    unsigned short nav_id;
    unsigned short index;
    unsigned char source;
    unsigned char mode;
    unsigned short queue;
    unsigned int data0_val;
    unsigned int data1_val;
} __attribute__((__packed__));

struct tisci_msg_rm_ring_mon_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem proxy */
struct tisci_msg_rm_proxy_cfg_req {
    struct tisci_header hdr;
    unsigned int valid_params;
    unsigned short nav_id;
    unsigned short index;
} __attribute__((__packed__));

struct tisci_msg_rm_proxy_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Retrieves a host's assigned range for a resource
 *
 * Returns the range for a unique resource type assigned to the specified host,
 * or secondary host. The unique resource type is formed by combining the
 * 10 LSB of type and the 6 LSB of subtype.
 */
struct tisci_msg_rm_get_resource_range_req {
    struct tisci_header hdr;
    unsigned short type;
    unsigned char subtype;
    unsigned char secondary_host;
} __attribute__((__packed__));

struct tisci_msg_rm_get_resource_range_resp {
    struct tisci_header hdr;
    unsigned short range_start;
    unsigned short range_num;
    unsigned short range_start_sec;
    unsigned short range_num_sec;
} __attribute__((__packed__));


/* ============================================= */
/*        TISCI RM PSIL message structures       */
/* ============================================= */

/** Pairs a PSI-L source thread to a destination thread */
struct tisci_msg_rm_psil_pair_req {
    struct tisci_header    hdr;
    unsigned int	nav_id;
    unsigned int	src_thread;
    unsigned int	dst_thread;
} __attribute__((__packed__));

struct tisci_msg_rm_psil_pair_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Unpairs a PSI-L source thread from a destination thread */
struct tisci_msg_rm_psil_unpair_req {
    struct tisci_header    hdr;
    unsigned int	nav_id;
    unsigned int	src_thread;
    unsigned int	dst_thread;
} __attribute__((__packed__));

struct tisci_msg_rm_psil_unpair_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Reads the specified thread real-time configuration register from a
 *  specified PSI-L thread using the PSI-L configuration proxy.*/
struct tisci_msg_rm_psil_read_req {
    struct tisci_header    hdr;
    unsigned int	valid_params;
    unsigned short	nav_id;
    unsigned short	thread;
    unsigned short	taddr;
} __attribute__((__packed__));

struct tisci_msg_rm_psil_read_resp {
    struct tisci_header    hdr;
    unsigned int	data;
} __attribute__((__packed__));


/** Write to the specified thread real-time configuration register in a
 *  specified PSI-L thread using the PSI-L configuration proxy. */
struct tisci_msg_rm_psil_write_req {
    struct tisci_header    hdr;
    unsigned int	valid_params;
    unsigned short	nav_id;
    unsigned short	thread;
    unsigned short	taddr;
    unsigned int	data;
} __attribute__((__packed__));

struct tisci_msg_rm_psil_write_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

#endif /* RM_TISCI_RA_H */

/* @} */
