/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * i.MX RT1170 basic peripherals control functions
 *
 * Copyright 2019-2022 Phoenix Systems
 * Author: Aleksander Kaminski, Gerard Świderski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_ARCH_IMXRT_H_
#define _PHOENIX_ARCH_IMXRT_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

/* clang-format off */
/* CCM - Clock gating */

enum { pctl_clk_cm7 = 0, pctl_clk_cm4, pctl_clk_bus, pctl_clk_bus_lpsr, pctl_clk_semc, pctl_clk_cssys,
	pctl_clk_cstrace, pctl_clk_m4_systick, pctl_clk_m7_systick, pctl_clk_adc1, pctl_clk_adc2, pctl_clk_acmp,
	pctl_clk_flexio1, pctl_clk_flexio2, pctl_clk_gpt1, pctl_clk_gpt2, pctl_clk_gpt3, pctl_clk_gpt4, pctl_clk_gpt5,
	pctl_clk_gpt6, pctl_clk_flexspi1, pctl_clk_flexspi2, pctl_clk_can1, pctl_clk_can2, pctl_clk_can3, pctl_clk_lpuart1,
	pctl_clk_lpuart2, pctl_clk_lpuart3, pctl_clk_lpuart4, pctl_clk_lpuart5, pctl_clk_lpuart6, pctl_clk_lpuart7,
	pctl_clk_lpuart8, pctl_clk_lpuart9, pctl_clk_lpuart10, pctl_clk_lpuart11, pctl_clk_lpuart12, pctl_clk_lpi2c1,
	pctl_clk_lpi2c2, pctl_clk_lpi2c3, pctl_clk_lpi2c4, pctl_clk_lpi2c5, pctl_clk_lpi2c6, pctl_clk_lpspi1, pctl_clk_lpspi2,
	pctl_clk_lpspi3, pctl_clk_lpspi4, pctl_clk_lpspi5, pctl_clk_lpspi6, pctl_clk_emv1, pctl_clk_emv2, pctl_clk_enet1,
	pctl_clk_enet2, pctl_clk_enet_qos, pctl_clk_enet_25m, pctl_clk_enet_time1, pctl_clk_enet_time2, pctl_clk_enet_time3,
	pctl_clk_usdhc1, pctl_clk_usdhc2, pctl_clk_asrc, pctl_clk_mqs, pctl_clk_pdm, pctl_clk_spdif, pctl_clk_sai1,
	pctl_clk_sai2, pctl_clk_sai3, pctl_clk_sai4, pctl_clk_gpu2d, pctl_clk_elcdif, pctl_clk_lcdifv2, pctl_clk_mipi_ref,
	pctl_clk_mipi_esc, pctl_clk_csi2, pctl_clk_csi2_esc, pctl_clk_csi2_ui, pctl_clk_csi, pctl_clk_ccm_clko1,
	pctl_clk_ccm_clko2 };


