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

#ifndef RM_TISCI_UDMA_H
#define RM_TISCI_UDMA_H

/* ========================================================================== */
/*                           Macros and typedefs                              */
/* ========================================================================== */

/** TISCI RM UDMA messages */
#define TISCI_MSG_RM_UDMAP_TX_CH_CFG            	(0x1205U)
#define TISCI_MSG_RM_UDMAP_RX_CH_CFG            	(0x1215U)
#define TISCI_MSG_RM_UDMAP_FLOW_CFG             	(0x1230U)
#define TISCI_MSG_RM_UDMAP_FLOW_SIZE_THRESH_CFG 	(0x1231U)
#define TISCI_MSG_RM_UDMAP_FLOW_DELEGATE        	(0x1234U)
#define TISCI_MSG_RM_UDMAP_GCFG_CFG             	(0x1240U)

/** UDMAP subtypes definitions */
#define TISCI_RESASG_SUBTYPE_UDMAP_RX_FLOW_COMMON 		(0x0000U)
#define TISCI_RESASG_SUBTYPE_UDMAP_INVALID_FLOW_OES 	(0x0001U)
#define TISCI_RESASG_SUBTYPE_GLOBAL_EVENT_TRIGGER 		(0x0002U)
#define TISCI_RESASG_SUBTYPE_UDMAP_GLOBAL_CONFIG 		(0x0003U)
#define TISCI_RESASG_SUBTYPE_UDMAP_RX_CHAN 				(0x000AU)
#define TISCI_RESASG_SUBTYPE_UDMAP_RX_HCHAN 			(0x000BU)
#define TISCI_RESASG_SUBTYPE_UDMAP_RX_UHCHAN 			(0x000CU)
#define TISCI_RESASG_SUBTYPE_UDMAP_TX_CHAN 				(0x000DU)
#define TISCI_RESASG_SUBTYPE_UDMAP_TX_ECHAN 			(0x000EU)
#define TISCI_RESASG_SUBTYPE_UDMAP_TX_HCHAN 			(0x000FU)
#define TISCI_RESASG_SUBTYPE_UDMAP_TX_UHCHAN 			(0x0010U)
#define TISCI_RESASG_SUBTYPE_BCDMA_SPLIT_TR_RX_CHAN 	(0x0021U)
#define TISCI_RESASG_SUBTYPE_BCDMA_SPLIT_TR_TX_CHAN 	(0x0022U)
#define TISCI_RESASG_SUBTYPES_UDMAP_CNT 				(0x000DU)

