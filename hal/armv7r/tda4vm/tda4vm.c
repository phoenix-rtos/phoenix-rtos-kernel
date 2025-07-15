/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TDA4VM internal peripheral control functions
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/armv7r/armv7r.h"
#include "hal/spinlock.h"
#include "include/arch/armv7r/tda4vm/tda4vm.h"
#include "include/arch/armv7r/tda4vm/tda4vm_pins.h"

#include "hal/armv7r/halsyspage.h"
#include "tda4vm_regs.h"
#include "tda4vm.h"

#include <board_config.h>


#define PMCR_DIVIDER64     (1 << 3)
#define PMCR_COUNTER_RESET (1 << 2)

static struct {
	spinlock_t pltctlSp;
} tda4vm_common;


static const struct {
	volatile u32 *reg;
} clksels[] = {
	[clksel_wkup_per] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_per_clksel },
	[clksel_wkup_usart] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_usart_clksel },
	[clksel_wkup_gpio] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_gpio_clksel },
	[clksel_wkup_main_pll0] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll0_clksel },
	[clksel_wkup_main_pll1] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll1_clksel },
	[clksel_wkup_main_pll2] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll2_clksel },
	[clksel_wkup_main_pll3] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll3_clksel },
	[clksel_wkup_main_pll4] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll4_clksel },
	[clksel_wkup_main_pll5] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll5_clksel },
	[clksel_wkup_main_pll6] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll6_clksel },
	[clksel_wkup_main_pll7] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll7_clksel },
	[clksel_wkup_main_pll8] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll8_clksel },
	[clksel_wkup_main_pll12] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll12_clksel },
	[clksel_wkup_main_pll13] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll13_clksel },
	[clksel_wkup_main_pll14] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll14_clksel },
	[clksel_wkup_main_pll15] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll15_clksel },
	[clksel_wkup_main_pll16] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll16_clksel },
	[clksel_wkup_main_pll17] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll17_clksel },
	[clksel_wkup_main_pll18] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll18_clksel },
	[clksel_wkup_main_pll19] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll19_clksel },
	[clksel_wkup_main_pll23] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll23_clksel },
	[clksel_wkup_main_pll24] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll24_clksel },
	[clksel_wkup_main_pll25] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_main_pll25_clksel },
	[clksel_wkup_mcu_spi0] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_mcu_spi0_clksel },
	[clksel_wkup_mcu_spi1] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_mcu_spi1_clksel },
	[clksel_mcu_efuse] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_efuse_clksel },
	[clksel_mcu_mcan0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_mcan0_clksel },
	[clksel_mcu_mcan1] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_mcan1_clksel },
	[clksel_mcu_ospi0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_ospi0_clksel },
	[clksel_mcu_ospi1] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_ospi1_clksel },
	[clksel_mcu_adc0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_adc0_clksel },
	[clksel_mcu_adc1] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_adc1_clksel },
	[clksel_mcu_enet] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_enet_clksel },
	[clksel_mcu_r5_core0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_r5_core0_clksel },
	[clksel_mcu_timer0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer0_clksel },
	[clksel_mcu_timer1] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer1_clksel },
	[clksel_mcu_timer2] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer2_clksel },
	[clksel_mcu_timer3] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer3_clksel },
	[clksel_mcu_timer4] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer4_clksel },
	[clksel_mcu_timer5] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer5_clksel },
	[clksel_mcu_timer6] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer6_clksel },
	[clksel_mcu_timer7] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer7_clksel },
	[clksel_mcu_timer8] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer8_clksel },
	[clksel_mcu_timer9] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_timer9_clksel },
	[clksel_mcu_rti0] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_rti0_clksel },
	[clksel_mcu_rti1] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_rti1_clksel },
	[clksel_mcu_usart] = { CTRLMMR_MCU_BASE_ADDR + ctrlmmr_mcu_reg_usart_clksel },
	[clksel_gtc] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_gtc_clksel },
	[clksel_efuse] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_efuse_clksel },
	[clksel_icssg0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_icssg0_clksel },
	[clksel_icssg1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_icssg1_clksel },
	[clksel_pcie0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie0_clksel },
	[clksel_pcie1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie1_clksel },
	[clksel_pcie2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie2_clksel },
	[clksel_pcie3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie3_clksel },
	[clksel_cpsw] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_cpsw_clksel },
	[clksel_navss] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_navss_clksel },
	[clksel_emmc0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_emmc0_clksel },
	[clksel_emmc1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_emmc1_clksel },
	[clksel_emmc2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_emmc2_clksel },
	[clksel_ufs0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_ufs0_clksel },
	[clksel_gpmc] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_gpmc_clksel },
	[clksel_usb0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usb0_clksel },
	[clksel_usb1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usb1_clksel },
	[clksel_timer0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer0_clksel },
	[clksel_timer1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer1_clksel },
	[clksel_timer2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer2_clksel },
	[clksel_timer3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer3_clksel },
	[clksel_timer4] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer4_clksel },
	[clksel_timer5] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer5_clksel },
	[clksel_timer6] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer6_clksel },
	[clksel_timer7] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer7_clksel },
	[clksel_timer8] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer8_clksel },
	[clksel_timer9] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer9_clksel },
	[clksel_timer10] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer10_clksel },
	[clksel_timer11] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer11_clksel },
	[clksel_timer12] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer12_clksel },
	[clksel_timer13] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer13_clksel },
	[clksel_timer14] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer14_clksel },
	[clksel_timer15] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer15_clksel },
	[clksel_timer16] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer16_clksel },
	[clksel_timer17] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer17_clksel },
	[clksel_timer18] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer18_clksel },
	[clksel_timer19] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_timer19_clksel },
	[clksel_spi0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi0_clksel },
	[clksel_spi1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi1_clksel },
	[clksel_spi2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi2_clksel },
	[clksel_spi3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi3_clksel },
	[clksel_spi5] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi5_clksel },
	[clksel_spi6] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi6_clksel },
	[clksel_spi7] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_spi7_clksel },
	[clksel_mcasp0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp0_clksel },
	[clksel_mcasp1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp1_clksel },
	[clksel_mcasp2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp2_clksel },
	[clksel_mcasp3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp3_clksel },
	[clksel_mcasp4] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp4_clksel },
	[clksel_mcasp5] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp5_clksel },
	[clksel_mcasp6] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp6_clksel },
	[clksel_mcasp7] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp7_clksel },
	[clksel_mcasp8] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp8_clksel },
	[clksel_mcasp9] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp9_clksel },
	[clksel_mcasp10] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp10_clksel },
	[clksel_mcasp11] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp11_clksel },
	[clksel_mcasp0_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp0_ahclksel },
	[clksel_mcasp1_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp1_ahclksel },
	[clksel_mcasp2_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp2_ahclksel },
	[clksel_mcasp3_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp3_ahclksel },
	[clksel_mcasp4_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp4_ahclksel },
	[clksel_mcasp5_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp5_ahclksel },
	[clksel_mcasp6_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp6_ahclksel },
	[clksel_mcasp7_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp7_ahclksel },
	[clksel_mcasp8_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp8_ahclksel },
	[clksel_mcasp9_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp9_ahclksel },
	[clksel_mcasp10_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp10_ahclksel },
	[clksel_mcasp11_ah] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcasp11_ahclksel },
	[clksel_atl] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_atl_clksel },
	[clksel_dphy0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_dphy0_clksel },
	[clksel_edp_phy0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_edp_phy0_clksel },
	[clksel_wwd0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd0_clksel },
	[clksel_wwd1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd1_clksel },
	[clksel_wwd15] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd15_clksel },
	[clksel_wwd16] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd16_clksel },
	[clksel_wwd24] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd24_clksel },
	[clksel_wwd25] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd25_clksel },
	[clksel_wwd28] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd28_clksel },
	[clksel_wwd29] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd29_clksel },
	[clksel_wwd30] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd30_clksel },
	[clksel_wwd31] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_wwd31_clksel },
	[clksel_serdes0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes0_clksel },
	[clksel_serdes0_1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes0_clk1sel },
	[clksel_serdes1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes1_clksel },
	[clksel_serdes1_1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes1_clk1sel },
	[clksel_serdes2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes2_clksel },
	[clksel_serdes2_1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes2_clk1sel },
	[clksel_serdes3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes3_clksel },
	[clksel_serdes3_1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_serdes3_clk1sel },
	[clksel_mcan0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan0_clksel },
	[clksel_mcan1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan1_clksel },
	[clksel_mcan2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan2_clksel },
	[clksel_mcan3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan3_clksel },
	[clksel_mcan4] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan4_clksel },
	[clksel_mcan5] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan5_clksel },
	[clksel_mcan6] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan6_clksel },
	[clksel_mcan7] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan7_clksel },
	[clksel_mcan8] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan8_clksel },
	[clksel_mcan9] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan9_clksel },
	[clksel_mcan10] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan10_clksel },
	[clksel_mcan11] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan11_clksel },
	[clksel_mcan12] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan12_clksel },
	[clksel_mcan13] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_mcan13_clksel },
	[clksel_pcie_refclk0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie_refclk0_clksel },
	[clksel_pcie_refclk1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie_refclk1_clksel },
	[clksel_pcie_refclk2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie_refclk2_clksel },
	[clksel_pcie_refclk3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_pcie_refclk3_clksel },
	[clksel_dss_dispc0_1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_dss_dispc0_clksel1 },
	[clksel_dss_dispc0_2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_dss_dispc0_clksel2 },
	[clksel_dss_dispc0_3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_dss_dispc0_clksel3 },
	[clksel_wkup_mcu_obsclk0] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_mcu_obsclk_ctrl },
	[clksel_obsclk0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_obsclk0_ctrl },
};


