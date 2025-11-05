/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TDA4VM basic peripherals control functions
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_ARCH_TDA4VM_H_
#define _PH_ARCH_TDA4VM_H_

#define PCTL_REBOOT_MAGIC 0xaa55aa55UL

enum tda4vm_clk_pll {
	clk_mcu_r5fss_pll0 = 0,
	clk_mcu_per_pll1,
	clk_mcu_cpsw_pll2,
	clk_main_pll0 = 3,
	clk_main_per0_pll1,
	clk_main_per1_pll2,
	clk_main_cpsw9g_pll3,
	clk_main_audio0_pll4,
	clk_main_video_pll5,
	clk_main_gpu_pll6,
	clk_main_c7x_pll7,
	clk_main_arm0_pll8,
	clk_main_ddr_pll12 = 15,
	clk_main_c66_pll13,
	clk_main_r5fss_pll14,
	clk_main_audio1_pll15,
	clk_main_dss_pll16,
	clk_main_dss_pll17,
	clk_main_dss_pll18,
	clk_main_dss_pll19,
	clk_main_dss_pll23,
	clk_main_mlb_pll24,
	clk_main_vision_pll25,
	clk_plls_count,
};


/* clang-format off */
enum tda4vm_clksels {
	clksel_wkup_per = 0,
	clksel_wkup_usart,
	clksel_wkup_gpio,
	clksel_wkup_main_pll0, clksel_wkup_main_pll1, clksel_wkup_main_pll2, clksel_wkup_main_pll3, clksel_wkup_main_pll4,
	clksel_wkup_main_pll5, clksel_wkup_main_pll6, clksel_wkup_main_pll7, clksel_wkup_main_pll8, clksel_wkup_main_pll12,
	clksel_wkup_main_pll13, clksel_wkup_main_pll14, clksel_wkup_main_pll15, clksel_wkup_main_pll16, clksel_wkup_main_pll17,
	clksel_wkup_main_pll18, clksel_wkup_main_pll19, clksel_wkup_main_pll23, clksel_wkup_main_pll24, clksel_wkup_main_pll25,
	clksel_wkup_mcu_spi0, clksel_wkup_mcu_spi1,
	clksel_mcu_efuse,
	clksel_mcu_mcan0, clksel_mcu_mcan1,
	clksel_mcu_ospi0, clksel_mcu_ospi1,
	clksel_mcu_adc0, clksel_mcu_adc1,
	clksel_mcu_enet,
	clksel_mcu_r5_core0,
	clksel_mcu_timer0, clksel_mcu_timer1, clksel_mcu_timer2, clksel_mcu_timer3, clksel_mcu_timer4, clksel_mcu_timer5,
	clksel_mcu_timer6, clksel_mcu_timer7, clksel_mcu_timer8, clksel_mcu_timer9,
	clksel_mcu_rti0, clksel_mcu_rti1,
	clksel_mcu_usart,
	clksel_gtc,
	clksel_efuse,
	clksel_icssg0, clksel_icssg1,
	clksel_pcie0, clksel_pcie1, clksel_pcie2, clksel_pcie3,
	clksel_cpsw,
	clksel_navss,
	clksel_emmc0, clksel_emmc1, clksel_emmc2,
	clksel_ufs0,
	clksel_gpmc,
	clksel_usb0, clksel_usb1,
	clksel_timer0, clksel_timer1, clksel_timer2, clksel_timer3, clksel_timer4, clksel_timer5, clksel_timer6, clksel_timer7,
	clksel_timer8, clksel_timer9, clksel_timer10, clksel_timer11, clksel_timer12, clksel_timer13, clksel_timer14, clksel_timer15,
	clksel_timer16, clksel_timer17, clksel_timer18, clksel_timer19,
	clksel_spi0, clksel_spi1, clksel_spi2, clksel_spi3, clksel_spi5, clksel_spi6, clksel_spi7,
	clksel_mcasp0, clksel_mcasp1, clksel_mcasp2, clksel_mcasp3, clksel_mcasp4, clksel_mcasp5, clksel_mcasp6, clksel_mcasp7,
	clksel_mcasp8, clksel_mcasp9, clksel_mcasp10, clksel_mcasp11,
	clksel_mcasp0_ah, clksel_mcasp1_ah, clksel_mcasp2_ah, clksel_mcasp3_ah, clksel_mcasp4_ah, clksel_mcasp5_ah, clksel_mcasp6_ah,
	clksel_mcasp7_ah, clksel_mcasp8_ah, clksel_mcasp9_ah, clksel_mcasp10_ah, clksel_mcasp11_ah,
	clksel_atl,
	clksel_dphy0,
	clksel_edp_phy0,
	clksel_wwd0, clksel_wwd1, clksel_wwd15, clksel_wwd16, clksel_wwd24, clksel_wwd25, clksel_wwd28, clksel_wwd29, clksel_wwd30, clksel_wwd31,
	clksel_serdes0, clksel_serdes0_1, clksel_serdes1, clksel_serdes1_1, clksel_serdes2, clksel_serdes2_1, clksel_serdes3, clksel_serdes3_1,
	clksel_mcan0, clksel_mcan1, clksel_mcan2, clksel_mcan3, clksel_mcan4, clksel_mcan5, clksel_mcan6, clksel_mcan7, clksel_mcan8,
	clksel_mcan9, clksel_mcan10, clksel_mcan11, clksel_mcan12, clksel_mcan13,
	clksel_pcie_refclk0, clksel_pcie_refclk1, clksel_pcie_refclk2, clksel_pcie_refclk3,
	clksel_dss_dispc0_1, clksel_dss_dispc0_2, clksel_dss_dispc0_3,
	clksel_wkup_mcu_obsclk0,
	clksel_obsclk0,
	clksels_count,
};

