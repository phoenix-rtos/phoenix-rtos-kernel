/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ZynqMP register definitions
 * based on: Zynq UltraScale+ Devices Register Reference UG1087 (v1.9)
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _ZYNQMP_REGS_H_
#define _ZYNQMP_REGS_H_

enum {
	iou_slcr_mio_pin_0 = 0x0,    /* 78 registers */
	iou_slcr_bank0_ctrl0 = 0x4e, /* 6 registers */
	iou_slcr_bank0_status = 0x54,
	iou_slcr_bank1_ctrl0, /* 6 registers */
	iou_slcr_bank1_status = 0x5b,
	iou_slcr_bank2_ctrl0, /* 6 registers */
	iou_slcr_bank2_status = 0x62,
	iou_slcr_mio_loopback = 0x80,
	iou_slcr_mio_mst_tri0, /* 3 registers */
	iou_slcr_wdt_clk_sel = 0xc0,
	iou_slcr_can_mio_ctrl,
	iou_slcr_gem_clk_ctrl,
	iou_slcr_sdio_clk_ctrl,
	iou_slcr_ctrl_reg_sd,
	iou_slcr_sd_itapdly,
	iou_slcr_sd_otapdlysel,
	iou_slcr_sd_config_reg1,
	iou_slcr_sd_config_reg2,
	iou_slcr_sd_config_reg3,
	iou_slcr_sd_initpreset,
	iou_slcr_sd_dsppreset,
	iou_slcr_sd_hspdpreset,
	iou_slcr_sd_sdr12preset,
	iou_slcr_sd_sdr25preset,
	iou_slcr_sd_sdr50prset,
	iou_slcr_sd_sdr104prst,
	iou_slcr_sd_ddr50preset,
	iou_slcr_sd_maxcur1p8 = 0xd3,
	iou_slcr_sd_maxcur3p0,
	iou_slcr_sd_maxcur3p1,
	iou_slcr_sd_dll_ctrl,
	iou_slcr_sd_cdn_ctrl,
	iou_slcr_gem_ctrl,
	iou_slcr_iou_ttc_apb_clk = 0xe0,
	iou_slcr_iou_tapdly_bypass = 0xe4,
	iou_slcr_iou_coherent_ctrl = 0x100,
	iou_slcr_video_pss_clk_sel,
	iou_slcr_iou_interconnect_route,
	iou_slcr_ctrl = 0x180,
	iou_slcr_isr = 0x1c0,
	iou_slcr_imr,
	iou_slcr_ier,
	iou_slcr_idr,
	iou_slcr_itr,
};

enum {
	apu_err_ctrl = 0x0,
	apu_isr = 0x4,
	apu_imr,
	apu_ien,
	apu_ids,
	apu_config_0,
	apu_config_1,
	apu_rvbaraddr0l = 0x10,
	apu_rvbaraddr0h,
	apu_rvbaraddr1l,
	apu_rvbaraddr1h,
	apu_rvbaraddr2l,
	apu_rvbaraddr2h,
	apu_rvbaraddr3l,
	apu_rvbaraddr3h,
	apu_ace_ctrl,
	apu_snoop_ctrl = 0x20,
	apu_pwrctl = 0x24,
	apu_pwrstat,
};

enum {
	crf_apb_err_ctrl = 0x0,
	crf_apb_ir_status,
	crf_apb_ir_mask,
	crf_apb_ir_enable,
	crf_apb_ir_disable,
	crf_apb_crf_wprot = 0x7,
	crf_apb_apll_ctrl,
	crf_apb_apll_cfg,
	crf_apb_apll_frac_cfg,
	crf_apb_dpll_ctrl,
	crf_apb_dpll_cfg,
	crf_apb_dpll_frac_cfg,
	crf_apb_vpll_ctrl,
	crf_apb_vpll_cfg,
	crf_apb_vpll_frac_cfg,
	crf_apb_pll_status,
	crf_apb_apll_to_lpd_ctrl,
	crf_apb_dpll_to_lpd_ctrl,
	crf_apb_vpll_to_lpd_ctrl,
	crf_apb_acpu_ctrl = 0x18,
	crf_apb_dbg_trace_ctrl,
	crf_apb_dbg_fpd_ctrl,
	crf_apb_dp_video_ref_ctrl = 0x1c,
	crf_apb_dp_audio_ref_ctrl,
	crf_apb_dp_stc_ref_ctrl = 0x1f,
	crf_apb_ddr_ctrl,
	crf_apb_gpu_ref_ctrl,
	crf_apb_sata_ref_ctrl = 0x28,
	crf_apb_pcie_ref_ctrl = 0x2d,
	crf_apb_fpd_dma_ref_ctrl,
	crf_apb_dpdma_ref_ctrl,
	crf_apb_topsw_main_ctrl,
	crf_apb_topsw_lsbus_ctrl,
	crf_apb_dbg_tstmp_ctrl = 0x3e,
	crf_apb_rst_fpd_top = 0x40,
	crf_apb_rst_fpd_apu,
	crf_apb_rst_ddr_ss,
};