/* CCM - Low Power Clock Gates */
enum { pctl_lpcg_m7 = 0, pctl_lpcg_m4, pctl_lpcg_sim_m7, pctl_lpcg_sim_m, pctl_lpcg_sim_disp, pctl_lpcg_sim_per,
	pctl_lpcg_sim_lpsr, pctl_lpcg_anadig, pctl_lpcg_dcdc, pctl_lpcg_src, pctl_lpcg_ccm, pctl_lpcg_gpc, pctl_lpcg_ssarc,
	pctl_lpcg_sim_r, pctl_lpcg_wdog1, pctl_lpcg_wdog2, pctl_lpcg_wdog3, pctl_lpcg_wdog4, pctl_lpcg_ewm0, pctl_lpcg_sema,
	pctl_lpcg_mu_a, pctl_lpcg_mu_b, pctl_lpcg_edma, pctl_lpcg_edma_lpsr, pctl_lpcg_romcp, pctl_lpcg_ocram,
	pctl_lpcg_flexram, pctl_lpcg_lmem, pctl_lpcg_flexspi1, pctl_lpcg_flexspi2, pctl_lpcg_rdc, pctl_lpcg_m7_xrdc,
	pctl_lpcg_m4_xrdc, pctl_lpcg_semc, pctl_lpcg_xecc, pctl_lpcg_iee, pctl_lpcg_puf, pctl_lpcg_ocotp, pctl_lpcg_snvs_hp,
	pctl_lpcg_snvs, pctl_lpcg_caam, pctl_lpcg_jtag_mux, pctl_lpcg_cstrace, pctl_lpcg_xbar1, pctl_lpcg_xbar2,
	pctl_lpcg_xbar3, pctl_lpcg_aoi1, pctl_lpcg_aoi2, pctl_lpcg_adc_etc, pctl_lpcg_iomuxc, pctl_lpcg_iomuxc_lpsr,
	pctl_lpcg_gpio, pctl_lpcg_kpp, pctl_lpcg_flexio1, pctl_lpcg_flexio2, pctl_lpcg_lpadc1, pctl_lpcg_lpadc2,
	pctl_lpcg_dac, pctl_lpcg_acmp1, pctl_lpcg_acmp2, pctl_lpcg_acmp3, pctl_lpcg_acmp4, pctl_lpcg_pit1, pctl_lpcg_pit2,
	pctl_lpcg_gpt1, pctl_lpcg_gpt2, pctl_lpcg_gpt3, pctl_lpcg_gpt4, pctl_lpcg_gpt5, pctl_lpcg_gpt6, pctl_lpcg_qtimer1,
	pctl_lpcg_qtimer2, pctl_lpcg_qtimer3, pctl_lpcg_qtimer4, pctl_lpcg_enc1, pctl_lpcg_enc2, pctl_lpcg_enc3,
	pctl_lpcg_enc4, pctl_lpcg_hrtimer, pctl_lpcg_pwm1, pctl_lpcg_pwm2, pctl_lpcg_pwm3, pctl_lpcg_pwm4, pctl_lpcg_can1,
	pctl_lpcg_can2, pctl_lpcg_can3, pctl_lpcg_lpuart1, pctl_lpcg_lpuart2, pctl_lpcg_lpuart3, pctl_lpcg_lpuart4,
	pctl_lpcg_lpuart5, pctl_lpcg_lpuart6, pctl_lpcg_lpuart7, pctl_lpcg_lpuart8, pctl_lpcg_lpuart9, pctl_lpcg_lpuart10,
	pctl_lpcg_lpuart11, pctl_lpcg_lpuart12, pctl_lpcg_lpi2c1, pctl_lpcg_lpi2c2, pctl_lpcg_lpi2c3, pctl_lpcg_lpi2c4,
	pctl_lpcg_lpi2c5, pctl_lpcg_lpi2c6, pctl_lpcg_lpspi1, pctl_lpcg_lpspi2, pctl_lpcg_lpspi3, pctl_lpcg_lpspi4,
	pctl_lpcg_lpspi5, pctl_lpcg_lpspi6, pctl_lpcg_sim1, pctl_lpcg_sim2, pctl_lpcg_enet, pctl_lpcg_enet_1g,
	pctl_lpcg_enet_qos, pctl_lpcg_usb, pctl_lpcg_cdog, pctl_lpcg_usdhc1, pctl_lpcg_usdhc2, pctl_lpcg_asrc,
	pctl_lpcg_mqs, pctl_lpcg_pdm, pctl_lpcg_spdif, pctl_lpcg_sai1, pctl_lpcg_sai2, pctl_lpcg_sai3, pctl_lpcg_sai4,
	pctl_lpcg_pxp, pctl_lpcg_gpu2d, pctl_lpcg_lcdif, pctl_lpcg_lcdifv2, pctl_lpcg_mipi_dsi, pctl_lpcg_mipi_csi,
	pctl_lpcg_csi, pctl_lpcg_dcic_mipi, pctl_lpcg_dcic_lcd, pctl_lpcg_video_mux, pctl_lpcg_uniq_edt_i };