/** Valid field macros */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERR_VALID         (1u << 0u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_VALID                (1u << 1u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_CHAN_TYPE_VALID            (1u << 2u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_VALID           (1u << 3u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_CQ_QNUM_VALID              (1u << 4u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_PRIORITY_VALID             (1u << 5u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_QOS_VALID                  (1u << 6u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ORDER_ID_VALID             (1u << 7u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_SCHED_PRIORITY_VALID       (1u << 8u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_BURST_SIZE_VALID           (1u << 14U)
#define TISCI_MSG_VALUE_RM_UDMAP_EXTENDED_CH_TYPE_VALID        (1u << 16U)

/** On error behavior */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERROR_DISABLED    (0u)
#define TISCI_MSG_VALUE_RM_UDMAP_CH_PAUSE_ON_ERROR_ENABLED     (1u)

/** Descriptor extended packet info (if present) state */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_FILT_EINFO_DISABLED     (0u)
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_FILT_EINFO_ENABLED      (1u)

/** Descriptor protocol specific info (if present) state */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_FILT_PSWORDS_DISABLED      (0u)
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_FILT_PSWORDS_ENABLED       (1u)

/**Pointers are physical addresses configuration for
 * tisci_msg_rm_udmap_tx_ch_cfg_req::tx_atype and
 * tisci_msg_rm_udmap_rx_ch_cfg_req::rx_atype parameters.*/
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_PHYS                 (0u)

/** Pointers are intermediate addresses requiring intermediate to
 *  physical translation before being decoded */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_INTERMEDIATE         (1u)

/** Pointers are virtual addresses requiring virtual to physical
 *  translation before being decoded configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_VIRTUAL              (2u)

/** Used for all non-coherent traffic like accelerator and real-time IP
 *  traffic. */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ATYPE_NON_COHERENT         (3U)


/**Channel performs packet oriented transfers using pass by reference
 * rings configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_PACKET                (2u)

/**  Channel performs packet oriented transfers using pass by reference
 * rings with single buffer packet mode enable configuration
 *
 *  NOTE: This type is only valid for UDMAP receive channel
 *        configuration
 */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_PACKET_SINGLE_BUF     (3u)
/**
 * Channel performs Third Party DMA transfers using pass by reference
 * rings configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_3P_DMA_REF            (10u)

/** Channel performs Third Party DMA transfers using pass by value
 *  rings configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_3P_DMA_VAL            (11u)

/** Channel performs Third Party Block Copy DMA transfers using pass by
 * reference rings configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_3P_BLOCK_REF          (12u)

/** Channel performs Third Party Block Copy DMA transfers using pass by
 * value rings configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_TYPE_3P_BLOCK_VAL          (13u)

/** Do not suppress teardown packet generation by transmit channel */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_SUPPRESS_TD_DISABLED    (0u)

/** Suppress teardown packet generation by transmit channel */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_SUPPRESS_TD_ENABLED     (1u)

/** Maximum value allowed in tisci_msg_rm_udmap_tx_ch_cfg_req::tx_fetch_size
 * and tisci_msg_rm_udmap_rx_ch_cfg_req::rx_fetch_size */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_FETCH_SIZE_MAX             (127u)

/** Maximum value allowed in tisci_msg_rm_udmap_tx_ch_cfg_req::tx_credit_count */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_CREDIT_CNT_MAX          (7u)

/** Maximum value allowed in tisci_msg_rm_udmap_tx_ch_cfg_req::tx_priority
 *  and tisci_msg_rm_udmap_rx_ch_cfg_req::rx_priority */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_PRIORITY_MAX               (7u)

/** Maximum value allowed in tisci_msg_rm_udmap_tx_ch_cfg_req::tx_qos
 *  and tisci_msg_rm_udmap_rx_ch_cfg_req::rx_qos */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_QOS_MAX                    (7u)

/* Maximum value allowed in tisci_msg_rm_udmap_tx_ch_cfg_req::tx_orderid
 * and tisci_msg_rm_udmap_rx_ch_cfg_req::rx_orderid */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_ORDER_ID_MAX               (15u)

/* High priority scheduling priority configuration for
 * tisci_msg_rm_udmap_tx_ch_cfg_req::tx_sched_priority and
 * tisci_msg_rm_udmap_rx_ch_cfg_req::rx_sched_priority parameters. */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_SCHED_PRIOR_HIGH           (0u)

/** Medium to high priority scheduling priority configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_SCHED_PRIOR_MEDHIGH        (1u)

/** Medium to low priority scheduling priority configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_SCHED_PRIOR_MEDLOW         (2u)

/** Low priority scheduling priority configuration */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_SCHED_PRIOR_LOW            (3u)

/** Burst size of 64 bytes in
 * tisci_msg_rm_udmap_tx_ch_cfg_req::tx_burst_size
 * and tisci_msg_rm_udmap_rx_ch_cfg_req::rx_burst_size
 */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_BURST_SIZE_64_BYTES        (1U)

/** Burst size of 128 bytes */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_BURST_SIZE_128_BYTES       (2U)

/** Burst size of 256 bytes */
#define TISCI_MSG_VALUE_RM_UDMAP_CH_BURST_SIZE_256_BYTES       (3U)

/** Teardown immediately once all traffic is complete in UDMA
 *  tisci_msg_rm_udmap_tx_ch_cfg_req::tx_tdtype */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_TDTYPE_IMMEDIATE        (0U)

/** Wait to teardown until remote peer sends back completion message */
#define TISCI_MSG_VALUE_RM_UDMAP_TX_CH_TDTYPE_WAIT             (1U)


/* ========================================================================== */
/*                           DeviceID definitions                             */
/* ========================================================================== */
#define J721E_DEV_MCU_NAVSS0_UDMAP_0	(236U)



/* ============================================= */
/*        TISCI RM UDMA message structures       */
/* ============================================= */

/** Configures a Navigator Subsystem UDMAP global configuration region.*/
 struct tisci_msg_rm_udmap_gcfg_cfg_req {
    struct tisci_header    hdr;
    unsigned int	valid_params;
    unsigned short	nav_id;
    unsigned int	perf_ctrl;
    unsigned int	emu_ctrl;
    unsigned int	psil_to;
    unsigned int	rflowfwstat;
} __attribute__((__packed__));

struct tisci_msg_rm_udmap_gcfg_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem UDMAP transmit channel */
struct tisci_msg_rm_udmap_tx_ch_cfg_req {
    struct tisci_header    hdr;
    unsigned int	 valid_params;
    unsigned short	 nav_id;
    unsigned short	 index;
    unsigned char 	tx_pause_on_err;
    unsigned char 	tx_filt_einfo;
    unsigned char 	tx_filt_pswords;
    unsigned char 	tx_atype;
    unsigned char 	tx_chan_type;
    unsigned char 	tx_supr_tdpkt;
    unsigned short	 tx_fetch_size;
    unsigned char 	tx_credit_count;
    unsigned short	 txcq_qnum;
    unsigned char 	tx_priority;
    unsigned char 	tx_qos;
    unsigned char 	tx_orderid;
    unsigned short	 fdepth;
    unsigned char 	tx_sched_priority;
    unsigned char 	tx_burst_size;
    unsigned char 	tx_tdtype;
    unsigned char 	extended_ch_type;
} __attribute__((__packed__));

struct tisci_msg_rm_udmap_tx_ch_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem UDMAP receive channel */
struct tisci_msg_rm_udmap_rx_ch_cfg_req {
    struct tisci_header    hdr;
    unsigned int	 valid_params;
    unsigned short	 nav_id;
    unsigned short	 index;
    unsigned short	 rx_fetch_size;
    unsigned short	 rxcq_qnum;
    unsigned char 	rx_priority;
    unsigned char 	rx_qos;
    unsigned char 	rx_orderid;
    unsigned char 	rx_sched_priority;
    unsigned short	 flowid_start;
    unsigned short	 flowid_cnt;
    unsigned char 	rx_pause_on_err;
    unsigned char 	rx_atype;
    unsigned char 	rx_chan_type;
    unsigned char 	rx_ignore_short;
    unsigned char 	rx_ignore_long;
    unsigned char 	rx_burst_size;
} __attribute__((__packed__));

struct tisci_msg_rm_udmap_rx_ch_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem UDMAP receive flow */
struct tisci_msg_rm_udmap_flow_cfg_req {
    struct tisci_header    hdr;
    unsigned int	 valid_params;
    unsigned short	 nav_id;
    unsigned short	 flow_index;
    unsigned char 	rx_einfo_present;
    unsigned char 	rx_psinfo_present;
    unsigned char 	rx_error_handling;
    unsigned char 	rx_desc_type;
    unsigned short	 rx_sop_offset;
    unsigned short	 rx_dest_qnum;
    unsigned char 	rx_src_tag_hi;
    unsigned char 	rx_src_tag_lo;
    unsigned char 	rx_dest_tag_hi;
    unsigned char 	rx_dest_tag_lo;
    unsigned char 	rx_src_tag_hi_sel;
    unsigned char 	rx_src_tag_lo_sel;
    unsigned char 	rx_dest_tag_hi_sel;
    unsigned char 	rx_dest_tag_lo_sel;
    unsigned short	 rx_fdq0_sz0_qnum;
    unsigned short	 rx_fdq1_qnum;
    unsigned short	 rx_fdq2_qnum;
    unsigned short	 rx_fdq3_qnum;
    unsigned char 	rx_ps_location;
} __attribute__((__packed__));

struct tisci_msg_rm_udmap_flow_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

/** Configures a Navigator Subsystem UDMAP receive flow's size threshold fields. */
struct tisci_msg_rm_udmap_flow_size_thresh_cfg_req {
    struct tisci_header    hdr;
    unsigned int	 valid_params;
    unsigned short	 nav_id;
    unsigned short	 flow_index;
    unsigned short	 rx_size_thresh0;
    unsigned short	 rx_size_thresh1;
    unsigned short	 rx_size_thresh2;
    unsigned short	 rx_fdq0_sz1_qnum;
    unsigned short	 rx_fdq0_sz2_qnum;
    unsigned short	 rx_fdq0_sz3_qnum;
    unsigned char 	rx_size_thresh_en;
} __attribute__((__packed__));


struct tisci_msg_rm_udmap_flow_size_thresh_cfg_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));

#endif //RM_TISCI_UDMA_H 
