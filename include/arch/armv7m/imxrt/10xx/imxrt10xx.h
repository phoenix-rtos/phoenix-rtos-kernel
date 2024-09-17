/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMX RT basic peripherals control functions
 *
 * Copyright 2019 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_IMXRT_H_
#define _PHOENIX_ARCH_IMXRT_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */

/* Devices clocks */
enum {
	/* CCM_CCGR0 */
	pctl_clk_aips_tz1 = 0, pctl_clk_aips_tz2, pctl_clk_mqs, pctl_clk_sim_m_main = pctl_clk_mqs + 2, pctl_clk_dcp,
	pctl_clk_lpuart3, pctl_clk_can1, pctl_clk_can1_serial, pctl_clk_can2, pctl_clk_can2_serial, pctl_clk_trace,
	pctl_clk_gpt2_bus, pctl_clk_gpt2_serial, pctl_clk_lpuart2, pctl_clk_gpio2,

	/* CCM_CCGR1 */
	pctl_clk_lpspi1, pctl_clk_lpspi2, pctl_clk_lpspi3, pctl_clk_lpspi4, pctl_clk_adc2, pctl_clk_enet,
	pctl_clk_pit, pctl_clk_aoi2, pctl_clk_adc1, pctl_clk_semc_exsc, pctl_clk_gpt1_bus, pctl_clk_gpt1_serial,
	pctl_clk_lpuart4, pctl_clk_gpio1, pctl_clk_csu, pctl_clk_gpio5,

	/* CCM_CCGR2 */
	pctl_clk_ocram_exsc, pctl_clk_csi, pctl_clk_iomuxc_snvs, pctl_clk_lpi2c1, pctl_clk_lpi2c2, pctl_clk_lpi2c3,
	pctl_clk_ocotp_ctrl, pctl_clk_xbar3, pctl_clk_ipmux1, pctl_clk_ipmux2, pctl_clk_ipmux3, pctl_clk_xbar1,
	pctl_clk_xbar2, pctl_clk_gpio3, pctl_clk_lcd, pctl_clk_pxp,

	/* CCM_CCGR3 */
	pctl_clk_flexio2, pctl_clk_lpuart5, pctl_clk_semc, pctl_clk_lpuart6, pctl_clk_aoi1, pctl_clk_lcdif_pix,
	pctl_clk_gpio4, pctl_clk_ewm, pctl_clk_wdog1, pctl_clk_flexram, pctl_clk_acmp1, pctl_clk_acmp2,
	pctl_clk_acmp3, pctl_clk_acmp4, pctl_clk_ocram, pctl_clk_iomux_snvs_gpr,

	/* CCM_CCGR4 */
	pctl_clk_sim_m7_reg, pctl_clk_iomuxc, pctl_clk_iomux_gpr, pctl_clk_bee, pctl_clk_sim_m7, pctl_clk_tsc_dig,
	pctl_clk_sim_m, pctl_clk_sim_ems, pctl_clk_pwm1, pctl_clk_pwm2, pctl_clk_pwm3, pctl_clk_pwm4,
	pctl_clk_enc1, pctl_clk_enc2, pctl_clk_enc3, pctl_clk_enc4,

	/* CCM_CCGR5 */
	pctl_clk_rom, pctl_clk_flexio1, pctl_clk_wdog3, pctl_clk_dma, pctl_clk_kpp, pctl_clk_wdog2,
	pctl_clk_aipstz4, pctl_clk_spdif, pctl_clk_sim_main, pctl_clk_sai1, pctl_clk_sai2, pctl_clk_sai3,
	pctl_clk_lpuart1, pctl_clk_lpuart7, pctl_clk_snvs_hp, pctl_clk_snvs_lp,

	/* CCM_CCGR6 */
	pctl_clk_usboh3, pctl_clk_usdhc1, pctl_clk_usdhc2, pctl_clk_dcdc, pctl_clk_ipmux4,
	pctl_clk_flexspi, pctl_clk_trng, pctl_clk_lpuart8, pctl_clk_timer4, pctl_clk_aips_tz3, pctl_clk_sim_axbs_p,
	pctl_clk_anadig, pctl_clk_lpi2c4, pctl_clk_timer1, pctl_clk_timer2, pctl_clk_timer3,

	/* CCM_CCGR7 */
	pctl_clk_enet2, pctl_clk_flexspi2, pctl_clk_axbs_l, pctl_clk_can3, pctl_clk_can3_serial,
	pctl_clk_aips_lite, pctl_clk_flexio3
};


/* Peripheral clock modes */
enum { clk_state_off = 0, clk_state_run, clk_state_run_wait = 3 };


/* IOMUX - GPR */
enum {
	/* IOMUXC_GPR_GPR1 */
	pctl_gpr_sai1_mclk1_sel = 32, pctl_gpr_sai1_mclk2_sel = 32 + 3, pctl_gpr_sai1_mclk3_sel = 32 + 6,
	pctl_gpr_sai2_mclk3_sel = 32 + 8, pctl_gpr_sai3_mclk3_sel = 32 + 10, pctl_gpr_gint = 32 + 12, pctl_gpr_enet1_clk_sel,
	pctl_gpr_enet2_clk_sel, pctl_gpr_usb_exp_mode, pctl_gpr_enet1_tx_clk_dir = 32 + 17, pctl_gpr_enet2_tx_clk_dir,
	pctl_gpr_sai1_mclk_dir, pctl_gpr_sai2_mclk_dir, pctl_gpr_sai3_mclk_dir, pctl_gpr_exc_mon,
	pctl_gpr_enet_ipg_clk_s_en, pctl_gpr_cm7_force_hclk_en = 32 + 31,