/* IOMUX - MUX */
enum {
	pctl_mux_gpio_emc_b1_00 = 0, pctl_mux_gpio_emc_b1_01, pctl_mux_gpio_emc_b1_02, pctl_mux_gpio_emc_b1_03,
	pctl_mux_gpio_emc_b1_04, pctl_mux_gpio_emc_b1_05, pctl_mux_gpio_emc_b1_06, pctl_mux_gpio_emc_b1_07,
	pctl_mux_gpio_emc_b1_08, pctl_mux_gpio_emc_b1_09, pctl_mux_gpio_emc_b1_10, pctl_mux_gpio_emc_b1_11,
	pctl_mux_gpio_emc_b1_12, pctl_mux_gpio_emc_b1_13, pctl_mux_gpio_emc_b1_14, pctl_mux_gpio_emc_b1_15,
	pctl_mux_gpio_emc_b1_16, pctl_mux_gpio_emc_b1_17, pctl_mux_gpio_emc_b1_18, pctl_mux_gpio_emc_b1_19,
	pctl_mux_gpio_emc_b1_20, pctl_mux_gpio_emc_b1_21, pctl_mux_gpio_emc_b1_22, pctl_mux_gpio_emc_b1_23,
	pctl_mux_gpio_emc_b1_24, pctl_mux_gpio_emc_b1_25, pctl_mux_gpio_emc_b1_26, pctl_mux_gpio_emc_b1_27,
	pctl_mux_gpio_emc_b1_28, pctl_mux_gpio_emc_b1_29, pctl_mux_gpio_emc_b1_30, pctl_mux_gpio_emc_b1_31,
	pctl_mux_gpio_emc_b1_32, pctl_mux_gpio_emc_b1_33, pctl_mux_gpio_emc_b1_34, pctl_mux_gpio_emc_b1_35,
	pctl_mux_gpio_emc_b1_36, pctl_mux_gpio_emc_b1_37, pctl_mux_gpio_emc_b1_38, pctl_mux_gpio_emc_b1_39,
	pctl_mux_gpio_emc_b1_40, pctl_mux_gpio_emc_b1_41, pctl_mux_gpio_emc_b2_00, pctl_mux_gpio_emc_b2_01,
	pctl_mux_gpio_emc_b2_02, pctl_mux_gpio_emc_b2_03, pctl_mux_gpio_emc_b2_04, pctl_mux_gpio_emc_b2_05,
	pctl_mux_gpio_emc_b2_06, pctl_mux_gpio_emc_b2_07, pctl_mux_gpio_emc_b2_08, pctl_mux_gpio_emc_b2_09,
	pctl_mux_gpio_emc_b2_10, pctl_mux_gpio_emc_b2_11, pctl_mux_gpio_emc_b2_12, pctl_mux_gpio_emc_b2_13,
	pctl_mux_gpio_emc_b2_14, pctl_mux_gpio_emc_b2_15, pctl_mux_gpio_emc_b2_16, pctl_mux_gpio_emc_b2_17,
	pctl_mux_gpio_emc_b2_18, pctl_mux_gpio_emc_b2_19, pctl_mux_gpio_emc_b2_20, pctl_mux_gpio_ad_00,
	pctl_mux_gpio_ad_01, pctl_mux_gpio_ad_02, pctl_mux_gpio_ad_03, pctl_mux_gpio_ad_04, pctl_mux_gpio_ad_05,
	pctl_mux_gpio_ad_06, pctl_mux_gpio_ad_07, pctl_mux_gpio_ad_08, pctl_mux_gpio_ad_09, pctl_mux_gpio_ad_10,
	pctl_mux_gpio_ad_11, pctl_mux_gpio_ad_12, pctl_mux_gpio_ad_13, pctl_mux_gpio_ad_14, pctl_mux_gpio_ad_15,
	pctl_mux_gpio_ad_16, pctl_mux_gpio_ad_17, pctl_mux_gpio_ad_18, pctl_mux_gpio_ad_19, pctl_mux_gpio_ad_20,
	pctl_mux_gpio_ad_21, pctl_mux_gpio_ad_22, pctl_mux_gpio_ad_23, pctl_mux_gpio_ad_24, pctl_mux_gpio_ad_25,
	pctl_mux_gpio_ad_26, pctl_mux_gpio_ad_27, pctl_mux_gpio_ad_28, pctl_mux_gpio_ad_29, pctl_mux_gpio_ad_30,
	pctl_mux_gpio_ad_31, pctl_mux_gpio_ad_32, pctl_mux_gpio_ad_33, pctl_mux_gpio_ad_34, pctl_mux_gpio_ad_35,
	pctl_mux_gpio_sd_b1_00, pctl_mux_gpio_sd_b1_01, pctl_mux_gpio_sd_b1_02, pctl_mux_gpio_sd_b1_03,
	pctl_mux_gpio_sd_b1_04, pctl_mux_gpio_sd_b1_05, pctl_mux_gpio_sd_b2_00, pctl_mux_gpio_sd_b2_01,
	pctl_mux_gpio_sd_b2_02, pctl_mux_gpio_sd_b2_03, pctl_mux_gpio_sd_b2_04, pctl_mux_gpio_sd_b2_05,
	pctl_mux_gpio_sd_b2_06, pctl_mux_gpio_sd_b2_07, pctl_mux_gpio_sd_b2_08, pctl_mux_gpio_sd_b2_09,
	pctl_mux_gpio_sd_b2_10, pctl_mux_gpio_sd_b2_11, pctl_mux_gpio_disp_b1_00, pctl_mux_gpio_disp_b1_01,
	pctl_mux_gpio_disp_b1_02, pctl_mux_gpio_disp_b1_03, pctl_mux_gpio_disp_b1_04, pctl_mux_gpio_disp_b1_05,
	pctl_mux_gpio_disp_b1_06, pctl_mux_gpio_disp_b1_07, pctl_mux_gpio_disp_b1_08, pctl_mux_gpio_disp_b1_09,
	pctl_mux_gpio_disp_b1_10, pctl_mux_gpio_disp_b1_11, pctl_mux_gpio_disp_b2_00, pctl_mux_gpio_disp_b2_01,
	pctl_mux_gpio_disp_b2_02, pctl_mux_gpio_disp_b2_03, pctl_mux_gpio_disp_b2_04, pctl_mux_gpio_disp_b2_05,
	pctl_mux_gpio_disp_b2_06, pctl_mux_gpio_disp_b2_07, pctl_mux_gpio_disp_b2_08, pctl_mux_gpio_disp_b2_09,
	pctl_mux_gpio_disp_b2_10, pctl_mux_gpio_disp_b2_11, pctl_mux_gpio_disp_b2_12, pctl_mux_gpio_disp_b2_13,
	pctl_mux_gpio_disp_b2_14, pctl_mux_gpio_disp_b2_15,