static const struct {
	volatile u32 *reg;
} clkdivs[] = {
	[clkdiv_usart0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart0_clk_ctrl },
	[clkdiv_usart1] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart1_clk_ctrl },
	[clkdiv_usart2] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart2_clk_ctrl },
	[clkdiv_usart3] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart3_clk_ctrl },
	[clkdiv_usart4] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart4_clk_ctrl },
	[clkdiv_usart5] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart5_clk_ctrl },
	[clkdiv_usart6] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart6_clk_ctrl },
	[clkdiv_usart7] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart7_clk_ctrl },
	[clkdiv_usart8] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart8_clk_ctrl },
	[clkdiv_usart9] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_usart9_clk_ctrl },
	[clkdiv_wkup_mcu_obsclk0] = { CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_mcu_obsclk_ctrl },
	[clkdiv_obsclk0] = { CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_obsclk0_ctrl },
};


static volatile u32 *tda4vm_getPLLBase(unsigned pll)
{
	if (pll >= clk_main_pll0) {
		return MAIN_PLL_BASE_ADDR + ((0x1000 / 4) * (pll - clk_main_pll0));
	}

	return MCU_PLL_BASE_ADDR + ((0x1000 / 4) * pll);
}


static int tda4vm_isPLLValid(unsigned pll)
{
	/* A few of the PLLs are missing on this platform... */
	return !((pll > clk_plls_count) || ((pll > clk_main_arm0_pll8) && (pll < clk_main_ddr_pll12)));
}