	/* IOMUXC_GPR_GPR2 */
	pctl_gpr_axbs_l_ahbxl_high_priority, pctl_gpr_axbs_l_dma_high_priority, pctl_gpr_axbs_l_force_round_robin,
	pctl_gpr_axbs_p_m0_high_priority, pctl_gpr_axbs_p_m1_high_priority, pctl_gpr_axbs_p_force_round_robin,
	pctl_gpr_canfd_filter_bypass, pctl_gpr_l2_mem_en_powersaving = 64 + 12, pctl_gpr_ram_auto_clk_gating_en,
	pctl_gpr_l2_mem_deepsleep, pctl_gpr_mqs_clk_div = 64 + 16, pctl_gpr_mqs_sw_rst = 64 + 24, pctl_gpr_mqs_en,
	pctl_gpr_mqs_oversample, pctl_gpr_qtimer1_tmr_cnts_freeze = 64 + 28, pctl_gpr_qtimer2_tmr_cnts_freeze,
	pctl_gpr_qtimer3_tmr_cnts_freeze, pctl_gpr_qtimer4_tmr_cnts_freeze,

	/* IOMUXC_GPR_GPR3 */
	pctl_gpr_ocram_ctl, pctl_gpr_dcp_key_sel = 96 + 4, pctl_gpr_ocram2_ctl = 96 + 8, pctl_gpr_axbs_l_halt_req = 96 + 15,
	pctl_gpr_ocram_status, pctl_gpr_ocram2_status = 96 + 24, pctl_gpr_axbs_l_halted = 96 + 31,

	/* IOMUXC_GPR_GPR4 */
	pctl_gpr_edma_stop_req, pctl_gpr_can1_stop_req, pctl_gpr_can2_stop_req, pctl_gpr_trng_stop_req,
	pctl_gpr_enet_stop_req, pctl_gpr_sai1_stop_req, pctl_gpr_sai2_stop_req, pctl_gpr_sai3_stop_req,
	pctl_gpr_enet2_stop_req, pctl_gpr_semc_stop_req, pctl_gpr_pit_stop_req, pctl_gpr_flexspi_stop_req,
	pctl_gpr_flexio1_stop_req, pctl_gpr_flexio2_stop_req, pctl_gpr_flexio3_stop_req, pctl_gpr_flexspi2_stop_req,
	pctl_gpr_edma_stop_ack, pctl_gpr_can1_stop_ack, pctl_gpr_can2_stop_ack, pctl_gpr_trng_stop_ack,
	pctl_gpr_enet_stop_ack, pctl_gpr_sai1_stop_ack, pctl_gpr_sai2_stop_ack, pctl_gpr_sai3_stop_ack,
	pctl_gpr_enet2_stop_ack, pctl_gpr_semc_stop_ack, pctl_gpr_pit_stop_ack, pctl_gpr_flexspi_stop_ack,
	pctl_gpr_flexio1_stop_ack, pctl_gpr_flexio2_stop_ack, pctl_gpr_flexio3_stop_ack, pctl_gpr_flexspi2_stop_ack,

	/* IOMUXC_GPR_GPR5 */
	pctl_gpr_wdog1_mask = 160 + 6, pctl_gpr_wdog2_mask, pctl_gpr_gpt2_capin1_sel = 160 + 23,
	pctl_gpr_gpt2_capin2_sel, pctl_gpr_enet_event3in_sel, pctl_gpr_enet2_event3in_sel,
	pctl_gpr_vref_1m_clk_gpt1 = 160 + 28, pctl_gpr_vref_1m_clk_gpt2,

	/* IOMUXC_GPR_GPR6 */
	pctl_gpr_qtimer1_trm0_input_sel = 192, pctl_gpr_qtimer1_trm1_input_sel, pctl_gpr_qtimer1_trm2_input_sel,
	pctl_gpr_qtimer1_trm3_input_sel, pctl_gpr_qtimer2_trm0_input_sel, pctl_gpr_qtimer2_trm1_input_sel,
	pctl_gpr_qtimer2_trm2_input_sel, pctl_gpr_qtimer2_trm3_input_sel, pctl_gpr_qtimer3_trm0_input_sel,
	pctl_gpr_qtimer3_trm1_input_sel, pctl_gpr_qtimer3_trm2_input_sel, pctl_gpr_qtimer3_trm3_input_sel,
	pctl_gpr_qtimer4_trm0_input_sel, pctl_gpr_qtimer4_trm1_input_sel, pctl_gpr_qtimer4_trm2_input_sel,
	pctl_gpr_qtimer4_trm3_input_sel, pctl_gpr_iomuxc_xbar_dir_sel_4, pctl_gpr_iomuxc_xbar_dir_sel_5,
	pctl_gpr_iomuxc_xbar_dir_sel_6, pctl_gpr_iomuxc_xbar_dir_sel_7, pctl_gpr_iomuxc_xbar_dir_sel_8,
	pctl_gpr_iomuxc_xbar_dir_sel_9, pctl_gpr_iomuxc_xbar_dir_sel_10, pctl_gpr_iomuxc_xbar_dir_sel_11,
	pctl_gpr_iomuxc_xbar_dir_sel_12, pctl_gpr_iomuxc_xbar_dir_sel_13, pctl_gpr_iomuxc_xbar_dir_sel_14,
	pctl_gpr_iomuxc_xbar_dir_sel_15, pctl_gpr_iomuxc_xbar_dir_sel_16, pctl_gpr_iomuxc_xbar_dir_sel_17,
	pctl_gpr_iomuxc_xbar_dir_sel_18, pctl_gpr_iomuxc_xbar_dir_sel_19,