enum {
	crl_apb_err_ctrl = 0x0,
	crl_apb_ir_status,
	crl_apb_ir_mask,
	crl_apb_ir_enable,
	crl_apb_ir_disable,
	crl_apb_crl_wprot = 0x7,
	crl_apb_iopll_ctrl,
	crl_apb_iopll_cfg,
	crl_apb_iopll_frac_cfg,
	crl_apb_rpll_ctrl = 0xc,
	crl_apb_rpll_cfg,
	crl_apb_rpll_frac_cfg,
	crl_apb_pll_status = 0x10,
	crl_apb_iopll_to_fpd_ctrl,
	crl_apb_rpll_to_fpd_ctrl,
	crl_apb_usb3_dual_ref_ctrl,
	crl_apb_gem0_ref_ctrl,
	crl_apb_gem1_ref_ctrl,
	crl_apb_gem2_ref_ctrl,
	crl_apb_gem3_ref_ctrl,
	crl_apb_usb0_bus_ref_ctrl,
	crl_apb_usb1_bus_ref_ctrl,
	crl_apb_qspi_ref_ctrl,
	crl_apb_sdio0_ref_ctrl,
	crl_apb_sdio1_ref_ctrl,
	crl_apb_uart0_ref_ctrl,
	crl_apb_uart1_ref_ctrl,
	crl_apb_spi0_ref_ctrl,
	crl_apb_spi1_ref_ctrl,
	crl_apb_can0_ref_ctrl,
	crl_apb_can1_ref_ctrl,
	crl_apb_cpu_r5_ctrl = 0x24,
	crl_apb_iou_switch_ctrl = 0x27,
	crl_apb_csu_pll_ctrl,
	crl_apb_pcap_ctrl,
	crl_apb_lpd_switch_ctrl,
	crl_apb_lpd_lsbus_ctrl,
	crl_apb_dbg_lpd_ctrl,
	crl_apb_nand_ref_ctrl,
	crl_apb_lpd_dma_ref_ctrl,
	crl_apb_pl0_ref_ctrl = 0x30,
	crl_apb_pl1_ref_ctrl,
	crl_apb_pl2_ref_ctrl,
	crl_apb_pl3_ref_ctrl,
	crl_apb_pl0_thr_ctrl,
	crl_apb_pl0_thr_cnt,
	crl_apb_pl1_thr_ctrl,
	crl_apb_pl1_thr_cnt,
	crl_apb_pl2_thr_ctrl,
	crl_apb_pl2_thr_cnt,
	crl_apb_pl3_thr_ctrl,
	crl_apb_pl3_thr_cnt = 0x3f,
	crl_apb_gem_tsu_ref_ctrl,
	crl_apb_dll_ref_ctrl,
	crl_apb_pssysmon_ref_ctrl,
	crl_apb_i2c0_ref_ctrl = 0x48,
	crl_apb_i2c1_ref_ctrl,
	crl_apb_timestamp_ref_ctrl,
	crl_apb_safety_chk = 0x4c,
	crl_apb_clkmon_status = 0x50,
	crl_apb_clkmon_mask,
	crl_apb_clkmon_enable,
	crl_apb_clkmon_disable,
	crl_apb_clkmon_trigger,
	crl_apb_chkr0_clka_upper = 0x58, /* 8 banks of 4 registers */
	crl_apb_chkr0_clka_lower,
	crl_apb_chkr0_clkb_cnt,
	crl_apb_chkr0_ctrl,
	crl_apb_boot_mode_user = 0x80,
	crl_apb_boot_mode_por,
	crl_apb_reset_ctrl = 0x86,
	crl_apb_blockonly_rst,
	crl_apb_reset_reason,
	crl_apb_rst_lpd_iou0 = 0x8c,
	crl_apb_rst_lpd_iou2 = 0x8e,
	crl_apb_rst_lpd_top,
	crl_apb_rst_lpd_dbg,
	crl_apb_boot_pin_ctrl = 0x94,
	crl_apb_bank3_ctrl0 = 0x9c, /* 6 registers */
	crl_apb_bank3_status = 0xa2,
};