static const unsigned char tda4vm_deskew_pll_val_to_divide[4] = { 4, 2, 1, 1 };


int tda4vm_getPLL(unsigned pll, tda4vm_clk_pll_t *config)
{
	volatile u32 *base;
	u32 ctrl, div, hw_config;
	if (!tda4vm_isPLLValid(pll)) {
		return -1;
	}

	base = tda4vm_getPLLBase(pll);
	hw_config = *(base + pll_reg_cfg);
	ctrl = *(base + pll_reg_ctrl);
	div = *(base + pll_reg_div_ctrl);
	if ((hw_config & 0x3) == 2) {
		/* Deskew PLL */
		config->pre_div = tda4vm_deskew_pll_val_to_divide[div & 0x3];
		config->mult_int = tda4vm_deskew_pll_val_to_divide[(div >> 12) & 0x3];
		config->mult_frac = 0;
		config->post_div1 = 1 << ((div >> 8) & 0x7);
		config->post_div2 = 1;
		/* Active when bit is 0 */
		config->is_enabled = (*(base + pll_reg_ctrl) & (1 << 4)) == 0 ? 1 : 0;
	}
	else {
		/* Fractional PLL */
		config->mult_int = *(base + pll_reg_freq_ctrl0) & ((1 << 12) - 1);
		if ((ctrl & 0x3) == 0x3) {
			/* Fractional mode active */
			config->mult_frac = *(base + pll_reg_freq_ctrl1) & ((1 << 24) - 1);
		}
		else {
			config->mult_frac = 0;
		}

		config->pre_div = div & (0x3f);
		config->post_div1 = (div >> 16) & 0x7;
		config->post_div2 = (div >> 24) & 0x7;
		/* Active when bit is 1 */
		config->is_enabled = (*(base + pll_reg_ctrl) & (1 << 15)) != 0 ? 1 : 0;
	}

	return 0;
}