	/* IOMUXC_GPR_GPR7 */
	pctl_gpr_lpi2c1_stop_req, pctl_gpr_lpi2c2_stop_req, pctl_gpr_lpi2c3_stop_req, pctl_gpr_lpi2c4_stop_req,
	pctl_gpr_lpspi1_stop_req, pctl_gpr_lpspi2_stop_req, pctl_gpr_lpspi3_stop_req, pctl_gpr_lpspi4_stop_req,
	pctl_gpr_lpuart1_stop_req, pctl_gpr_lpuart2_stop_req, pctl_gpr_lpuart3_stop_req, pctl_gpr_lpuart4_stop_req,
	pctl_gpr_lpuart5_stop_req, pctl_gpr_lpuart6_stop_req, pctl_gpr_lpuart7_stop_req, pctl_gpr_lpuart8_stop_req,
	pctl_gpr_lpi2c1_stop_ack, pctl_gpr_lpi2c2_stop_ack, pctl_gpr_lpi2c3_stop_ack, pctl_gpr_lpi2c4_stop_ack,
	pctl_gpr_lpspi1_stop_ack, pctl_gpr_lpspi2_stop_ack, pctl_gpr_lpspi3_stop_ack, pctl_gpr_lpspi4_stop_ack,
	pctl_gpr_lpuart1_stop_ack, pctl_gpr_lpuart2_stop_ack, pctl_gpr_lpuart3_stop_ack, pctl_gpr_lpuart4_stop_ack,
	pctl_gpr_lpuart5_stop_ack, pctl_gpr_lpuart6_stop_ack, pctl_gpr_lpuart7_stop_ack, pctl_gpr_lpuart8_stop_ack,

	/* IOMUXC_GPR_GPR8 */
	pctl_gpr_lpi2c1_ipg_stop_mode, pctl_gpr_lpi2c1_ipg_doze, pctl_gpr_lpi2c2_ipg_stop_mode, pctl_gpr_lpi2c2_ipg_doze,
	pctl_gpr_lpi2c3_ipg_stop_mode, pctl_gpr_lpi2c3_ipg_doze, pctl_gpr_lpi2c4_ipg_stop_mode, pctl_gpr_lpi2c4_ipg_doze,
	pctl_gpr_lpspi1_ipg_stop_mode, pctl_gpr_lpspi1_ipg_doze, pctl_gpr_lpspi2_ipg_stop_mode, pctl_gpr_lpspi2_ipg_doze,
	pctl_gpr_lpspi3_ipg_stop_mode, pctl_gpr_lpspi3_ipg_doze, pctl_gpr_lpspi4_ipg_stop_mode, pctl_gpr_lpspi4_ipg_doze,
	pctl_gpr_lpuart1_ipg_stop_mode, pctl_gpr_lpuart1_ipg_doze, pctl_gpr_lpuart2_ipg_stop_mode, pctl_gpr_lpuart2_ipg_doze,
	pctl_gpr_lpuart3_ipg_stop_mode, pctl_gpr_lpuart3_ipg_doze, pctl_gpr_lpuart4_ipg_stop_mode, pctl_gpr_lpuart4_ipg_doze,
	pctl_gpr_lpuart5_ipg_stop_mode, pctl_gpr_lpuart5_ipg_doze, pctl_gpr_lpuart6_ipg_stop_mode, pctl_gpr_lpuart6_ipg_doze,
	pctl_gpr_lpuart7_ipg_stop_mode, pctl_gpr_lpuart7_ipg_doze, pctl_gpr_lpuart8_ipg_stop_mode, pctl_gpr_lpuart8_ipg_doze,

	/* IOMUXC_GPR_GPR10 */
	pctl_gpr_niden = 320, pctl_gpr_dbg_en, pctl_gpr_sec_err_resp, pctl_gpr_dcpkey_ocotp_or_keymux = 320 + 4,
	pctl_gpr_ocram_tz_en = 320 + 8, pctl_gpr_ocram_tz_addr, pctl_gpr_lock_niden = 320 + 16, pctl_gpr_lock_dbg_en,
	pctl_gpr_lock_sec_err_resp, pctl_gpr_lock_dcpkey_ocotp_or_keymux = 320 + 20, pctl_gpr_lock_ocram_tz_en = 320 + 24,
	pctl_gpr_lock_ocram_tz_addr,

	/* IOMUXC_GPR_GPR11 */
	pctl_gpr_m7_apc_ac_r0_ctrl = 352, pctl_gpr_m7_apc_ac_r1_ctrl = 352 + 2, pctl_gpr_m7_apc_ac_r2_ctrl = 352 + 4,
	pctl_gpr_m7_apc_ac_r3_ctrl = 352 + 6, pctl_gpr_bee_de_rx_en = 352 + 8,

	/* IOMUXC_GPR_GPR12 */
	pctl_gpr_flexio1_ipg_stop_mode = 384, pctl_gpr_flexio1_ipg_doze, pctl_gpr_flexio2_ipg_stop_mode,
	pctl_gpr_flexio2_ipg_doze, pctl_gpr_acmp_ipg_stop_mode, pctl_gpr_flexio3_ipg_stop_mode,
	pctl_gpr_flexio3_ipg_doze,

	/* IOMUXC_GPR_GPR13 */
	pctl_gpr_arcache_usdhc = 416, pctl_gpr_awcache_usdhc, pctl_gpr_canfd_stop_req = 416 + 4,
	pctl_gpr_cache_enet = 416 + 7, pctl_gpr_cache_usb = 416 + 13, pctl_gpr_canfd_stop_ack = 416 + 20,

