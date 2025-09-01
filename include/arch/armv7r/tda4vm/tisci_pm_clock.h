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
 * Copyright (C) 2015-2025 Texas Instruments Incorporated
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

#ifndef TISCI_PM_TISCI_CLOCK_H
#define TISCI_PM_TISCI_CLOCK_H

#include "tisci_protocol.h"

/* ========================================================================== */
/*                           Macros & Typedefs                                */
/* ========================================================================== */

/**
 * The IP does not require this clock, it can be disabled, regardless of the
 * state of the device
 */
#define TISCI_MSG_VALUE_CLOCK_SW_STATE_UNREQ        0

/**
 * Allow the system controller to automatically manage the state of this clock.
 * If the device is enabled, then the clock is enabled. If the device is set to
 * off or retention, then the clock is internally set as not being required
 * by the device. This is the default state.
 */
#define TISCI_MSG_VALUE_CLOCK_SW_STATE_AUTO         1

/** Configure the clock to be enabled, regardless of the state of the device. */
#define TISCI_MSG_VALUE_CLOCK_SW_STATE_REQ          2

/** Indicate hardware state of the clock is that it is not running. */
#define TISCI_MSG_VALUE_CLOCK_HW_STATE_NOT_READY    0

/** Indicate hardware state of the clock is that it is running. */
#define TISCI_MSG_VALUE_CLOCK_HW_STATE_READY        1

/**
 * Allow this clock's frequency to be changed while it is running
 * so long as it is within the min/max limits.
 */
#define TISCI_MSG_FLAG_CLOCK_ALLOW_FREQ_CHANGE        BIT(9)


/** TISCI PM Clock messages */
#define TISCI_MSG_SET_CLOCK                     (0x0100U)
#define TISCI_MSG_GET_CLOCK                     (0x0101U)
#define TISCI_MSG_SET_CLOCK_PARENT              (0x0102U)
#define TISCI_MSG_GET_CLOCK_PARENT              (0x0103U)
#define TISCI_MSG_GET_NUM_CLOCK_PARENTS         (0x0104U)
#define TISCI_MSG_SET_FREQ                      (0x010cU)
#define TISCI_MSG_QUERY_FREQ                    (0x010dU)
#define TISCI_MSG_GET_FREQ                      (0x010eU)

/* ========================================================================== */
/*                           ClockID definitions                              */
/* ========================================================================== */
#define TISCI_DEV_MCU_CPSW0_RGMII1_RXC_I 			0
#define TISCI_DEV_MCU_CPSW0_RGMII_MHZ_250_CLK 		1
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK 			2
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_HSDIV4_16FFT_MAIN_3_HSDIVOUT1_CLK			3
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_POSTDIV3_16FFT_MAIN_0_HSDIVOUT6_CLK 		4
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_BOARD_0_MCU_CPTS0_RFT_CLK_OUT 				5
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_BOARD_0_CPTS0_RFT_CLK_OUT 					6
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_BOARD_0_MCU_EXT_REFCLK0_OUT 				7
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_BOARD_0_EXT_REFCLK1_OUT 					8
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_0_IP2_LN1_TXMCLK 			10
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_0_IP2_LN0_TXMCLK 			9
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_1_IP2_LN0_TXMCLK 			11
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_1_IP2_LN1_TXMCLK 			12
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_2_IP2_LN0_TXMCLK 			13
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_2_IP2_LN1_TXMCLK 			14
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_3_IP2_LN0_TXMCLK 			15
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_WIZ16B4M4CS_MAIN_3_IP2_LN1_TXMCLK 			16
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_HSDIV4_16FFT_MCU_2_HSDIVOUT1_CLK 			17
#define TISCI_DEV_MCU_CPSW0_CPTS_RFT_CLK_PARENT_K3_PLL_CTRL_WRAP_WKUP_0_CHIP_DIV1_CLK_CLK2 	18
#define TISCI_DEV_MCU_CPSW0_GMII_RFT_CLK 		19
#define TISCI_DEV_MCU_CPSW0_RMII_MHZ_50_CLK 	20
#define TISCI_DEV_MCU_CPSW0_RGMII_MHZ_50_CLK 	21
#define TISCI_DEV_MCU_CPSW0_CPPI_CLK_CLK 		22
#define TISCI_DEV_MCU_CPSW0_RGMII_MHZ_5_CLK 	23
#define TISCI_DEV_MCU_CPSW0_GMII1_MR_CLK 		24
#define TISCI_DEV_MCU_CPSW0_GMII1_MT_CLK 		25
#define TISCI_DEV_MCU_CPSW0_RGMII1_TXC_I 		26
#define TISCI_DEV_MCU_CPSW0_RGMII1_TXC_O 		27
#define TISCI_DEV_MCU_CPSW0_CPTS_GENF0 			28
#define TISCI_DEV_MCU_CPSW0_MDIO_MDCLK_O 		29