u64 tda4vm_getFrequency(unsigned pll, unsigned hsdiv)
{
	volatile u32 *base;
	tda4vm_clk_pll_t config;
	u32 hw_config, hsdiv_ctrl;
	u64 multiplier_24, final_freq_24; /* 30 bit integer : 24 bit fractional part format */
	u32 rounding;
	unsigned total_division, in_frequency;
	if (hsdiv >= 16) {
		return 0;
	}

	if (tda4vm_getPLL(pll, &config) < 0) {
		return 0;
	}

	base = tda4vm_getPLLBase(pll);
	hw_config = *(base + pll_reg_cfg);
	if (((hw_config >> 16) & (1 << hsdiv)) == 0) {
		return 0;
	}

	/* TODO: detect which clock is used as PLL source */
	if (pll < clk_main_pll0) {
		in_frequency = WKUP_HFOSC0_HZ;
	}
	else {
		in_frequency = HFOSC1_HZ;
	}

	if (config.is_enabled == 0) {
		return in_frequency;
	}

	multiplier_24 = (u64)config.mult_int << 24;
	multiplier_24 |= config.mult_frac;

	total_division = config.pre_div * config.post_div1 * config.post_div2;
	if (total_division == 0) {
		return 0;
	}

	hsdiv_ctrl = *(base + pll_reg_hsdiv_ctrl0 + hsdiv);
	total_division *= (hsdiv_ctrl & 0x7f) + 1;
	final_freq_24 = multiplier_24 / total_division;
	final_freq_24 *= in_frequency;
	rounding = ((final_freq_24 & ((1 << 24) - 1)) >= (1 << 23)) ? 1 : 0;
	return (final_freq_24 >> 24) + rounding;
}


int tda4vm_setDebounceConfig(unsigned idx, unsigned period)
{
	volatile u32 *base = CTRLMMR_WKUP_BASE_ADDR;
	if ((idx == 0) || (idx > 6)) {
		return -1;
	}

	*(base + ctrlmmr_wkup_reg_dbounce_cfg1 - 1 + idx) = period & 0x3f;
	return 0;
}


int tda4vm_setPinConfig(unsigned pin, const tda4vm_pinConfig_t *config)
{
	volatile u32 *reg;
	if (pin >= pins_main_count) {
		return -1;
	}
	else if (pin >= PIN_MAIN_OFFS) {
		reg = CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_padconfig0 + pin - PIN_MAIN_OFFS;
	}
	else if (pin >= pins_wkup_count) {
		return -1;
	}
	else {
		reg = CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_padconfig0 + pin;
	}

	*reg = (config->flags & 0xffffc000) | ((config->debounce_idx & 0x3) << 11) | (config->mux & 0xf);

	return 0;
}


int tda4vm_getPinConfig(unsigned pin, tda4vm_pinConfig_t *config)
{
	volatile u32 *reg;
	u32 val;
	if (pin >= pins_main_count) {
		return -1;
	}
	else if (pin >= PIN_MAIN_OFFS) {
		reg = CTRL_MMR0_BASE_ADDR + ctrlmmr_reg_padconfig0 + pin - PIN_MAIN_OFFS;
	}
	else if (pin >= pins_wkup_count) {
		return -1;
	}
	else {
		reg = CTRLMMR_WKUP_BASE_ADDR + ctrlmmr_wkup_reg_padconfig0 + pin;
	}

	val = *reg;
	config->flags = (val & 0xffffc000);
	config->debounce_idx = ((val >> 11) & 0x3);
	config->mux = (val & 0xf);

	return 0;
}


__attribute__((noreturn)) void tda4vm_warmReset(void)
{
	volatile u32 *base = CTRLMMR_WKUP_BASE_ADDR;
	*(base + ctrlmmr_wkup_reg_mcu_warm_rst_ctrl) = 0x60000; /* Magic value to trigger reset */
	while (1) {
		/* Hang and wait for reset */
	}

	__builtin_unreachable();
}