	/* IOMUXC_GPR_GPR14 */
	pctl_gpr_acmp1_cmp_igen_trim_dn = 448, pctl_gpr_acmp2_cmp_igen_trim_dn, pctl_gpr_acmp3_cmp_igen_trim_dn,
	pctl_gpr_acmp4_cmp_igen_trim_dn, pctl_gpr_acmp1_cmp_igen_trim_up, pctl_gpr_acmp2_cmp_igen_trim_up,
	pctl_gpr_acmp3_cmp_igen_trim_up, pctl_gpr_acmp4_cmp_igen_trim_up, pctl_gpr_acmp1_sample_sync_en,
	pctl_gpr_acmp2_sample_sync_en, pctl_gpr_acmp3_sample_sync_en, pctl_gpr_acmp4_sample_sync_en,
	pctl_gpr_cm7_cfgitcmsz = 448 + 16, pctl_gpr_cm7_cfgdtcmsz = 448 + 20,

	/* IOMUXC_GPR_GPR16 */
	pctl_gpr_init_itcm_en = 512, pctl_gpr_init_dtcm_en, pctl_gpr_flexram_bank_cfg_sel,

	/* IOMUXC_GPR_GPR17 */
	pctl_gpr_flexram_bank_cfg = 544,

	/* IOMUXC_GPR_GPR18 */
	pctl_gpr_lock_m7_apc_ac_r0_bot = 576, pctl_gpr_m7_apc_ac_r0_bot = 576 + 3,

	/* IOMUXC_GPR_GPR19 */
	pctl_gpr_lock_m7_apc_ac_r0_top = 608, pctl_gpr_m7_apc_ac_r0_top = 608 + 3,

	/* IOMUXC_GPR_GPR20 */
	pctl_gpr_lock_m7_apc_ac_r1_bot = 640, pctl_gpr_m7_apc_ac_r1_bot = 640 + 3,

	/* IOMUXC_GPR_GPR21 */
	pctl_gpr_lock_m7_apc_ac_r1_top = 672, pctl_gpr_m7_apc_ac_r1_top = 672 + 3,

	/* IOMUXC_GPR_GPR22 */
	pctl_gpr_lock_m7_apc_ac_r2_bot = 704, pctl_gpr_m7_apc_ac_r2_bot = 704 + 3,

	/* IOMUXC_GPR_GPR23 */
	pctl_gpr_lock_m7_apc_ac_r2_top = 736, pctl_gpr_m7_apc_ac_r2_top = 736 + 3,

	/* IOMUXC_GPR_GPR24 */
	pctl_gpr_lock_m7_apc_ac_r3_bot = 768, pctl_gpr_m7_apc_ac_r3_bot = 768 + 3,

	/* IOMUXC_GPR_GPR25 */
	pctl_gpr_lock_m7_apc_ac_r3_top = 800, pctl_gpr_m7_apc_ac_r3_top = 800 + 3,

	/* IOMUXC_GPR_GPR26 */
	pctl_gpr_gpio_mux1_gpio_sel = 832,

	/* IOMUXC_GPR_GPR27 */
	pctl_gpr_gpio_mux2_gpio_sel = 864,

	/* IOMUXC_GPR_GPR28 */
	pctl_gpr_gpio_mux3_gpio_sel = 896,

	/* IOMUXC_GPR_GPR29 */
	pctl_gpr_gpio_mux4_gpio_sel = 928,

	/* IOMUXC_GPR_GPR30 */
	pctl_gpr_flexspi_remap_addr_start = 960 + 12,

	/* IOMUXC_GPR_GPR31 */
	pctl_gpr_flexspi_remap_addr_end = 992 + 12,

	/* IOMUXC_GPR_GPR32 */
	pctl_gpr_flexspi_remap_addr_offset = 1024 + 12,

	/* IOMUXC_GPR_GPR33 */
	pctl_gpr_ocram2_tz_en = 1056, pctl_gpr_ocram2_tz_addr, pctl_gpr_lock_ocram2_tz_en = 1056 + 16,
	pctl_gpr_lock_ocram2_tz_addr = 1056 + 17,

	/* IOMUXC_GPR_GPR34 */
	pctl_gpr_sip_test_mux_qspi_sip_sel = 1088, pctl_gpr_sip_test_mux_qspi_sip_en = 1088 + 8
};