	/* SNVS */
	pctl_mux_wakeup, pctl_mux_pmic_on_req, pctl_mux_pmic_stby_req, pctl_mux_gpio_snvs_00, pctl_mux_gpio_snvs_01,
	pctl_mux_gpio_snvs_02, pctl_mux_gpio_snvs_03, pctl_mux_gpio_snvs_04, pctl_mux_gpio_snvs_05, pctl_mux_gpio_snvs_06,
	pctl_mux_gpio_snvs_07, pctl_mux_gpio_snvs_08, pctl_mux_gpio_snvs_09,

	/* LPSR */
	pctl_mux_gpio_lpsr_00, pctl_mux_gpio_lpsr_01, pctl_mux_gpio_lpsr_02, pctl_mux_gpio_lpsr_03, pctl_mux_gpio_lpsr_04,
	pctl_mux_gpio_lpsr_05, pctl_mux_gpio_lpsr_06, pctl_mux_gpio_lpsr_07, pctl_mux_gpio_lpsr_08, pctl_mux_gpio_lpsr_09,
	pctl_mux_gpio_lpsr_10, pctl_mux_gpio_lpsr_11, pctl_mux_gpio_lpsr_12, pctl_mux_gpio_lpsr_13, pctl_mux_gpio_lpsr_14,
	pctl_mux_gpio_lpsr_15
};


/* IOMUX - PAD */
enum {
	pctl_pad_gpio_emc_b1_00 = 0, pctl_pad_gpio_emc_b1_01, pctl_pad_gpio_emc_b1_02, pctl_pad_gpio_emc_b1_03,
	pctl_pad_gpio_emc_b1_04, pctl_pad_gpio_emc_b1_05, pctl_pad_gpio_emc_b1_06, pctl_pad_gpio_emc_b1_07,
	pctl_pad_gpio_emc_b1_08, pctl_pad_gpio_emc_b1_09, pctl_pad_gpio_emc_b1_10, pctl_pad_gpio_emc_b1_11,
	pctl_pad_gpio_emc_b1_12, pctl_pad_gpio_emc_b1_13, pctl_pad_gpio_emc_b1_14, pctl_pad_gpio_emc_b1_15,
	pctl_pad_gpio_emc_b1_16, pctl_pad_gpio_emc_b1_17, pctl_pad_gpio_emc_b1_18, pctl_pad_gpio_emc_b1_19,
	pctl_pad_gpio_emc_b1_20, pctl_pad_gpio_emc_b1_21, pctl_pad_gpio_emc_b1_22, pctl_pad_gpio_emc_b1_23,
	pctl_pad_gpio_emc_b1_24, pctl_pad_gpio_emc_b1_25, pctl_pad_gpio_emc_b1_26, pctl_pad_gpio_emc_b1_27,
	pctl_pad_gpio_emc_b1_28, pctl_pad_gpio_emc_b1_29, pctl_pad_gpio_emc_b1_30, pctl_pad_gpio_emc_b1_31,
	pctl_pad_gpio_emc_b1_32, pctl_pad_gpio_emc_b1_33, pctl_pad_gpio_emc_b1_34, pctl_pad_gpio_emc_b1_35,
	pctl_pad_gpio_emc_b1_36, pctl_pad_gpio_emc_b1_37, pctl_pad_gpio_emc_b1_38, pctl_pad_gpio_emc_b1_39,
	pctl_pad_gpio_emc_b1_40, pctl_pad_gpio_emc_b1_41, pctl_pad_gpio_emc_b2_00, pctl_pad_gpio_emc_b2_01,
	pctl_pad_gpio_emc_b2_02, pctl_pad_gpio_emc_b2_03, pctl_pad_gpio_emc_b2_04, pctl_pad_gpio_emc_b2_05,
	pctl_pad_gpio_emc_b2_06, pctl_pad_gpio_emc_b2_07, pctl_pad_gpio_emc_b2_08, pctl_pad_gpio_emc_b2_09,
	pctl_pad_gpio_emc_b2_10, pctl_pad_gpio_emc_b2_11, pctl_pad_gpio_emc_b2_12, pctl_pad_gpio_emc_b2_13,
	pctl_pad_gpio_emc_b2_14, pctl_pad_gpio_emc_b2_15, pctl_pad_gpio_emc_b2_16, pctl_pad_gpio_emc_b2_17,
	pctl_pad_gpio_emc_b2_18, pctl_pad_gpio_emc_b2_19, pctl_pad_gpio_emc_b2_20, pctl_pad_gpio_ad_00,
	pctl_pad_gpio_ad_01, pctl_pad_gpio_ad_02, pctl_pad_gpio_ad_03, pctl_pad_gpio_ad_04, pctl_pad_gpio_ad_05,
	pctl_pad_gpio_ad_06, pctl_pad_gpio_ad_07, pctl_pad_gpio_ad_08, pctl_pad_gpio_ad_09, pctl_pad_gpio_ad_10,
	pctl_pad_gpio_ad_11, pctl_pad_gpio_ad_12, pctl_pad_gpio_ad_13, pctl_pad_gpio_ad_14, pctl_pad_gpio_ad_15,
	pctl_pad_gpio_ad_16, pctl_pad_gpio_ad_17, pctl_pad_gpio_ad_18, pctl_pad_gpio_ad_19, pctl_pad_gpio_ad_20,
	pctl_pad_gpio_ad_21, pctl_pad_gpio_ad_22, pctl_pad_gpio_ad_23, pctl_pad_gpio_ad_24, pctl_pad_gpio_ad_25,
	pctl_pad_gpio_ad_26, pctl_pad_gpio_ad_27, pctl_pad_gpio_ad_28, pctl_pad_gpio_ad_29, pctl_pad_gpio_ad_30,
	pctl_pad_gpio_ad_31, pctl_pad_gpio_ad_32, pctl_pad_gpio_ad_33, pctl_pad_gpio_ad_34, pctl_pad_gpio_ad_35,
	pctl_pad_gpio_sd_b1_00, pctl_pad_gpio_sd_b1_01, pctl_pad_gpio_sd_b1_02, pctl_pad_gpio_sd_b1_03,
	pctl_pad_gpio_sd_b1_04, pctl_pad_gpio_sd_b1_05, pctl_pad_gpio_sd_b2_00, pctl_pad_gpio_sd_b2_01,
	pctl_pad_gpio_sd_b2_02, pctl_pad_gpio_sd_b2_03, pctl_pad_gpio_sd_b2_04, pctl_pad_gpio_sd_b2_05,
	pctl_pad_gpio_sd_b2_06, pctl_pad_gpio_sd_b2_07, pctl_pad_gpio_sd_b2_08, pctl_pad_gpio_sd_b2_09,
	pctl_pad_gpio_sd_b2_10, pctl_pad_gpio_sd_b2_11, pctl_pad_gpio_disp_b1_00, pctl_pad_gpio_disp_b1_01,
	pctl_pad_gpio_disp_b1_02, pctl_pad_gpio_disp_b1_03, pctl_pad_gpio_disp_b1_04, pctl_pad_gpio_disp_b1_05,
	pctl_pad_gpio_disp_b1_06, pctl_pad_gpio_disp_b1_07, pctl_pad_gpio_disp_b1_08, pctl_pad_gpio_disp_b1_09,
	pctl_pad_gpio_disp_b1_10, pctl_pad_gpio_disp_b1_11, pctl_pad_gpio_disp_b2_00, pctl_pad_gpio_disp_b2_01,
	pctl_pad_gpio_disp_b2_02, pctl_pad_gpio_disp_b2_03, pctl_pad_gpio_disp_b2_04, pctl_pad_gpio_disp_b2_05,
	pctl_pad_gpio_disp_b2_06, pctl_pad_gpio_disp_b2_07, pctl_pad_gpio_disp_b2_08, pctl_pad_gpio_disp_b2_09,
	pctl_pad_gpio_disp_b2_10, pctl_pad_gpio_disp_b2_11, pctl_pad_gpio_disp_b2_12, pctl_pad_gpio_disp_b2_13,
	pctl_pad_gpio_disp_b2_14, pctl_pad_gpio_disp_b2_15,