/* ========================================================================== */
/*                           DeviceID definitions                             */
/* ========================================================================== */
#define J721E_DEV_MCU_CPSW0 	18U

/* ========================================================================== */
/*                          Structure definitions                             */
/* ========================================================================== */

/** Mark a clock as required/not required.*/
struct tisci_msg_set_clock_req {
    struct tisci_header hdr;
    unsigned int   device;
    unsigned char   clk;
    unsigned char   state;
    unsigned int   clk32;
} __attribute__((__packed__));

struct tisci_msg_set_clock_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));


/** Get the current state of a clock */
struct tisci_msg_get_clock_req {
    struct tisci_header	hdr;
    unsigned int    device;
    unsigned char    clk;
    unsigned int    clk32;
} __attribute__((__packed__));

struct tisci_msg_get_clock_resp {
    struct tisci_header	hdr;
    unsigned char   programmed_state;
    unsigned char   current_state;
} __attribute__((__packed__));

/** Set the clock parent */
struct tisci_msg_set_clock_parent_req {
    struct tisci_header hdr;
    unsigned int   device;
    unsigned char   clk;
    unsigned char   parent;
    unsigned int   clk32;
    unsigned int   parent32;
} __attribute__((__packed__));

struct tisci_msg_set_clock_parent_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));


/** Return the current clock parent */
struct tisci_msg_get_clock_parent_req {
    struct tisci_header	hdr;
    unsigned int   device;
    unsigned char   clk;
    unsigned int   clk32;
} __attribute__((__packed__));

struct tisci_msg_get_clock_parent_resp {
    struct tisci_header	hdr;
    unsigned char   parent;
    unsigned int   parent32;
} __attribute__((__packed__));


/**Return the number of possible parents for a clock */
struct tisci_msg_get_num_clock_parents_req {
    struct tisci_header	hdr;
    unsigned int   device;
    unsigned char   clk;
    unsigned int   clk32;
} __attribute__((__packed__));

struct tisci_msg_get_num_clock_parents_resp {
    struct tisci_header	hdr;
    unsigned char   num_parents;
    unsigned int   num_parentint32_t;
} __attribute__((__packed__));


/** Set the desired frequency for a clock. */
struct tisci_msg_set_freq_req {
    struct tisci_header	hdr;
    unsigned int  device;
    unsigned long long  min_freq_hz;
    unsigned long long  target_freq_hz;
    unsigned long long  max_freq_hz;
    unsigned char  clk;
    unsigned int  clk32;
} __attribute__((__packed__));

struct tisci_msg_set_freq_resp {
    struct tisci_header hdr;
} __attribute__((__packed__));


/** Determine the result of a hypothetical set frequency operation.*/
struct tisci_msg_query_freq_req {
    struct tisci_header	hdr;
    unsigned int  device;
    unsigned long long  min_freq_hz;
    unsigned long long  target_freq_hz;
    unsigned long long  max_freq_hz;
    unsigned char  clk;
    unsigned int  clk32;
} __attribute__((__packed__));

struct tisci_msg_query_freq_resp {
    struct tisci_header	hdr;
    unsigned long long  freq_hz;
} __attribute__((__packed__));


/**Get the current frequency of a device's clock*/
struct tisci_msg_get_freq_req {
    struct tisci_header hdr;
    unsigned int   device;
    unsigned char   clk;
    unsigned int   clk32;
} __attribute__((__packed__));

struct tisci_msg_get_freq_resp {
    struct tisci_header hdr;
    unsigned long long   freq_hz;
} __attribute__((__packed__));

#endif /* TISCI_PM_TISCI_CLOCK_H */

/* @} */
