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
 */

#ifndef _TISCI_PROT_H_
#define _TISCI_PROT_H_

#include "../../../types.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

#define BIT(n)  (1UL << (n))

/**
 * This flag is reserved and not to be used.
 */
#define TISCI_MSG_FLAG_RESERVED0    BIT(0)

/**
 * ACK on Processed: Send a response to a message after it has been processed
 * with TISCI_MSG_FLAG_ACK set if the processing succeeded, or a NAK otherwise.
 * This response contains the complete response to the message with the result
 * of the actual action that was requested.
 */
#define TISCI_MSG_FLAG_AOP    BIT(1)

/** Indicate that this message is marked secure */
#define TISCI_MSG_FLAG_SEC    BIT(2)

/**
 * Response flag for a message that indicates success. If this flag is NOT
 * set then that is to be interpreted as a NAK.
 */
#define TISCI_MSG_FLAG_ACK    BIT(1)


/* TISCI Message IDs */
#define TISCI_MSG_VERSION                       (0x0002U)
#define TISCI_MSG_DM_VERSION                    (0x000FU)
#define TISCI_MSG_BOOT_NOTIFICATION             (0x000AU)
#define TISCI_MSG_BOARD_CONFIG                  (0x000BU)
#define TISCI_MSG_BOARD_CONFIG_RM               (0x000CU)
#define TISCI_MSG_BOARD_CONFIG_SECURITY         (0x000DU)
#define TISCI_MSG_BOARD_CONFIG_PM               (0x000EU)

#define TISCI_MSG_RM_GET_RESOURCE_RANGE         (0x1500U)

#define TISCI_MSG_ENABLE_WDT                    (0x0000U)
#define TISCI_MSG_WAKE_RESET                    (0x0001U)
#define TISCI_MSG_WAKE_REASON                   (0x0003U)
#define TISCI_MSG_GOODBYE                       (0x0004U)
#define TISCI_MSG_SYS_RESET                     (0x0005U)

typedef unsigned char domgrp_t;

/* ========================================================================== */
/*                          Structure definitions                             */
/* ========================================================================== */


/** Header that prefixes all TISCI messages sent via secure transport. */
struct tisci_sec_header {
	unsigned short	integ_check;
	unsigned short	rsvd;
};

/** Header that prefixes all TISCI messages. */
struct tisci_header {
    unsigned short   type;
    unsigned char    host;
    unsigned char    seq;
    unsigned int   flags;
};

/** TISCI_MSG_VERSION request to provide version info about currently running firmware.*/
struct tisci_msg_version_req {
	struct tisci_header hdr;
} __attribute__((__packed__));


struct tisci_msg_version_resp {
	struct tisci_header	hdr;
	char			str[32];
	unsigned short	version;
	unsigned char	abi_major;
	unsigned char	abi_minor;
	unsigned char	sub_version;
	unsigned char	patch_version;
} __attribute__((__packed__));

/** Request for TISCI_MSG_SYS_RESET.*/
struct tisci_msg_sys_reset_req {
	struct tisci_header	hdr;
	domgrp_t		domain;
} __attribute__((__packed__));



#endif  /** _TISCI_PROT_H_  */