/* IOMUX - MUX */
enum {
	pctl_mux_gpio_emc_00 = 0, pctl_mux_gpio_emc_01, pctl_mux_gpio_emc_02, pctl_mux_gpio_emc_03,
	pctl_mux_gpio_emc_04, pctl_mux_gpio_emc_05, pctl_mux_gpio_emc_06, pctl_mux_gpio_emc_07,
	pctl_mux_gpio_emc_08, pctl_mux_gpio_emc_09, pctl_mux_gpio_emc_10, pctl_mux_gpio_emc_11,
	pctl_mux_gpio_emc_12, pctl_mux_gpio_emc_13, pctl_mux_gpio_emc_14, pctl_mux_gpio_emc_15,
	pctl_mux_gpio_emc_16, pctl_mux_gpio_emc_17, pctl_mux_gpio_emc_18, pctl_mux_gpio_emc_19,
	pctl_mux_gpio_emc_20, pctl_mux_gpio_emc_21, pctl_mux_gpio_emc_22, pctl_mux_gpio_emc_23,
	pctl_mux_gpio_emc_24, pctl_mux_gpio_emc_25, pctl_mux_gpio_emc_26, pctl_mux_gpio_emc_27,
	pctl_mux_gpio_emc_28, pctl_mux_gpio_emc_29, pctl_mux_gpio_emc_30, pctl_mux_gpio_emc_31,
	pctl_mux_gpio_emc_32, pctl_mux_gpio_emc_33, pctl_mux_gpio_emc_34, pctl_mux_gpio_emc_35,
	pctl_mux_gpio_emc_36, pctl_mux_gpio_emc_37, pctl_mux_gpio_emc_38, pctl_mux_gpio_emc_39,
	pctl_mux_gpio_emc_40, pctl_mux_gpio_emc_41, pctl_mux_gpio_ad_b0_00, pctl_mux_gpio_ad_b0_01,
	pctl_mux_gpio_ad_b0_02, pctl_mux_gpio_ad_b0_03, pctl_mux_gpio_ad_b0_04, pctl_mux_gpio_ad_b0_05,
	pctl_mux_gpio_ad_b0_06, pctl_mux_gpio_ad_b0_07, pctl_mux_gpio_ad_b0_08, pctl_mux_gpio_ad_b0_09,
	pctl_mux_gpio_ad_b0_10, pctl_mux_gpio_ad_b0_11, pctl_mux_gpio_ad_b0_12, pctl_mux_gpio_ad_b0_13,
	pctl_mux_gpio_ad_b0_14, pctl_mux_gpio_ad_b0_15, pctl_mux_gpio_ad_b1_00, pctl_mux_gpio_ad_b1_01,
	pctl_mux_gpio_ad_b1_02, pctl_mux_gpio_ad_b1_03, pctl_mux_gpio_ad_b1_04, pctl_mux_gpio_ad_b1_05,
	pctl_mux_gpio_ad_b1_06, pctl_mux_gpio_ad_b1_07, pctl_mux_gpio_ad_b1_08, pctl_mux_gpio_ad_b1_09,
	pctl_mux_gpio_ad_b1_10, pctl_mux_gpio_ad_b1_11, pctl_mux_gpio_ad_b1_12, pctl_mux_gpio_ad_b1_13,
	pctl_mux_gpio_ad_b1_14, pctl_mux_gpio_ad_b1_15, pctl_mux_gpio_b0_00, pctl_mux_gpio_b0_01,
	pctl_mux_gpio_b0_02, pctl_mux_gpio_b0_03, pctl_mux_gpio_b0_04, pctl_mux_gpio_b0_05,
	pctl_mux_gpio_b0_06, pctl_mux_gpio_b0_07, pctl_mux_gpio_b0_08, pctl_mux_gpio_b0_09,
	pctl_mux_gpio_b0_10, pctl_mux_gpio_b0_11, pctl_mux_gpio_b0_12, pctl_mux_gpio_b0_13,
	pctl_mux_gpio_b0_14, pctl_mux_gpio_b0_15, pctl_mux_gpio_b1_00, pctl_mux_gpio_b1_01,
	pctl_mux_gpio_b1_02, pctl_mux_gpio_b1_03, pctl_mux_gpio_b1_04, pctl_mux_gpio_b1_05,
	pctl_mux_gpio_b1_06, pctl_mux_gpio_b1_07, pctl_mux_gpio_b1_08, pctl_mux_gpio_b1_09,
	pctl_mux_gpio_b1_10, pctl_mux_gpio_b1_11, pctl_mux_gpio_b1_12, pctl_mux_gpio_b1_13,
	pctl_mux_gpio_b1_14, pctl_mux_gpio_b1_15, pctl_mux_gpio_sd_b0_00, pctl_mux_gpio_sd_b0_01,
	pctl_mux_gpio_sd_b0_02, pctl_mux_gpio_sd_b0_03, pctl_mux_gpio_sd_b0_04, pctl_mux_gpio_sd_b0_05,
	pctl_mux_gpio_sd_b1_00, pctl_mux_gpio_sd_b1_01, pctl_mux_gpio_sd_b1_02, pctl_mux_gpio_sd_b1_03,
	pctl_mux_gpio_sd_b1_04, pctl_mux_gpio_sd_b1_05, pctl_mux_gpio_sd_b1_06, pctl_mux_gpio_sd_b1_07,
	pctl_mux_gpio_sd_b1_08, pctl_mux_gpio_sd_b1_09, pctl_mux_gpio_sd_b1_10, pctl_mux_gpio_sd_b1_11,

	pctl_mux_snvs_wakeup, pctl_mux_snvs_pmic_on_req, pctl_mux_snvs_pmic_stby_req
};