	/* SNVS */
	pctl_pad_test_mode, pctl_pad_por_b, pctl_pad_onoff, pctl_pad_wakeup, pctl_pad_pmic_on_req, pctl_pad_pmic_stby_req,
	pctl_pad_gpio_snvs_00, pctl_pad_gpio_snvs_01, pctl_pad_gpio_snvs_02, pctl_pad_gpio_snvs_03, pctl_pad_gpio_snvs_04,
	pctl_pad_gpio_snvs_05, pctl_pad_gpio_snvs_06, pctl_pad_gpio_snvs_07, pctl_pad_gpio_snvs_08, pctl_pad_gpio_snvs_09,

	/* LPSR */
	pctl_pad_gpio_lpsr_00, pctl_pad_gpio_lpsr_01, pctl_pad_gpio_lpsr_02, pctl_pad_gpio_lpsr_03, pctl_pad_gpio_lpsr_04,
	pctl_pad_gpio_lpsr_05, pctl_pad_gpio_lpsr_06, pctl_pad_gpio_lpsr_07, pctl_pad_gpio_lpsr_08, pctl_pad_gpio_lpsr_09,
	pctl_pad_gpio_lpsr_10, pctl_pad_gpio_lpsr_11, pctl_pad_gpio_lpsr_12, pctl_pad_gpio_lpsr_13, pctl_pad_gpio_lpsr_14,
	pctl_pad_gpio_lpsr_15
};


/* IOMUX - DAISY */
enum {
	pctl_isel_flexcan1_rx = 0, pctl_isel_flexcan2_rx,