enum tda4vm_clkdivs {
	clkdiv_usart0 = 0,
	clkdiv_usart1,
	clkdiv_usart2,
	clkdiv_usart3,
	clkdiv_usart4,
	clkdiv_usart5,
	clkdiv_usart6,
	clkdiv_usart7,
	clkdiv_usart8,
	clkdiv_usart9,
	clkdiv_wkup_mcu_obsclk0,
	clkdiv_obsclk0,
	clkdivs_count,
};
/* clang-format on */


#define TDA4VM_GPIO_LOCK             (1 << 31) /* Lock this pad configuration from further writes */
#define TDA4VM_GPIO_WKUP_EN          (1 << 29) /* Enable wakeup operation */
#define TDA4VM_GPIO_DS_PULL_UP_NDOWN (1 << 28) /* Pull direction in Deep Sleep - pull-up (pull-down if not selected) */
#define TDA4VM_GPIO_DS_PULL_DISABLE  (1 << 27) /* Disable pull-up or pull-down in Deep Sleep */
#define TDA4VM_GPIO_DS_OUT_VAL       (1 << 26) /* Set output value in Deep Sleep to 1 */
#define TDA4VM_GPIO_DS_OUT_DIS       (1 << 25) /* Disable output in Deep Sleep */
#define TDA4VM_GPIO_DS_EN            (1 << 24) /* Set IO to OFF mode when in Deep Sleep */
#define TDA4VM_GPIO_ISO_BYP          (1 << 23) /* Bypass IO isolation */
#define TDA4VM_GPIO_ISO_OVR          (1 << 22) /* Override IO isolation */
#define TDA4VM_GPIO_TX_DIS           (1 << 21) /* Disable output driver */
#define TDA4VM_GPIO_DRV_STR_NORMAL   (0 << 19) /* Normal drive strength */
#define TDA4VM_GPIO_DRV_STR_FAST     (1 << 19) /* Fast drive strength (LVCMOS pins only) */
#define TDA4VM_GPIO_DRV_STR_SLOW     (2 << 19) /* Slow drive strength (LVCMOS pins only) */
#define TDA4VM_GPIO_RX_EN            (1 << 18) /* Enable receiver */
#define TDA4VM_GPIO_PULL_UP_NDOWN    (1 << 17) /* Pull direction - pull-up (pull-down if unset) */
#define TDA4VM_GPIO_PULL_DISABLE     (1 << 16) /* Disable pull-up or pull-down */
#define TDA4VM_GPIO_FORCE_DS_EN      (1 << 15) /* Enable pad Deep Sleep controls by overriding DMSC gating */
#define TDA4VM_GPIO_SCHMITT_EN       (1 << 14) /* Enable input Schmitt trigger */


typedef struct {
	/* clang-format off */
	enum { pctl_set = 0, pctl_get } action;
	enum {
		pctl_reboot = 0,
		pctl_pll,
		pctl_frequency,
		pctl_pinconfig,
		pctl_rat_map,
		pctl_clksel,
		pctl_clkdiv,
		pctl_cpuperfmon,
	} type;
	/* clang-format on */
	union {
		struct {
			unsigned int magic;
			unsigned int reason;
		} reboot;
		struct {
			unsigned int pll_num;
			unsigned int mult_int;
			unsigned int mult_frac;
			unsigned char pre_div;
			unsigned char post_div1;
			unsigned char post_div2;
			char is_enabled;
		} pll;
		struct {
			unsigned int pll_num;
			unsigned int hsdiv;
			unsigned long long int val;
		} frequency;
		struct {
			unsigned int pin_num;
			unsigned int flags;         /* Bitfield of TDA4VM_GPIO_* flags */
			unsigned char debounce_idx; /* Debounce period selection */
			unsigned char mux;          /* Pad mux selection */
		} pin_config;
		struct {
			unsigned int entry;
			unsigned int cpuAddr;
			unsigned long long int physAddr;
			unsigned int logSize;
			char is_enabled;
		} rat_map;
		struct {
			unsigned int sel;
			unsigned int val;
		} clksel_clkdiv;
		struct {
			char user_access;
			char div64;
			char reset_counter;
		} cpuperfmon;
	};
} __attribute__((packed)) platformctl_t;

#endif
