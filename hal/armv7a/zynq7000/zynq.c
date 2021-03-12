/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * zynq-7000 basic peripherals control functions
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "pmap.h"
#include "../../include/errno.h"
#include "vm/map.h"



/* SLCR (System Level Control Registers) */
enum {
	/* SLCR protection registers */
	slcr_scl = 0, slcr_lock, slcr_unlock, slcr_locksta,
	/* PLL configuration registers */
	slcr_arm_pll_ctrl = 0x40 ,slcr_ddr_pll_ctrl, slcr_io_pll_ctrl, slcr_pll_status, slcr_arm_pll_cfg, slcr_ddr_pll_cfg, slcr_io_pll_cfg,
	/* Clock control registers */
	slcr_arm_clk_ctrl = 0x48, slcr_ddr_clk_ctrl, slcr_dci_clk_ctrl, slcr_aper_clk_ctrl, slcr_usb0_clk_ctrl, slcr_usb1_clk_ctrl, slcr_gem0_rclk_ctrl,
	slcr_gem1_rclk_ctrl, slcr_gem0_clk_ctrl, slcr_gem1_clk_ctrl, slcr_smc_clk_ctrl, slcr_lqspi_clk_ctrl, slcr_sdio_clk_ctrl, slcr_uart_clk_ctrl,
	slcr_spi_clk_ctrl, slcr_can_clk_ctrl, slcr_can_mioclk_ctrl, slcr_dbg_clk_ctrl, slcr_pcap_clk_ctrl, slcr_topsw_clk_ctrl, slcr_fpga0_clk_ctrl,
	/* FPGA configuration registers */
	slcr_fpga0_thr_ctrl, slcr_fpga0_thr_cnt, slcr_fpga0_thr_sta, slcr_fpga1_clk_ctrl, slcr_fpga1_thr_ctrl, slcr_fpga1_thr_cnt, slcr_fpga1_thr_sta,
	slcr_fpga2_clk_ctrl, slcr_fpga2_thr_ctrl, slcr_fpga2_thr_cnt, slcr_fpga2_thr_sta, slcr_fpga3_clk_ctrl, slcr_fpga3_thr_ctrl, slcr_fpga3_thr_cnt,
	slcr_fpga3_thr_sta,
	/* Clock ratio register */
	slcr_clk_621_true = 0x71,
	/* Reset registers */
	slcr_pss_rst_ctrl = 0x80, slcr_ddr_rst_ctrl, slcr_topsw_rst_ctrl, slcr_dmac_rst_ctrl, slcr_usb_rst_ctrl, slcr_gem_rst_ctrl, slcr_sdio_rst_ctrl,
	slcr_spi_rst_ctrl, slcr_can_rst_ctrl, slcr_i2c_rst_ctrl, slcr_uart_rst_ctrl, slcr_gpio_rst_ctrl, slcr_lqspi_rst_ctrl, slcr_smc_rst_ctrl, slcr_ocm_rst_ctrl,
	slcr_fpga_rst_ctrl = 0x90, slcr_a9_cpu_rst_ctrl,
	/* APU watchdog register */
	slcr_rs_awdt_rst_ctrl = 0x93,
	slcr_reboot_status = 0x96, slcr_boot_mode,
	slcr_apu_control = 0xc0, slcr_wdt_clk_sel,
	slcr_tz_dma_ns = 0x110, slcr_tz_dma_irq_ns, slcr_tz_dma_periph_ns,
	slcr_pss_idcode = 0x14c,
	slcr_ddr_urgent = 0x180,
	slcr_ddr_cal_start = 0x183,
	slcr_ddr_ref_start = 0x185, slcr_ddr_cmd_sta, slcr_ddr_urgent_sel, slcr_ddr_dfi_status,
	/* MIO pins config registers */
	slcr_mio_pin_00 = 0x1c0, slcr_mio_pin_01, slcr_mio_pin_02, slcr_mio_pin_03, slcr_mio_pin_04, slcr_mio_pin_05, slcr_mio_pin_06, slcr_mio_pin_07, slcr_mio_pin_08,
	slcr_mio_pin_09, slcr_mio_pin_10, slcr_mio_pin_11, slcr_mio_pin_12, slcr_mio_pin_13, slcr_mio_pin_14, slcr_mio_pin_15, slcr_mio_pin_16, slcr_mio_pin_17,
	slcr_mio_pin_18, slcr_mio_pin_19, slcr_mio_pin_20, slcr_mio_pin_21, slcr_mio_pin_22, slcr_mio_pin_23, slcr_mio_pin_24, slcr_mio_pin_25, slcr_mio_pin_26,
	slcr_mio_pin_27, slcr_mio_pin_28, slcr_mio_pin_29, slcr_mio_pin_30, slcr_mio_pin_31, slcr_mio_pin_32, slcr_mio_pin_33, slcr_mio_pin_34, slcr_mio_pin_35,
	slcr_mio_pin_36, slcr_mio_pin_37, slcr_mio_pin_38, slcr_mio_pin_39, slcr_mio_pin_40, slcr_mio_pin_41, slcr_mio_pin_42, slcr_mio_pin_43, slcr_mio_pin_44,
	slcr_mio_pin_45, slcr_mio_pin_46, slcr_mio_pin_47, slcr_mio_pin_48, slcr_mio_pin_49, slcr_mio_pin_50, slcr_mio_pin_51, slcr_mio_pin_52, slcr_mio_pin_53,
	slcr_mio_loopback = 0x201,
	slcr_mio_mst_tri0 = 0x203, slcr_mio_mst_tri1,
	slcr_sd0_wp_cd_sel = 0x20c, slcr_sd1_wp_cd_sel,
	slcr_lvl_shftr_en = 0x240,
	slcr_ocm_cfg = 0x244,
	/* GPIO config registers */
	slcr_gpiob_ctrl = 0x2c0, slcr_gpiob_cfg_cmos18, slcr_gpiob_cfg_cmos25, slcr_gpiob_cfg_cmos33,
	slcr_gpiob_cfg_hstl = 0x2c5, slcr_gpiob_drvr_bias_ctrl,
	/* DDR config registers */
	slcr_ddriob_addr0 = 0x2d0, slcr_ddriob_addr1, slcr_ddriob_data0, slcr_ddriob_data1, slcr_ddriob_diff0, slcr_ddriob_diff1, slcr_ddriob_clock, slcr_ddriob_drive_slew_addr,
	slcr_ddriob_drive_slew_data, slcr_ddriob_drive_slew_diff, slcr_ddriob_drive_slew_clock, slcr_ddriob_ddr_ctrl, slcr_ddriob_dci_ctrl, slcr_ddriob_dci_status,
};