	pctl_isel_ccm_enet_qos_ref_clk, pctl_isel_ccm_enet_qos_tx_clk, pctl_isel_enet_ipg_clk_rmii,
	pctl_isel_enet_mac0_mdio, pctl_isel_enet_mac0_rxdata_0, pctl_isel_enet_mac0_rxdata_1, pctl_isel_enet_mac0_rxen,
	pctl_isel_enet_mac0_rxerr, pctl_isel_enet_mac0_txclk,

	pctl_isel_enet_1g_ipg_clk_rmii, pctl_isel_enet_1g_mac0_mdio, pctl_isel_enet_1g_mac0_rxclk,
	pctl_isel_enet_1g_mac0_rxdata_0, pctl_isel_enet_1g_mac0_rxdata_1, pctl_isel_enet_1g_mac0_rxdata_2,
	pctl_isel_enet_1g_mac0_rxdata_3, pctl_isel_enet_1g_mac0_rxen, pctl_isel_enet_1g_mac0_rxerr,
	pctl_isel_enet_1g_mac0_txclk,

	pctl_isel_enet_qos_gmii_mdi, pctl_isel_enet_qos_phy_rxd_0, pctl_isel_enet_qos_phy_rxd_1,
	pctl_isel_enet_qos_phy_rxdv, enet_qos_phy_rxer,

	pctl_isel_flexpwm1_pwma_0, pctl_isel_flexpwm1_pwma_1, pctl_isel_flexpwm1_pwma_2, pctl_isel_flexpwm1_pwmb_0,
	pctl_isel_flexpwm1_pwmb_1, pctl_isel_flexpwm1_pwmb_2,

	pctl_isel_flexpwm2_pwma_0, pctl_isel_flexpwm2_pwma_1, pctl_isel_flexpwm2_pwma_2, pctl_isel_flexpwm2_pwmb_0,
	pctl_isel_flexpwm2_pwmb_1, pctl_isel_flexpwm2_pwmb_2,

	pctl_isel_flexpwm3_pwma_0,pctl_isel_flexpwm3_pwma_1, pctl_isel_flexpwm3_pwma_2, pctl_isel_flexpwm3_pwma_3,
	pctl_isel_flexpwm3_pwmb_0, pctl_isel_flexpwm3_pwmb_1, pctl_isel_flexpwm3_pwmb_2, pctl_isel_flexpwm3_pwmb_3,

	pctl_isel_flexspi1_dqs_fa, pctl_isel_flexspi1_fa_0, pctl_isel_flexspi1_fa_1, pctl_isel_flexspi1_fa_2,
	pctl_isel_flexspi1_fa_3, pctl_isel_flexspi1_fb_0, pctl_isel_flexspi1_fb_1, pctl_isel_flexspi1_fb_2,
	pctl_isel_flexspi1_fb_3, pctl_isel_flexspi1_sck_fa, pctl_isel_flexspi1_sck_fb,

	pctl_isel_flexspi2_fa_0, pctl_isel_flexspi2_fa_1, pctl_isel_flexspi2_fa_2, pctl_isel_flexspi2_fa_3,
	pctl_isel_flexspi2_sck_fa,

	pctl_isel_gpt3_capin1, pctl_isel_gpt3_capin2, pctl_isel_gpt3_clkin,

	pctl_isel_kpp_col_6, pctl_isel_kpp_col_7, pctl_isel_kpp_row_6, pctl_isel_kpp_row_7,

	pctl_isel_lpi2c1_scl, pctl_isel_lpi2c1_sda,

	pctl_isel_lpi2c2_scl, pctl_isel_lpi2c2_sda,

	pctl_isel_lpi2c3_scl, pctl_isel_lpi2c3_sda,

	pctl_isel_lpi2c4_scl, pctl_isel_lpi2c4_sda,

	pctl_isel_lpspi1_pcs_0, pctl_isel_lpspi1_sck, pctl_isel_lpspi1_sdi, pctl_isel_lpspi1_sdo,

	pctl_isel_lpspi2_pcs_0, pctl_isel_lpspi2_pcs_1, pctl_isel_lpspi2_sck, pctl_isel_lpspi2_sdi, pctl_isel_lpspi2_sdo,

	pctl_isel_lpspi3_pcs_0, pctl_isel_lpspi3_pcs_1, pctl_isel_lpspi3_pcs_2, pctl_isel_lpspi3_pcs_3,
	pctl_isel_lpspi3_sck, pctl_isel_lpspi3_sdi, pctl_isel_lpspi3_sdo,

	pctl_isel_lpspi4_pcs_0, pctl_isel_lpspi4_sck, pctl_isel_lpspi4_sdi, pctl_isel_lpspi4_sdo,

	pctl_isel_lpuart1_rxd, pctl_isel_lpuart1_txd,

	pctl_isel_lpuart10_rxd, pctl_isel_lpuart10_txd,

	pctl_isel_lpuart7_rxd, pctl_isel_lpuart7_txd,

	pctl_isel_lpuart8_rxd, pctl_isel_lpuart8_txd,