/* IOMUX - PAD */
enum {
	pctl_pad_gpio_emc_00 = 0, pctl_pad_gpio_emc_01, pctl_pad_gpio_emc_02, pctl_pad_gpio_emc_03,
	pctl_pad_gpio_emc_04, pctl_pad_gpio_emc_05, pctl_pad_gpio_emc_06, pctl_pad_gpio_emc_07,
	pctl_pad_gpio_emc_08, pctl_pad_gpio_emc_09, pctl_pad_gpio_emc_10, pctl_pad_gpio_emc_11,
	pctl_pad_gpio_emc_12, pctl_pad_gpio_emc_13, pctl_pad_gpio_emc_14, pctl_pad_gpio_emc_15,
	pctl_pad_gpio_emc_16, pctl_pad_gpio_emc_17, pctl_pad_gpio_emc_18, pctl_pad_gpio_emc_19,
	pctl_pad_gpio_emc_20, pctl_pad_gpio_emc_21, pctl_pad_gpio_emc_22, pctl_pad_gpio_emc_23,
	pctl_pad_gpio_emc_24, pctl_pad_gpio_emc_25, pctl_pad_gpio_emc_26, pctl_pad_gpio_emc_27,
	pctl_pad_gpio_emc_28, pctl_pad_gpio_emc_29, pctl_pad_gpio_emc_30, pctl_pad_gpio_emc_31,
	pctl_pad_gpio_emc_32, pctl_pad_gpio_emc_33, pctl_pad_gpio_emc_34, pctl_pad_gpio_emc_35,
	pctl_pad_gpio_emc_36, pctl_pad_gpio_emc_37, pctl_pad_gpio_emc_38, pctl_pad_gpio_emc_39,
	pctl_pad_gpio_emc_40, pctl_pad_gpio_emc_41, pctl_pad_gpio_ad_b0_00, pctl_pad_gpio_ad_b0_01,
	pctl_pad_gpio_ad_b0_02, pctl_pad_gpio_ad_b0_03, pctl_pad_gpio_ad_b0_04, pctl_pad_gpio_ad_b0_05,
	pctl_pad_gpio_ad_b0_06, pctl_pad_gpio_ad_b0_07, pctl_pad_gpio_ad_b0_08, pctl_pad_gpio_ad_b0_09,
	pctl_pad_gpio_ad_b0_10, pctl_pad_gpio_ad_b0_11, pctl_pad_gpio_ad_b0_12, pctl_pad_gpio_ad_b0_13,
	pctl_pad_gpio_ad_b0_14, pctl_pad_gpio_ad_b0_15, pctl_pad_gpio_ad_b1_00, pctl_pad_gpio_ad_b1_01,
	pctl_pad_gpio_ad_b1_02, pctl_pad_gpio_ad_b1_03, pctl_pad_gpio_ad_b1_04, pctl_pad_gpio_ad_b1_05,
	pctl_pad_gpio_ad_b1_06, pctl_pad_gpio_ad_b1_07, pctl_pad_gpio_ad_b1_08, pctl_pad_gpio_ad_b1_09,
	pctl_pad_gpio_ad_b1_10, pctl_pad_gpio_ad_b1_11, pctl_pad_gpio_ad_b1_12, pctl_pad_gpio_ad_b1_13,
	pctl_pad_gpio_ad_b1_14, pctl_pad_gpio_ad_b1_15, pctl_pad_gpio_b0_00, pctl_pad_gpio_b0_01,
	pctl_pad_gpio_b0_02, pctl_pad_gpio_b0_03, pctl_pad_gpio_b0_04, pctl_pad_gpio_b0_05,
	pctl_pad_gpio_b0_06, pctl_pad_gpio_b0_07, pctl_pad_gpio_b0_08, pctl_pad_gpio_b0_09,
	pctl_pad_gpio_b0_10, pctl_pad_gpio_b0_11, pctl_pad_gpio_b0_12, pctl_pad_gpio_b0_13,
	pctl_pad_gpio_b0_14, pctl_pad_gpio_b0_15, pctl_pad_gpio_b1_00, pctl_pad_gpio_b1_01,
	pctl_pad_gpio_b1_02, pctl_pad_gpio_b1_03, pctl_pad_gpio_b1_04, pctl_pad_gpio_b1_05,
	pctl_pad_gpio_b1_06, pctl_pad_gpio_b1_07, pctl_pad_gpio_b1_08, pctl_pad_gpio_b1_09,
	pctl_pad_gpio_b1_10, pctl_pad_gpio_b1_11, pctl_pad_gpio_b1_12, pctl_pad_gpio_b1_13,
	pctl_pad_gpio_b1_14, pctl_pad_gpio_b1_15, pctl_pad_gpio_sd_b0_00, pctl_pad_gpio_sd_b0_01,
	pctl_pad_gpio_sd_b0_02, pctl_pad_gpio_sd_b0_03, pctl_pad_gpio_sd_b0_04, pctl_pad_gpio_sd_b0_05,
	pctl_pad_gpio_sd_b1_00, pctl_pad_gpio_sd_b1_01, pctl_pad_gpio_sd_b1_02, pctl_pad_gpio_sd_b1_03,
	pctl_pad_gpio_sd_b1_04, pctl_pad_gpio_sd_b1_05, pctl_pad_gpio_sd_b1_06, pctl_pad_gpio_sd_b1_07,
	pctl_pad_gpio_sd_b1_08, pctl_pad_gpio_sd_b1_09, pctl_pad_gpio_sd_b1_10, pctl_pad_gpio_sd_b1_11,

	pctl_pad_gpio_spi_b0_00, pctl_pad_gpio_spi_b0_01, pctl_pad_gpio_spi_b0_02, pctl_pad_gpio_spi_b0_03,
	pctl_pad_gpio_spi_b0_04, pctl_pad_gpio_spi_b0_05, pctl_pad_gpio_spi_b0_06, pctl_pad_gpio_spi_b0_07,
	pctl_pad_gpio_spi_b0_08, pctl_pad_gpio_spi_b0_09, pctl_pad_gpio_spi_b0_10, pctl_pad_gpio_spi_b0_11,
	pctl_pad_gpio_spi_b0_12, pctl_pad_gpio_spi_b0_13, pctl_pad_gpio_spi_b1_00, pctl_pad_gpio_spi_b1_01,
	pctl_pad_gpio_spi_b1_02, pctl_pad_gpio_spi_b1_03, pctl_pad_gpio_spi_b1_04, pctl_pad_gpio_spi_b1_05,
	pctl_pad_gpio_spi_b1_06, pctl_pad_gpio_spi_b1_07,

	pctl_pad_snvs_test_mode, pctl_pad_snvs_por_b, pctl_pad_snvs_onoff, pctl_pad_snvs_wakeup,
	pctl_pad_snvs_pmic_on_req, pctl_pad_snvs_pmic_stby_req
};