struct {
	volatile u32 *slcr;
} zynq_common;


extern void _end(void);


static void _zynq_slcrLock(void)
{
	*(zynq_common.slcr + slcr_lock) = 0x0000767b;
}


static void _zynq_slcrUnlock(void)
{
	*(zynq_common.slcr + slcr_unlock) = 0x0000df0d;
}


static void _zynq_peripherals(void)
{
	_zynq_slcrUnlock();

	/* UART_0 Initialization
	 * TxD
	 * TRI_ENABLE = 0; L0_SEL = 0; L1_SEL = 0; L2_SEL = 0; L3_SEL = 7; Speed = 0; IO_Type = 1; PULLUP = 0; DisableRcvr = 0 */
	*(zynq_common.slcr + slcr_mio_pin_11) = (*(zynq_common.slcr + slcr_mio_pin_11) & ~0x00003fff) | 0x000002e0;

	/* RxD
	 * TRI_ENABLE = 1; L0_SEL = 0; L1_SEL = 0; L2_SEL = 0; L3_SEL = 7; Speed = 0; IO_Type = 1; PULLUP = 0; DisableRcvr = 0 */
	*(zynq_common.slcr + slcr_mio_pin_10) = (*(zynq_common.slcr + slcr_mio_pin_10) & ~0x00003fff) | 0x000002e1;


	/* UART_1 Initialization
	 * TxD
	 * TRI_ENABLE = 0; L0_SEL = 0; L1_SEL = 0; L2_SEL = 0; L3_SEL = 7; Speed = 0; IO_Type = 1; PULLUP = 0; DisableRcvr = 0 */
	*(zynq_common.slcr + slcr_mio_pin_48) = (*(zynq_common.slcr + slcr_mio_pin_48) & ~0x00003fff) | 0x000002e0;

	/* RxD
	 * TRI_ENABLE = 1; L0_SEL = 0; L1_SEL = 0; L2_SEL = 0; L3_SEL = 7; Speed = 0; IO_Type = 1; PULLUP = 0; DisableRcvr = 0 */
	*(zynq_common.slcr + slcr_mio_pin_49) = (*(zynq_common.slcr + slcr_mio_pin_49) & ~0x00003fff) | 0x000002e1;


	/* Define UARTs' clocks speed
	 * IO_PLL / 20 :  1000 MHz / 20 = 50 MHz
	 * CLKACT0 = 0x0; CLKACT1 = 0x1; SRCSEL = 0x0; DIVISOR = 0x14 */
	*(zynq_common.slcr + slcr_uart_clk_ctrl) = (*(zynq_common.slcr + slcr_uart_clk_ctrl) & ~0x00003f33) | 0x00001402 | 0x1;

	/* Enable UART_0 & UART_1 clock */
	*(zynq_common.slcr + slcr_aper_clk_ctrl) = *(zynq_common.slcr + slcr_aper_clk_ctrl) | (1 << 21) | (1 << 20);

	_zynq_slcrLock();
}


int hal_platformctl(void *ptr)
{
	return 0;
}


void _hal_platformInit(void)
{
	zynq_common.slcr = (void *)(((u32)_end + 7 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));


	/* Initialize basic peripherals - MIO & Clk:
	 *  -  UART_1       : ref_clk = 50 MHz based on TRM                                */
	_zynq_peripherals();
}