	pctl_isel_qtimer1_tmr0, pctl_isel_qtimer1_tmr1, pctl_isel_qtimer1_tmr2,

	pctl_isel_qtimer2_tmr0, pctl_isel_qtimer2_tmr1, pctl_isel_qtimer2_tmr2,

	pctl_isel_qtimer3_tmr0, pctl_isel_qtimer3_tmr1, pctl_isel_qtimer3_tmr2,

	pctl_isel_qtimer4_tmr0, pctl_isel_qtimer4_tmr1, pctl_isel_qtimer4_tmr2,

	pctl_isel_sai1_mclk, pctl_isel_sai1_rxbclk, pctl_isel_sai1_rxdata_0, pctl_isel_sai1_rxsync,
	pctl_isel_sai1_txbclk, pctl_isel_sai1_txsync,

	pctl_isel_sdio_slv_clk_sd, pctl_isel_sdio_slv_cmd_di, pctl_isel_sdio_slv_dat0_do, pctl_isel_slv_dat1_irq,
	pctl_isel_sdio_slv_dat2_rw, pctl_isel_sdio_slv_dat3_cs,

	pctl_isel_emvsim1_sio, pctl_isel_emvsim1_ipp_simpd, pctl_isel_emvsim1_power_fail,

	pctl_isel_emvsim2_sio, pctl_isel_emvsim2_ipp_simpd, pctl_isel_emvsim2_power_fail,

	pctl_isel_spdif_in1,

	pctl_isel_usb_otg2_oc, pctl_isel_usb_otg_oc,

	pctl_isel_usbphy1_id, pctl_isel_usbphy2_id,

	pctl_isel_usdhc1_ipp_card_det, pctl_isel_usdhc1_ipp_wp_on, pctl_isel_usdhc2_ipp_card_det, pctl_isel_usdhc_ipp_wp_on,

	pctl_isel_xbar1_in_20, pctl_isel_xbar1_in_21, pctl_isel_xbar1_in_22, pctl_isel_xbar1_in_23,
	pctl_isel_xbar1_in_24, pctl_isel_xbar1_in_25, pctl_isel_xbar1_in_26, pctl_isel_xbar1_in_27,
	pctl_isel_xbar1_in_28, pctl_isel_xbar1_in_29, pctl_isel_xbar1_in_30, pctl_isel_xbar1_in_31,
	pctl_isel_xbar1_in_32, pctl_isel_xbar1_in_33, pctl_isel_xbar1_in_34, pctl_isel_xbar1_in_35,

	/* LPSR */
	pctl_isel_can3_canrx,

	pctl_isel_lpi2c5_scl, pctl_isel_lpi2c5_sda,

	pctl_isel_lpi2c6_scl, pctl_isel_lpi2c6_sda,

	pctl_isel_lpspi5_pcs_0, pctl_isel_lpspi5_sck, pctl_isel_lpspi5_sdi, pctl_isel_lpspi5_sdo,

	pctl_isel_lpuart11_rxd, pctl_isel_lpuart11_txd,

	pctl_isel_lpuart12_rxd, pctl_isel_lpuart12_txd,

	pctl_isel_mic_pdm_bitstream_0, pctl_isel_mic_pdm_bitstream_1, pctl_isel_mic_pdm_bitstream_2,
	pctl_isel_mic_pdm_bitstream_3,

	pctl_isel_nmi,

	pctl_isel_sai4_mclk, pctl_isel_sai4_rxbclk, pctl_isel_sai4_rxdata_0, pctl_isel_sai4_rxsync, pctl_isel_sai4_txbclk,
	pctl_isel_sai4_txsync
};


/* SRC - Reset slices */
enum {
	pctl_resetSliceMega = 0, pctl_resetSliceDisplay, pctl_resetSliceWakeup, pctl_resetSliceLpsr,
	pctl_resetSliceCM4Core, pctl_resetSliceCM7Core, pctl_resetSliceCM4Debug, pctl_resetSliceCM7Debug,
	pctl_resetSliceUSBPHY1, pctl_resetSliceUSBPHY2, pctl_resetSliceCM4Mem, pctl_resetSliceCM7Mem
};