int tda4vm_RATMapMemory(unsigned entry, addr_t cpuAddr, u64 physAddr, u32 logSize)
{
	volatile u32 *base = MCU_ARMSS_RAT_BASE_ADDR;
	u32 regions;

	regions = *(base + r5fss_rat_reg_config) & 0xff;
	if ((logSize >= 32) || (entry >= regions)) {
		return -1;
	}

	/* Regions must be aligned to size on both sides */
	if (((cpuAddr & ((1u << logSize) - 1)) != 0) || ((physAddr & ((1uLL << logSize) - 1)) != 0)) {
		return -1;
	}

	hal_cpuDataMemoryBarrier();
	*(base + r5fss_rat_reg_ctrl_0 + (entry * 4)) = 0; /* Disable translation */
	*(base + r5fss_rat_reg_base_0 + (entry * 4)) = cpuAddr;
	*(base + r5fss_rat_reg_trans_l_0 + (entry * 4)) = physAddr & 0xffffffff;
	*(base + r5fss_rat_reg_trans_u_0 + (entry * 4)) = (physAddr >> 32) & 0xffff;
	*(base + r5fss_rat_reg_ctrl_0 + (entry * 4)) = (1 << 31) | logSize; /* Enable and set size */
	hal_cpuDataMemoryBarrier();
	return 0;
}


void tda4vm_RATUnmapMemory(unsigned entry)
{
	volatile u32 *base = MCU_ARMSS_RAT_BASE_ADDR;
	u32 regions;

	regions = *(base + r5fss_rat_reg_config) & 0xff;
	if (entry >= regions) {
		return;
	}

	*(base + r5fss_rat_reg_ctrl_0 + (entry * 4)) = 0;
}


int tda4vm_setClksel(unsigned sel, unsigned val)
{
	if (sel >= clksels_count) {
		return -1;
	}

	if ((sel == clksel_wkup_mcu_obsclk0) || (sel == clksel_obsclk0)) {
		*clksels[sel].reg &= ~0x1f;
		*clksels[sel].reg |= val & 0x1f;
	}
	else {
		*clksels[sel].reg = val;
	}

	return 0;
}


int tda4vm_getClksel(unsigned sel)
{
	if (sel >= clksels_count) {
		return -1;
	}

	return (int)(*clksels[sel].reg & 0xff);
}


int tda4vm_setClkdiv(unsigned sel, unsigned val)
{
	if (sel >= clkdivs_count) {
		return -1;
	}

	if ((sel == clkdiv_wkup_mcu_obsclk0) || (sel == clkdiv_obsclk0)) {
		*clkdivs[sel].reg &= ~(0x1ff << 8);
		*clkdivs[sel].reg |= (val & 0xff) << 8;
	}
	else {
		*clkdivs[sel].reg = val & 0xff;
	}

	*clkdivs[sel].reg |= (1 << 16);
	return 0;
}


int tda4vm_getClkdiv(unsigned sel)
{
	u32 val;
	if (sel >= clkdivs_count) {
		return -1;
	}

	val = *clkdivs[sel].reg;
	if ((sel == clkdiv_wkup_mcu_obsclk0) || (sel == clkdiv_obsclk0)) {
		return (int)(val >> 8) & 0xff;
	}

	return (int)(val & 0xff);
}


static inline u32 getPMCR(void)
{
	u32 val;
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val));
	return val;
}


static inline void setPMCR(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c12, 0" ::"r"(val));
}


static inline u32 getPMUSERENR(void)
{
	u32 val;
	asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(val));
	return val;
}