/* IOMUX - DAISY */
enum {
	pctl_isel_anatop_usb_otg1_id = 0, pctl_isel_anatop_usb_otg2_id, pctl_isel_ccm_pmic_ready,
	pctl_isel_csi_data02, pctl_isel_csi_data03, pctl_isel_csi_data04, pctl_isel_csi_data05,
	pctl_isel_csi_data06, pctl_isel_csi_data07, pctl_isel_csi_data08, pctl_isel_csi_data09,
	pctl_isel_csi_hsync, pctl_isel_csi_pixclk, pctl_isel_csi_vsync, pctl_isel_enet_ipg_clk_rmi,
	pctl_isel_enet_mdio, pctl_isel_enet0_rxdata, pctl_isel_enet1_rxdata, pctl_isel_enet_rxen,
	pctl_isel_enet_rxerr, pctl_isel_enet0_timer, pctl_isel_enet_txclk, pctl_isel_flexcan1_rx,
	pctl_isel_flexcan2_rx, pctl_isel_flexpwm1_pwma3, pctl_isel_flexpwm1_pwma0, pctl_isel_flexpwm1_pwma1,
	pctl_isel_flexpwm1_pwma2, pctl_isel_flexpwm1_pwmb3, pctl_isel_flexpwm1_pwmb0, pctl_isel_flexpwm1_pwmb1,
	pctl_isel_flexpwm1_pwmb2, pctl_isel_flexpwm2_pwma3, pctl_isel_flexpwm2_pwma0, pctl_isel_flexpwm2_pwma1,
	pctl_isel_flexpwm2_pwma2, pctl_isel_flexpwm2_pwmb3, pctl_isel_flexpwm2_pwmb0, pctl_isel_flexpwm2_pwmb1,
	pctl_isel_flexpwm2_pwmb2, pctl_isel_flexpwm4_pwma0, pctl_isel_flexpwm4_pwma1, pctl_isel_flexpwm4_pwma2,
	pctl_isel_flexpwm4_pwma3, pctl_isel_flexspia_dqs, pctl_isel_flexspia_data0, pctl_isel_flexspia_data1,
	pctl_isel_flexspia_data2, pctl_isel_flexspia_data3, pctl_isel_flexspib_data0, pctl_isel_flexspib_data1,
	pctl_isel_flexspib_data2, pctl_isel_flexspib_data3, pctl_isel_flexspia_sck, pctl_isel_lpi2c1_scl,
	pctl_isel_lpi2c1_sda, pctl_isel_lpi2c2_scl, pctl_isel_lpi2c2_sda, pctl_isel_lpi2c3_scl,
	pctl_isel_lpi2c3_sda, pctl_isel_lpi2c4_scl, pctl_isel_lpi2c4_sda, pctl_isel_lpspi1_pcs0,
	pctl_isel_lpspi1_sck, pctl_isel_lpspi1_sdi, pctl_isel_lpspi1_sdo, pctl_isel_lpspi2_pcs0,
	pctl_isel_lpspi2_sck, pctl_isel_lpspi2_sdi, pctl_isel_lpspi2_sdo, pctl_isel_lpspi3_pcs0,
	pctl_isel_lpspi3_sck, pctl_isel_lpspi3_sdi, pctl_isel_lpspi3_sdo, pctl_isel_lpspi4_pcs0,
	pctl_isel_lpspi4_sck, pctl_isel_lpspi4_sdi, pctl_isel_lpspi4_sdo, pctl_isel_lpuart2_rx,
	pctl_isel_lpuart2_tx, pctl_isel_lpuart3_cts_b, pctl_isel_lpuart3_rx, pctl_isel_lpuart3_tx,
	pctl_isel_lpuart4_rx, pctl_isel_lpuart4_tx, pctl_isel_lpuart5_rx, pctl_isel_lpuart5_tx,
	pctl_isel_lpuart6_rx, pctl_isel_lpuart6_tx, pctl_isel_lpuart7_rx, pctl_isel_lpuart7_tx,
	pctl_isel_lpuart8_rx, pctl_isel_lpuart8_tx, pctl_isel_nmi, pctl_isel_qtimer2_timer0,
	pctl_isel_qtimer2_timer1, pctl_isel_qtimer2_timer2, pctl_isel_qtimer2_timer3,
	pctl_isel_qtimer3_timer0, pctl_isel_qtimer3_timer1, pctl_isel_qtimer3_timer2,
	pctl_isel_qtimer3_timer3, pctl_isel_sai1_mclk2, pctl_isel_sai1_rx_bclk, pctl_isel_sai1_rx_data0,
	pctl_isel_sai1_rx_data1, pctl_isel_sai1_rx_data2, pctl_isel_sai1_rx_data3, pctl_isel_sai1_rx_sync,
	pctl_isel_sai1_tx_bclk, pctl_isel_sai1_tx_sync, pctl_isel_sai2_mclk2, pctl_isel_sai2_rx_bclk,
	pctl_isel_sai2_rx_data0, pctl_isel_sai2_rx_sync, pctl_isel_sai2_tx_bclk, pctl_isel_sai2_tx_sync,
	pctl_isel_spdif_in, pctl_isel_usb_otg2_oc, pctl_isel_usb_otg1_oc, pctl_isel_usdhc1_cd_b,
	pctl_isel_usdhc1_wp, pctl_isel_usdhc2_clk, pctl_isel_usdhc2_cd_b, pctl_isel_usdhc2_cmd,
	pctl_isel_usdhc2_data0, pctl_isel_usdhc2_data1, pctl_isel_usdhc2_data2, pctl_isel_usdhc2_data3,
	pctl_isel_usdhc2_data4, pctl_isel_usdhc2_data5, pctl_isel_usdhc2_data6, pctl_isel_usdhc2_data7,
	pctl_isel_usdhc2_wp, pctl_isel_xbar1_in02, pctl_isel_xbar1_in03, pctl_isel_xbar1_in04,
	pctl_isel_xbar1_in05, pctl_isel_xbar1_in06, pctl_isel_xbar1_in07, pctl_isel_xbar1_in08,
	pctl_isel_xbar1_in09, pctl_isel_xbar1_in17, pctl_isel_xbar1_in18, pctl_isel_xbar1_in20,
	pctl_isel_xbar1_in22, pctl_isel_xbar1_in23, pctl_isel_xbar1_in24, pctl_isel_xbar1_in14,
	pctl_isel_xbar1_in15, pctl_isel_xbar1_in16, pctl_isel_xbar1_in25, pctl_isel_xbar1_in19,
	pctl_isel_xbar1_in21,