/* Interrupts numbers */
enum { cti0_err_irq = 17 + 16, cti1_err_irq, core_irq, lpuart1_irq, lpuart2_irq, lpuart3_irq, lpuart4_irq, lpuart5_irq,
	lpuart6_irq, lpuart7_irq, lpuart8_irq, lpuart9_irq, lpuart10_irq, lpuart11_irq, lpuart12_irq, lpi2c1_irq,
	lpi2c2_irq, lpi2c3_irq, lpi2c4_irq, lpi2c5_irq, lpi2c6_irq, lpspi1_irq, lpspi2_irq, lpspi3_irq, lpspi4_irq,
	lpspi5_irq, lpspi6_irq, can1_irq, can2_irq, can3_irq, flexram_irq, kpp_irq, tsc_dig_irq, /* Reserved #53 */
	lcdif1_irq = 54 + 16, lcdif2_irq, csi_irq, pxp_irq, mipi_csi_irq, mipi_dsi_irq, gpu2d_irq, gpio6_int0_irq,
	gpio6_int1_irq, /* Reserved #63..64 */ wdog2_irq = 65 + 16, snvs_hp_wrapper_irq, snvs_hp_wrapper_tz_irq,
	snvs_lp_wrapper_irq, caam_jq0_irq, caam_jq1_irq, caam_jq2_irq, caam_jq3_irq, caam_err_irq, caam_rtic_irq,
	/* Reserved #75 */ sai1_irq = 76 + 16, sai2_irq, sai3_0_irq, sai3_1_irq, sai4_0_irq, sai4_1_irq, spdif_irq,
	/* Reserved #83..87 */ adc1_irq = 88 + 16, adc2_irq, adc3_irq, dcdc_irq, /* Reserved #92..98 */ cm7_irq = 99 + 16,
	gpio1_int0_irq, gpio1_int1_irq, gpio2_int0_irq, gpio2_int1_irq, gpio3_int0_irq, gpio3_int1_irq, gpio4_int0_irq,
	gpio4_int1_irq, gpio5_int0_irq, gpio5_int1_irq, flexio1_irq, flexio2_irq, wdog1_irq, rtwdog_irq, ewm_irq, ccm_1_irq,
	ccm2_irq, gpc_irq, mu_irq, gpt1_irq, gpt2_irq, gpt3_irq, gpt4_irq, gpt5_irq, gpt6_irq, flexpwm1_0_irq, flexpwm1_1_irq,
	flexpwm1_2_irq, flexpwm1_3_irq, flexpwm1_err_irq, flexspi1_irq, flexspi2_irq, semc_irq, usdhc1_irq, usdhc2_irq,
	usb_otg2_irq, usb_otg1_irq, enet_irq, enet_1588_timer_irq, enet_1g_rxtx_irq, enet_1g_rxtxdone_irq, enet_1g_irq,
	enet_1g_1588_timer_irq, xbar1_0_irq, xbar1_1_irq, adc_etc1_0_irq, adc_etc1_1_irq, adc_etc1_2_irq, adc_etc1_3_irq,
	adc_etc1_err_irq, adc_etc2_0_irq, adc_etc2_1_irq, adc_etc2_2_irq, adc_etc2_3_irq, adc_etc2_err_irq, pit1_irq, pit2_irq,
	acmp1_irq, acmp2_irq, acmp3_irq, acmp4_irq, acmp5_irq, acmp6_irq, acmp7_irq, acmp_lpsr, enc1_irq, enc2_irq, enc3_irq,
	enc4_irq, enc5_irq, enc6_irq, qtimer1_irq, qtimer2_irq, qtimer3_irq, qtimer4_irq, qtimer5_irq, qtimer6_irq,
	flexpwm2_0_irq, flexpwm2_1_irq, flexpwm2_2_irq, flexpwm2_3_irq, flexpwm2_err_irq, flexpwm3_0_irq, flexpwm3_1_irq,
	flexpwm3_2_irq, flexpwm3_3_irq, flexpwm3_err_irq,flexpwm4_0_irq, flexpwm4_1_irq, flexpwm4_2_irq, flexpwm4_3_irq,
	flexpwm4_err_irq, flexpwm5_0_irq, flexpwm5_1_irq, flexpwm5_2_irq, flexpwm5_3_irq, flexpwm5_err_irq, flexpwm6_0_irq,
	flexpwm6_1_irq, flexpwm6_2_irq, flexpwm6_3_irq, flexpwm6_err_irq, mic_irq, mic_err_irq, sim1_irq, sim2_irq, mecc1_irq,
	mecc1_fatal_irq, mecc2_irq, mecc2_fatal_irq, xecc_flexspi1_irq, xecc_flexspi1_fatal_irq, xecc_flexspi2_irq,
	xecc_flexspi2_fatal_irq, xecc_semc_irq, xecc_semc_fatal_irq, enet_qos_irq, enet_pmt_irq };


typedef struct {
	enum { pctl_set = 0, pctl_get } action;
	enum { pctl_devclock = 0, pctl_iogpr, pctl_iolpsrgpr, pctl_iomux, pctl_iopad, pctl_ioisel, pctl_reboot, pctl_devcache,
		pctl_lpcg, pctl_cleanInvalDCache, pctl_resetSlice, pctl_sharedGpr, pctl_invalDCache } type;

	union {
		struct {
			int dev;
			int div;
			int mux;
			int mfd;
			int mfn;
			int state;
		} devclock;

		struct {
			enum { pctl_lpcg_op_direct = 0, pctl_lpcg_op_level } op;
			int dev;
			int state;
		} lpcg;

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
			char pus;
			char pue;
			char pke;
			char ode;
			char dse;
			char sre;
			char apc;
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
			unsigned int index;
		} resetSlice;
	};
} __attribute__((packed)) platformctl_t;


/* clang-format on */

#endif