static inline void setPMUSERENR(u32 val)
{
	asm volatile("mcr p15, 0, %0, c9, c14, 0" ::"r"(val));
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	spinlock_ctx_t sc;
	int ret = -1;
	u32 pmcr;
	tda4vm_pinConfig_t pinConfig;
	tda4vm_clk_pll_t pllConfig;

	hal_spinlockSet(&tda4vm_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_reboot:
			if ((data->action == pctl_set) && (data->reboot.magic == PCTL_REBOOT_MAGIC)) {
				tda4vm_warmReset();
			}
			else if (data->action == pctl_get) {
				ret = 0;
				data->reboot.reason = syspage->hs.resetReason;
			}
			break;
		case pctl_pll:
			if (data->action == pctl_get) {
				ret = tda4vm_getPLL(data->pll.pll_num, &pllConfig);
				if (ret == 0) {
					data->pll.mult_int = pllConfig.mult_int;
					data->pll.mult_frac = pllConfig.mult_frac;
					data->pll.pre_div = pllConfig.pre_div;
					data->pll.post_div1 = pllConfig.post_div1;
					data->pll.post_div2 = pllConfig.post_div2;
					data->pll.is_enabled = pllConfig.is_enabled;
				}
			}
			break;
		case pctl_frequency:
			if (data->action == pctl_get) {
				data->frequency.val = tda4vm_getFrequency(data->frequency.pll_num, data->frequency.hsdiv);
				ret = 0;
			}
			break;
		case pctl_pinconfig:
			if (data->action == pctl_set) {
				pinConfig.flags = data->pin_config.flags;
				pinConfig.debounce_idx = data->pin_config.debounce_idx;
				pinConfig.mux = data->pin_config.mux;
				ret = tda4vm_setPinConfig(data->pin_config.pin_num, &pinConfig);
			}
			else if (data->action == pctl_get) {
				ret = tda4vm_getPinConfig(data->pin_config.pin_num, &pinConfig);
				if (ret == 0) {
					data->pin_config.flags = pinConfig.flags;
					data->pin_config.debounce_idx = pinConfig.debounce_idx;
					data->pin_config.mux = pinConfig.mux;
				}
			}
			break;
		case pctl_rat_map:
			if (data->action == pctl_set) {
				if (data->rat_map.is_enabled != 0) {
					ret = tda4vm_RATMapMemory(data->rat_map.entry, data->rat_map.cpuAddr, data->rat_map.physAddr, data->rat_map.logSize);
				}
				else {
					tda4vm_RATUnmapMemory(data->rat_map.entry);
					ret = 0;
				}
			}
			break;
		case pctl_clksel:
			if (data->action == pctl_set) {
				ret = tda4vm_setClksel(data->clksel_clkdiv.sel, data->clksel_clkdiv.val);
			}
			else if (data->action == pctl_get) {
				ret = tda4vm_getClksel(data->clksel_clkdiv.sel);
				if (ret >= 0) {
					data->clksel_clkdiv.val = ret;
					ret = 0;
				}
			}
			break;
		case pctl_clkdiv:
			if (data->action == pctl_set) {
				ret = tda4vm_setClkdiv(data->clksel_clkdiv.sel, data->clksel_clkdiv.val);
			}
			else if (data->action == pctl_get) {
				ret = tda4vm_getClkdiv(data->clksel_clkdiv.sel);
				if (ret >= 0) {
					data->clksel_clkdiv.val = ret;
					ret = 0;
				}
			}
			break;
		case pctl_cpuperfmon:
			if (data->action == pctl_set) {
				pmcr = getPMCR();
				if (data->cpuperfmon.div64 != 0) {
					pmcr |= PMCR_DIVIDER64;
				}
				else {
					pmcr &= ~PMCR_DIVIDER64;
				}

				pmcr |= (data->cpuperfmon.reset_counter != 0) ? PMCR_COUNTER_RESET : 0;
				setPMCR(pmcr);
				setPMUSERENR((data->cpuperfmon.user_access != 0) ? 1 : 0);
				ret = 0;
			}
			else if (data->action == pctl_get) {
				pmcr = getPMCR();
				data->cpuperfmon.div64 = ((pmcr & PMCR_COUNTER_RESET) != 0) ? 1 : 0;
				data->cpuperfmon.reset_counter = 0;
				data->cpuperfmon.user_access = getPMUSERENR() & 1;
				ret = 0;
			}
			break;
		default:
			break;
	}

	hal_spinlockClear(&tda4vm_common.pltctlSp, &sc);

	return ret;
}


void hal_cpuReboot(void)
{
	tda4vm_warmReset();
}


void hal_wdgReload(void)
{
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&tda4vm_common.pltctlSp, "pltctl");
}


unsigned int hal_cpuGetCount(void)
{
	return NUM_CPUS;
}