enum {
	ddrc_mstr = 0x0,
	ddrc_stat,
	ddrc_mrctrl0 = 0x4,
	ddrc_mrctrl1,
	ddrc_mrstat,
	ddrc_mrctrl2,
	ddrc_derateen,
	ddrc_derateint,
	ddrc_pwrctl = 0xc,
	ddrc_pwrtmg,
	ddrc_hwlpctl,
	ddrc_rfshctl0 = 0x14,
	ddrc_rfshctl1,
	ddrc_rfshctl3 = 0x18,
	ddrc_rfshtmg,
	ddrc_ecccfg0 = 0x1c,
	ddrc_ecccfg1,
	ddrc_eccstat,
	ddrc_eccclr,
	ddrc_eccerrcnt,
	ddrc_ecccaddr0,
	ddrc_ecccaddr1,
	ddrc_ecccsyn0,
	ddrc_ecccsyn1,
	ddrc_ecccsyn2,
	ddrc_eccbitmask0,
	ddrc_eccbitmask1,
	ddrc_eccbitmask2,
	ddrc_eccuaddr0,
	ddrc_eccuaddr1,
	ddrc_eccusyn0,
	ddrc_eccusyn1,
	ddrc_eccusyn2,
	ddrc_eccpoisonaddr0,
	ddrc_eccpoisonaddr1,
	ddrc_crcparctl0,
	ddrc_crcparctl1,
	ddrc_crcparctl2,
	ddrc_crcparstat,
	ddrc_init0,
	ddrc_init1,
	ddrc_init2,
	ddrc_init3,
	ddrc_init4,
	ddrc_init5,
	ddrc_init6,
	ddrc_init7,
	ddrc_dimmctl,
	ddrc_rankctl,
	ddrc_dramtmg0 = 0x40,
	ddrc_dramtmg1,
	ddrc_dramtmg2,
	ddrc_dramtmg3,
	ddrc_dramtmg4,
	ddrc_dramtmg5,
	ddrc_dramtmg6,
	ddrc_dramtmg7,
	ddrc_dramtmg8,
	ddrc_dramtmg9,
	ddrc_dramtmg10,
	ddrc_dramtmg11,
	ddrc_dramtmg12,
	ddrc_dramtmg13,
	ddrc_dramtmg14,
	ddrc_zqctl0 = 0x60,
	ddrc_zqctl1,
	ddrc_zqctl2,
	ddrc_zqstat,
	ddrc_dfitmg0,
	ddrc_dfitmg1,
	ddrc_dfilpcfg0,
	ddrc_dfilpcfg1,
	ddrc_dfiupd0,
	ddrc_dfiupd1,
	ddrc_dfiupd2,
	ddrc_dfimisc = 0x6c,
	ddrc_dfitmg2,
	ddrc_dbictl = 0x70,
	ddrc_addrmap0 = 0x80,
	ddrc_addrmap1,
	ddrc_addrmap2,
	ddrc_addrmap3,
	ddrc_addrmap4,
	ddrc_addrmap5,
	ddrc_addrmap6,
	ddrc_addrmap7,
	ddrc_addrmap8,
	ddrc_addrmap9,
	ddrc_addrmap10,
	ddrc_addrmap11,
	ddrc_odtcfg = 0x90,
	ddrc_odtmap,
	ddrc_sched = 0x94,
	ddrc_sched1,
	ddrc_perfhpr1 = 0x97,
	ddrc_perflpr1 = 0x99,
	ddrc_perfwr1 = 0x9b,
	ddrc_perfvpr1 = 0x9d,
	ddrc_perfvpw1,
	ddrc_dqmap0 = 0xa0,
	ddrc_dqmap1,
	ddrc_dqmap2,
	ddrc_dqmap3,
	ddrc_dqmap4,
	ddrc_dqmap5,
	ddrc_dbg0 = 0xc0,
	ddrc_dbg1,
	ddrc_dbgcam,
	ddrc_dbgcmd,
	ddrc_dbgstat,
	ddrc_swctl = 0xc8,
	ddrc_swstat,
	ddrc_poisoncfg = 0xdb,
	ddrc_poisonstat,
	ddrc_pstat = 0xff,
	ddrc_pccfg,
	ddrc_pcfgr_0,
	ddrc_pcfgw_0,
	ddrc_pctrl_0 = 0x124,
	ddrc_pcfgqos0_0,
	ddrc_pcfgqos1_0,
	ddrc_pcfgwqos0_0,
	ddrc_pcfgwqos1_0,
	ddrc_pcfgr_1 = 0x12d,
	ddrc_pcfgw_1,
	ddrc_pctrl_1 = 0x150,
	ddrc_pcfgqos0_1,
	ddrc_pcfgqos1_1,
	ddrc_pcfgwqos0_1,
	ddrc_pcfgwqos1_1,
	ddrc_pcfgr_2 = 0x159,
	ddrc_pcfgw_2,
	ddrc_pctrl_2 = 0x17c,
	ddrc_pcfgqos0_2,
	ddrc_pcfgqos1_2,
	ddrc_pcfgwqos0_2,
	ddrc_pcfgwqos1_2,
	ddrc_pcfgr_3 = 0x185,
	ddrc_pcfgw_3,
	ddrc_pctrl_3 = 0x1a8,
	ddrc_pcfgqos0_3,
	ddrc_pcfgqos1_3,
	ddrc_pcfgwqos0_3,
	ddrc_pcfgwqos1_3,
	ddrc_pcfgr_4 = 0x1b1,
	ddrc_pcfgw_4,
	ddrc_pctrl_4 = 0x1d4,
	ddrc_pcfgqos0_4,
	ddrc_pcfgqos1_4,
	ddrc_pcfgwqos0_4,
	ddrc_pcfgwqos1_4,
	ddrc_pcfgr_5 = 0x1dd,
	ddrc_pcfgw_5,
	ddrc_pctrl_5 = 0x200,
	ddrc_pcfgqos0_5,
	ddrc_pcfgqos1_5,
	ddrc_pcfgwqos0_5,
	ddrc_pcfgwqos1_5,
	ddrc_sarbase0 = 0x3c1,
	ddrc_sarsize0,
	ddrc_sarbase1,
	ddrc_sarsize1,
	ddrc_derateint_shadow = 0x809,
	ddrc_rfshctl0_shadow = 0x814,
	ddrc_rfshtmg_shadow = 0x819,
	ddrc_init3_shadow = 0x837,
	ddrc_init4_shadow,
	ddrc_init6_shadow = 0x83a,
	ddrc_init7_shadow,
	ddrc_dramtmg0_shadow = 0x840,
	ddrc_dramtmg1_shadow,
	ddrc_dramtmg2_shadow,
	ddrc_dramtmg3_shadow,
	ddrc_dramtmg4_shadow,
	ddrc_dramtmg5_shadow,
	ddrc_dramtmg6_shadow,
	ddrc_dramtmg7_shadow,
	ddrc_dramtmg8_shadow,
	ddrc_dramtmg9_shadow,
	ddrc_dramtmg10_shadow,
	ddrc_dramtmg11_shadow,
	ddrc_dramtmg12_shadow,
	ddrc_dramtmg13_shadow,
	ddrc_dramtmg14_shadow,
	ddrc_zqctl0_shadow = 0x860,
	ddrc_dfitmg0_shadow = 0x864,
	ddrc_dfitmg1_shadow,
	ddrc_dfitmg2_shadow = 0x86d,
	ddrc_odtcfg_shadow = 0x890,
};