	pctl_isel_enet2_ipg_clk_rmii, pctl_isel_enet2_ipp_ind_mac0_mdio, pctl_isel_enet2_ipp_ind_mac0_rxdata,
	pctl_isel_enet2_ipp_ind_mac0_rxen, pctl_isel_enet2_ipp_ind_mac0_rxerr, pctl_isel_enet2_ipp_ind_mac0_timer,
	pctl_isel_enet2_ipp_ind_mac0_txclk, pctl_isel_gpt1_ipp_ind_capin1, pctl_isel_gpt1_ipp_ind_capin2,
	pctl_isel_gpt1_ipp_ind_clkin, pctl_isel_gpt2_ipp_ind_capin1, pctl_isel_gpt2_ipp_ind_capin2,
	pctl_isel_gpt2_ipp_ind_clkin, pctl_isel_sai3_ipg_clk_sai_mclk, pctl_isel_sai3_ipp_ind_sai_rxbclk,
	pctl_isel_sai3_ipp_ind_sai_rxdata, pctl_isel_sai3_ipp_ind_sai_rxsync,
	pctl_isel_sai3_ipp_ind_sai_txbclk, pctl_isel_sai3_ipp_ind_sai_txsync, pctl_isel_semc_i_ipp_ind_dqs4,
	pctl_isel_canfd_ipp_ind_canrx
};


/* Interrupts numbers */
enum { cti0_err_irq = 17 + 16, cti1_err_irq, core_irq, lpuart1_irq, lpuart2_irq, lpuart3_irq, lpuart4_irq, lpuart5_irq,
	lpuart6_irq, lpuart7_irq, lpuart8_irq, lpi2c1_irq, lpi2c2_irq, lpi2c3_irq, lpi2c4_irq, lpspi1_irq, lpspi2_irq,
	lpspi3_irq, lpspi4_irq, can1_irq, can2_irq, flexram_irq, kpp_irq, tsc_dig_irq, gpr_irq, lcdif_irq, csi_irq, pxp_irq,
	wdog2_irq, snvs_hp_wrapper_irq, snvs_hp_wrapper_tz_irq, snvs_lp_wrapper_irq, /* Reserved #49 */ dcp_irq = 50 + 16,
	dcp_vmi_irq, dcp_secure_irq, trng_irq, /* Reserved #54 */ bee_irq = 55 + 16, sai1_irq, sai2_irq, sai3_irq,
	/* Reserved #59 */ spdif_irq = 60 + 16, /* Reserved #61..64 */ usb_phy1_irq = 65 + 16, usb_phy2_irq, adc1_irq,
	adc2_irq, dcdc_irq, /* Reserved #70..71 */ gpio1_int0_irq = 72 + 16, gpio1_int1_irq, gpio1_int2_irq, gpio1_int3_irq,
	gpio1_int4_irq, gpio1_int5_irq, gpio1_int6_irq, gpio1_int7_irq , gpio1_0_15_irq, gpio1_16_31_irq, gpio2_0_15_irq,
	gpio2_16_31_irq, gpio3_0_15_irq, gpio3_16_31_irq, gpio4_0_15_irq, gpio4_16_31_irq, gpio5_0_15_irq, gpio5_16_31_irq,
	flexio1_irq, flexio2_irq, wdog1_irq, rtwdog_irq, ewm_irq, ccm_1_irq, ccm2_irq, gpc_irq, src_irq, /* Reserved #99 */
	gpt1_irq = 100 + 16, gpt2_irq, pwm1_0_irq, pwm1_1_irq, pwm1_2_irq, pwm1_3_irq, pwm1_fault_irq, /* Reserved #107 */
	flexspi_irq = 108 + 16, semc_irq, usdhc1_irq, usdhc2_irq, usb_otg2_irq, usb_otg1_irq, enet_irq, enet_1588_timer_irq,
	/* Reserved #116..117 */ adc_etc0_irq = 118 + 16, adc_etc1_irq, adc_etc2_irq, adc_etc_error_irq, pit_irq,
	acmp0_irq, acmp1_irq, acmp2_irq, acmp3_irq, acmp4_irq, /* Reserved #127..128 */ enc1_irq = 129 + 16, enc2_irq,
	enc3_irq, enc4_irq, tmr1_irq, tmr2_irq, tmr3_irq, tmr4_irq, pwm2_0_irq, pwm2_1_irq, pwm2_2_irq, pwm2_3_irq,
	pwm2_fault_irq, pwm3_0_irq, pwm3_1_irq, pwm3_2_irq, pwm3_3_irq, pwm3_fault_irq, pwm4_0_irq, pwm4_1_irq, pwm4_2_irq,
	pwm4_3_irq, pwm4_fault_irq };


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclock = 0, pctl_iogpr, pctl_iomux, pctl_iopad, pctl_ioisel, pctl_reboot, pctl_devcache,
		pctl_cleanInvalDCache, pctl_invalDCache, pctl_rttDetails } type;

	union {
		struct {
			int dev;
			unsigned int state;
		} devclock;

		struct {
			int field;
			unsigned int val;
		} iogpr;

		struct {
			int mux;
			char sion;
			char mode;
		} iomux;

		struct {
			int pad;
			char hys;
			char pus;
			char pue;
			char pke;
			char ode;
			char speed;
			char dse;
			char sre;
		} iopad;

		struct {
			int isel;
			char daisy;
		} ioisel;

		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;

		struct {
			unsigned char state;
		} devcache;

		struct {
			void *addr;
			unsigned int sz;
		} cleanInvalDCache;
		struct {
			void *cbAddr;
			unsigned int cbSize;
			void *bufAddr;
			unsigned int bufSize;
		} rttDetails;
	};
} __attribute__((packed)) platformctl_t;


/* clang-format on */

#endif