enum {
	ddr_phy_pir = 0x1,
	ddr_phy_pgcr0 = 0x4,
	ddr_phy_pgcr2 = 0x6,
	ddr_phy_pgcr3,
	ddr_phy_pgcr4,
	ddr_phy_pgcr5,
	ddr_phy_pgcr6,
	ddr_phy_pgcr7,
	ddr_phy_pgsr0,
	ddr_phy_pgsr1,
	ddr_phy_pgsr2,
	ddr_phy_ptr0 = 0x10,
	ddr_phy_ptr1,
	ddr_phy_ptr2,
	ddr_phy_ptr3,
	ddr_phy_ptr4,
	ddr_phy_ptr5,
	ddr_phy_ptr6,
	ddr_phy_pllcr0 = 0x1a,
	ddr_phy_pllcr1,
	ddr_phy_pllcr2,
	ddr_phy_pllcr3,
	ddr_phy_pllcr4,
	ddr_phy_pllcr5,
	ddr_phy_dxccr = 0x22,
	ddr_phy_dsgcr = 0x24,
	ddr_phy_odtcr = 0x26,
	ddr_phy_aacr = 0x28,
	ddr_phy_gpr0 = 0x30,
	ddr_phy_gpr1,
	ddr_phy_dcr = 0x40,
	ddr_phy_dtpr0 = 0x44,
	ddr_phy_dtpr1,
	ddr_phy_dtpr2,
	ddr_phy_dtpr3,
	ddr_phy_dtpr4,
	ddr_phy_dtpr5,
	ddr_phy_dtpr6,
	ddr_phy_rdimmgcr0 = 0x50,
	ddr_phy_rdimmgcr1,
	ddr_phy_rdimmgcr2,
	ddr_phy_rdimmcr0 = 0x54,
	ddr_phy_rdimmcr1,
	ddr_phy_rdimmcr2,
	ddr_phy_rdimmcr3,
	ddr_phy_rdimmcr4,
	ddr_phy_schcr0 = 0x5a,
	ddr_phy_schcr1,
	ddr_phy_mr0 = 0x60,
	ddr_phy_mr1,
	ddr_phy_mr2,
	ddr_phy_mr3,
	ddr_phy_mr4,
	ddr_phy_mr5,
	ddr_phy_mr6,
	ddr_phy_mr7,
	ddr_phy_mr11 = 0x6b,
	ddr_phy_mr12,
	ddr_phy_mr13,
	ddr_phy_mr14,
	ddr_phy_mr22 = 0x76,
	ddr_phy_dtcr0 = 0x80,
	ddr_phy_dtcr1,
	ddr_phy_dtar0,
	ddr_phy_dtar1,
	ddr_phy_dtar2,
	ddr_phy_dtdr0 = 0x86,
	ddr_phy_dtdr1,
	ddr_phy_dtedr0 = 0x8c,
	ddr_phy_dtedr1,
	ddr_phy_dtedr2,
	ddr_phy_vtdr,
	ddr_phy_catr0,
	ddr_phy_catr1,
	ddr_phy_dqsdr0 = 0x94,
	ddr_phy_dqsdr1,
	ddr_phy_dqsdr2,
	ddr_phy_dtedr3,
	ddr_phy_dcuar = 0xc0,
	ddr_phy_dcudr,
	ddr_phy_dcurr,
	ddr_phy_dculr,
	ddr_phy_dcugcr,
	ddr_phy_dcutpr,
	ddr_phy_dcusr0,
	ddr_phy_dcusr1,
	ddr_phy_bistlsr = 0x105,
	ddr_phy_rankidr = 0x137,
	ddr_phy_riocr0,
	ddr_phy_riocr1,
	ddr_phy_riocr2,
	ddr_phy_riocr3,
	ddr_phy_riocr4,
	ddr_phy_riocr5,
	ddr_phy_aciocr0 = 0x140,
	ddr_phy_aciocr1,
	ddr_phy_aciocr2,
	ddr_phy_aciocr3,
	ddr_phy_aciocr4,
	ddr_phy_aciocr5,
	ddr_phy_iovcr0 = 0x148,
	ddr_phy_iovcr1,
	ddr_phy_vtcr0,
	ddr_phy_vtcr1,
	ddr_phy_acbdlr0 = 0x150,
	ddr_phy_acbdlr1,
	ddr_phy_acbdlr2,
	ddr_phy_acbdlr3,
	ddr_phy_acbdlr4,
	ddr_phy_acbdlr5,
	ddr_phy_acbdlr6,
	ddr_phy_acbdlr7,
	ddr_phy_acbdlr8,
	ddr_phy_acbdlr9,
	ddr_phy_acbdlr15 = 0x15f,
	ddr_phy_acbdlr16,
	ddr_phy_aclcdlr,
	ddr_phy_acmdlr0 = 0x168,
	ddr_phy_acmdlr1,
	ddr_phy_zqcr = 0x1a0,
	ddr_phy_zq0pr0,
	ddr_phy_zq0pr1,
	ddr_phy_zq0dr0,
	ddr_phy_zq0dr1,
	ddr_phy_zq0or0,
	ddr_phy_zq0or1,
	ddr_phy_zq0sr,
	ddr_phy_zq1pr0 = 0x1a9,
	ddr_phy_zq1pr1,
	ddr_phy_zq1dr0,
	ddr_phy_zq1dr1,
	ddr_phy_zq1or0,
	ddr_phy_zq1or1,
	ddr_phy_zq1sr,
	ddr_phy_dx0gcr0 = 0x1c0,
	ddr_phy_dx0gcr1,
	ddr_phy_dx0gcr2,
	ddr_phy_dx0gcr3,
	ddr_phy_dx0gcr4,
	ddr_phy_dx0gcr5,
	ddr_phy_dx0gcr6,
	ddr_phy_dx0bdlr0 = 0x1d0,
	ddr_phy_dx0bdlr1,
	ddr_phy_dx0bdlr2,
	ddr_phy_dx0bdlr3 = 0x1d4,
	ddr_phy_dx0bdlr4,
	ddr_phy_dx0bdlr5,
	ddr_phy_dx0bdlr6 = 0x1d8,
	ddr_phy_dx0lcdlr0 = 0x1e0,
	ddr_phy_dx0lcdlr1,
	ddr_phy_dx0lcdlr2,
	ddr_phy_dx0lcdlr3,
	ddr_phy_dx0lcdlr4,
	ddr_phy_dx0lcdlr5,
	ddr_phy_dx0mdlr0 = 0x1e8,
	ddr_phy_dx0mdlr1,
	ddr_phy_dx0gtr0 = 0x1f0,
	ddr_phy_dx0rsr1 = 0x1f5,
	ddr_phy_dx0rsr2,
	ddr_phy_dx0rsr3,
	ddr_phy_dx0gsr0,
	ddr_phy_dx0gsr1,
	ddr_phy_dx0gsr2,
	ddr_phy_dx0gsr3,
	ddr_phy_dx1gcr0 = 0x200,
	ddr_phy_dx1gcr1,
	ddr_phy_dx1gcr2,
	ddr_phy_dx1gcr3,
	ddr_phy_dx1gcr4,
	ddr_phy_dx1gcr5,
	ddr_phy_dx1gcr6,
	ddr_phy_dx1bdlr0 = 0x210,
	ddr_phy_dx1bdlr1,
	ddr_phy_dx1bdlr2,
	ddr_phy_dx1bdlr3 = 0x214,
	ddr_phy_dx1bdlr4,
	ddr_phy_dx1bdlr5,
	ddr_phy_dx1bdlr6 = 0x218,
	ddr_phy_dx1lcdlr0 = 0x220,
	ddr_phy_dx1lcdlr1,
	ddr_phy_dx1lcdlr2,
	ddr_phy_dx1lcdlr3,
	ddr_phy_dx1lcdlr4,
	ddr_phy_dx1lcdlr5,
	ddr_phy_dx1mdlr0 = 0x228,
	ddr_phy_dx1mdlr1,
	ddr_phy_dx1gtr0 = 0x230,
	ddr_phy_dx1rsr1 = 0x235,
	ddr_phy_dx1rsr2,
	ddr_phy_dx1rsr3,
	ddr_phy_dx1gsr0,
	ddr_phy_dx1gsr1,
	ddr_phy_dx1gsr2,
	ddr_phy_dx1gsr3,
	ddr_phy_dx2gcr0 = 0x240,
	ddr_phy_dx2gcr1,
	ddr_phy_dx2gcr2,
	ddr_phy_dx2gcr3,
	ddr_phy_dx2gcr4,
	ddr_phy_dx2gcr5,
	ddr_phy_dx2gcr6,
	ddr_phy_dx2bdlr0 = 0x250,
	ddr_phy_dx2bdlr1,
	ddr_phy_dx2bdlr2,
	ddr_phy_dx2bdlr3 = 0x254,
	ddr_phy_dx2bdlr4,
	ddr_phy_dx2bdlr5,
	ddr_phy_dx2bdlr6 = 0x258,
	ddr_phy_dx2lcdlr0 = 0x260,
	ddr_phy_dx2lcdlr1,
	ddr_phy_dx2lcdlr2,
	ddr_phy_dx2lcdlr3,
	ddr_phy_dx2lcdlr4,
	ddr_phy_dx2lcdlr5,
	ddr_phy_dx2mdlr0 = 0x268,
	ddr_phy_dx2mdlr1,
	ddr_phy_dx2gtr0 = 0x270,
	ddr_phy_dx2rsr1 = 0x275,
	ddr_phy_dx2rsr2,
	ddr_phy_dx2rsr3,
	ddr_phy_dx2gsr0,
	ddr_phy_dx2gsr1,
	ddr_phy_dx2gsr2,
	ddr_phy_dx2gsr3,
	ddr_phy_dx3gcr0 = 0x280,
	ddr_phy_dx3gcr1,
	ddr_phy_dx3gcr2,
	ddr_phy_dx3gcr3,
	ddr_phy_dx3gcr4,
	ddr_phy_dx3gcr5,
	ddr_phy_dx3gcr6,
	ddr_phy_dx3bdlr0 = 0x290,
	ddr_phy_dx3bdlr1,
	ddr_phy_dx3bdlr2,
	ddr_phy_dx3bdlr3 = 0x294,
	ddr_phy_dx3bdlr4,
	ddr_phy_dx3bdlr5,
	ddr_phy_dx3bdlr6 = 0x298,
	ddr_phy_dx3lcdlr0 = 0x2a0,
	ddr_phy_dx3lcdlr1,
	ddr_phy_dx3lcdlr2,
	ddr_phy_dx3lcdlr3,
	ddr_phy_dx3lcdlr4,
	ddr_phy_dx3lcdlr5,
	ddr_phy_dx3mdlr0 = 0x2a8,
	ddr_phy_dx3mdlr1,
	ddr_phy_dx3gtr0 = 0x2b0,
	ddr_phy_dx3rsr1 = 0x2b5,
	ddr_phy_dx3rsr2,
	ddr_phy_dx3rsr3,
	ddr_phy_dx3gsr0,
	ddr_phy_dx3gsr1,
	ddr_phy_dx3gsr2,
	ddr_phy_dx3gsr3,
	ddr_phy_dx4gcr0 = 0x2c0,
	ddr_phy_dx4gcr1,
	ddr_phy_dx4gcr2,
	ddr_phy_dx4gcr3,
	ddr_phy_dx4gcr4,
	ddr_phy_dx4gcr5,
	ddr_phy_dx4gcr6,
	ddr_phy_dx4bdlr0 = 0x2d0,
	ddr_phy_dx4bdlr1,
	ddr_phy_dx4bdlr2,
	ddr_phy_dx4bdlr3 = 0x2d4,
	ddr_phy_dx4bdlr4,
	ddr_phy_dx4bdlr5,
	ddr_phy_dx4bdlr6 = 0x2d8,
	ddr_phy_dx4lcdlr0 = 0x2e0,
	ddr_phy_dx4lcdlr1,
	ddr_phy_dx4lcdlr2,
	ddr_phy_dx4lcdlr3,
	ddr_phy_dx4lcdlr4,
	ddr_phy_dx4lcdlr5,
	ddr_phy_dx4mdlr0 = 0x2e8,
	ddr_phy_dx4mdlr1,
	ddr_phy_dx4gtr0 = 0x2f0,
	ddr_phy_dx4rsr1 = 0x2f5,
	ddr_phy_dx4rsr2,
	ddr_phy_dx4rsr3,
	ddr_phy_dx4gsr0,
	ddr_phy_dx4gsr1,
	ddr_phy_dx4gsr2,
	ddr_phy_dx4gsr3,
	ddr_phy_dx5gcr0 = 0x300,
	ddr_phy_dx5gcr1,
	ddr_phy_dx5gcr2,
	ddr_phy_dx5gcr3,
	ddr_phy_dx5gcr4,
	ddr_phy_dx5gcr5,
	ddr_phy_dx5gcr6,
	ddr_phy_dx5bdlr0 = 0x310,
	ddr_phy_dx5bdlr1,
	ddr_phy_dx5bdlr2,
	ddr_phy_dx5bdlr3 = 0x314,
	ddr_phy_dx5bdlr4,
	ddr_phy_dx5bdlr5,
	ddr_phy_dx5bdlr6 = 0x318,
	ddr_phy_dx5lcdlr0 = 0x320,
	ddr_phy_dx5lcdlr1,
	ddr_phy_dx5lcdlr2,
	ddr_phy_dx5lcdlr3,
	ddr_phy_dx5lcdlr4,
	ddr_phy_dx5lcdlr5,
	ddr_phy_dx5mdlr0 = 0x328,
	ddr_phy_dx5mdlr1,
	ddr_phy_dx5gtr0 = 0x330,
	ddr_phy_dx5rsr1 = 0x335,
	ddr_phy_dx5rsr2,
	ddr_phy_dx5rsr3,
	ddr_phy_dx5gsr0,
	ddr_phy_dx5gsr1,
	ddr_phy_dx5gsr2,
	ddr_phy_dx5gsr3,
	ddr_phy_dx6gcr0 = 0x340,
	ddr_phy_dx6gcr1,
	ddr_phy_dx6gcr2,
	ddr_phy_dx6gcr3,
	ddr_phy_dx6gcr4,
	ddr_phy_dx6gcr5,
	ddr_phy_dx6gcr6,
	ddr_phy_dx6bdlr0 = 0x350,
	ddr_phy_dx6bdlr1,
	ddr_phy_dx6bdlr2,
	ddr_phy_dx6bdlr3 = 0x354,
	ddr_phy_dx6bdlr4,
	ddr_phy_dx6bdlr5,
	ddr_phy_dx6bdlr6 = 0x358,
	ddr_phy_dx6lcdlr0 = 0x360,
	ddr_phy_dx6lcdlr1,
	ddr_phy_dx6lcdlr2,
	ddr_phy_dx6lcdlr3,
	ddr_phy_dx6lcdlr4,
	ddr_phy_dx6lcdlr5,
	ddr_phy_dx6mdlr0 = 0x368,
	ddr_phy_dx6mdlr1,
	ddr_phy_dx6gtr0 = 0x370,
	ddr_phy_dx6rsr1 = 0x375,
	ddr_phy_dx6rsr2,
	ddr_phy_dx6rsr3,
	ddr_phy_dx6gsr0,
	ddr_phy_dx6gsr1,
	ddr_phy_dx6gsr2,
	ddr_phy_dx6gsr3,
	ddr_phy_dx7gcr0 = 0x380,
	ddr_phy_dx7gcr1,
	ddr_phy_dx7gcr2,
	ddr_phy_dx7gcr3,
	ddr_phy_dx7gcr4,
	ddr_phy_dx7gcr5,
	ddr_phy_dx7gcr6,
	ddr_phy_dx7bdlr0 = 0x390,
	ddr_phy_dx7bdlr1,
	ddr_phy_dx7bdlr2,
	ddr_phy_dx7bdlr3 = 0x394,
	ddr_phy_dx7bdlr4,
	ddr_phy_dx7bdlr5,
	ddr_phy_dx7bdlr6 = 0x398,
	ddr_phy_dx7lcdlr0 = 0x3a0,
	ddr_phy_dx7lcdlr1,
	ddr_phy_dx7lcdlr2,
	ddr_phy_dx7lcdlr3,
	ddr_phy_dx7lcdlr4,
	ddr_phy_dx7lcdlr5,
	ddr_phy_dx7mdlr0 = 0x3a8,
	ddr_phy_dx7mdlr1,
	ddr_phy_dx7gtr0 = 0x3b0,
	ddr_phy_dx7rsr1 = 0x3b5,
	ddr_phy_dx7rsr2,
	ddr_phy_dx7rsr3,
	ddr_phy_dx7gsr0,
	ddr_phy_dx7gsr1,
	ddr_phy_dx7gsr2,
	ddr_phy_dx7gsr3,
	ddr_phy_dx8gcr0 = 0x3c0,
	ddr_phy_dx8gcr1,
	ddr_phy_dx8gcr2,
	ddr_phy_dx8gcr3,
	ddr_phy_dx8gcr4,
	ddr_phy_dx8gcr5,
	ddr_phy_dx8gcr6,
	ddr_phy_dx8bdlr0 = 0x3d0,
	ddr_phy_dx8bdlr1,
	ddr_phy_dx8bdlr2,
	ddr_phy_dx8bdlr3 = 0x3d4,
	ddr_phy_dx8bdlr4,
	ddr_phy_dx8bdlr5,
	ddr_phy_dx8bdlr6 = 0x3d8,
	ddr_phy_dx8lcdlr0 = 0x3e0,
	ddr_phy_dx8lcdlr1,
	ddr_phy_dx8lcdlr2,
	ddr_phy_dx8lcdlr3,
	ddr_phy_dx8lcdlr4,
	ddr_phy_dx8lcdlr5,
	ddr_phy_dx8mdlr0 = 0x3e8,
	ddr_phy_dx8mdlr1,
	ddr_phy_dx8gtr0 = 0x3f0,
	ddr_phy_dx8rsr1 = 0x3f5,
	ddr_phy_dx8rsr2,
	ddr_phy_dx8rsr3,
	ddr_phy_dx8gsr0,
	ddr_phy_dx8gsr1,
	ddr_phy_dx8gsr2,
	ddr_phy_dx8gsr3,
	ddr_phy_dx8sl0osc = 0x500,
	ddr_phy_dx8sl0pllcr0,
	ddr_phy_dx8sl0pllcr1,
	ddr_phy_dx8sl0pllcr2,
	ddr_phy_dx8sl0pllcr3,
	ddr_phy_dx8sl0pllcr4,
	ddr_phy_dx8sl0pllcr5,
	ddr_phy_dx8sl0dqsctl,
	ddr_phy_dx8sl0trnctl,
	ddr_phy_dx8sl0ddlctl,
	ddr_phy_dx8sl0dxctl1,
	ddr_phy_dx8sl0dxctl2,
	ddr_phy_dx8sl0iocr,
	ddr_phy_dx8sl1osc = 0x510,
	ddr_phy_dx8sl1pllcr0,
	ddr_phy_dx8sl1pllcr1,
	ddr_phy_dx8sl1pllcr2,
	ddr_phy_dx8sl1pllcr3,
	ddr_phy_dx8sl1pllcr4,
	ddr_phy_dx8sl1pllcr5,
	ddr_phy_dx8sl1dqsctl,
	ddr_phy_dx8sl1trnctl,
	ddr_phy_dx8sl1ddlctl,
	ddr_phy_dx8sl1dxctl1,
	ddr_phy_dx8sl1dxctl2,
	ddr_phy_dx8sl1iocr,
	ddr_phy_dx8sl2osc = 0x520,
	ddr_phy_dx8sl2pllcr0,
	ddr_phy_dx8sl2pllcr1,
	ddr_phy_dx8sl2pllcr2,
	ddr_phy_dx8sl2pllcr3,
	ddr_phy_dx8sl2pllcr4,
	ddr_phy_dx8sl2pllcr5,
	ddr_phy_dx8sl2dqsctl,
	ddr_phy_dx8sl2trnctl,
	ddr_phy_dx8sl2ddlctl,
	ddr_phy_dx8sl2dxctl1,
	ddr_phy_dx8sl2dxctl2,
	ddr_phy_dx8sl2iocr,
	ddr_phy_dx8sl3osc = 0x530,
	ddr_phy_dx8sl3pllcr0,
	ddr_phy_dx8sl3pllcr1,
	ddr_phy_dx8sl3pllcr2,
	ddr_phy_dx8sl3pllcr3,
	ddr_phy_dx8sl3pllcr4,
	ddr_phy_dx8sl3pllcr5,
	ddr_phy_dx8sl3dqsctl,
	ddr_phy_dx8sl3trnctl,
	ddr_phy_dx8sl3ddlctl,
	ddr_phy_dx8sl3dxctl1,
	ddr_phy_dx8sl3dxctl2,
	ddr_phy_dx8sl3iocr,
	ddr_phy_dx8sl4osc = 0x540,
	ddr_phy_dx8sl4pllcr0,
	ddr_phy_dx8sl4pllcr1,
	ddr_phy_dx8sl4pllcr2,
	ddr_phy_dx8sl4pllcr3,
	ddr_phy_dx8sl4pllcr4,
	ddr_phy_dx8sl4pllcr5,
	ddr_phy_dx8sl4dqsctl,
	ddr_phy_dx8sl4trnctl,
	ddr_phy_dx8sl4ddlctl,
	ddr_phy_dx8sl4dxctl1,
	ddr_phy_dx8sl4dxctl2,
	ddr_phy_dx8sl4iocr,
	ddr_phy_dx8slbosc = 0x5f0,
	ddr_phy_dx8slbpllcr0,
	ddr_phy_dx8slbpllcr1,
	ddr_phy_dx8slbpllcr2,
	ddr_phy_dx8slbpllcr3,
	ddr_phy_dx8slbpllcr4,
	ddr_phy_dx8slbpllcr5,
	ddr_phy_dx8slbdqsctl,
	ddr_phy_dx8slbtrnctl,
	ddr_phy_dx8slbddlctl,
	ddr_phy_dx8slbdxctl1,
	ddr_phy_dx8slbdxctl2,
	ddr_phy_dx8slbiocr,
};

#endif /* _ZYNQMP_REGS_H_ */
