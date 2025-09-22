/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * MCXN94x basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "mcxn94x.h"

#include "hal/arm/scs.h"

#include "hal/cpu.h"
#include "include/errno.h"


extern void _interrupts_nvicSystemReset(void);


/* clang-format off */
enum {
	syscon_ahbmatprio = 4, syscon_cpu0stckcal = 14, syscon_cpu0nstckcal, syscon_cpu1stckcal,
	syscon_nmisrc = 18,
	syscon_presetctrl0 = 64, syscon_presetctrl1, syscon_presetctrl2, syscon_presetctrl3,
	syscon_presetctrlset0 = 72, syscon_presetctrlset1, syscon_presetctrlset2, syscon_presetctrlset3,
	syscon_presetctrlcrl0 = 80, syscon_presetctrlcrl1, syscon_presetctrlcrl2, syscon_presetctrlcrl3,
	syscon_ahbclkctrl0 = 128, syscon_ahbclkctrl1, syscon_ahbclkctrl2, syscon_ahbclkctrl3,
	syscon_ahbclkctrlset0 = 136, syscon_ahbclkctrlset1, syscon_ahbclkctrlset2, syscon_ahbclkctrlset3,
	syscon_ahbclkctrlclr0 = 144, syscon_ahbclkctrlclr1, syscon_ahbclkctrlclr2, syscon_ahbclkctrlclr3,
	syscon_systickclksel0 = 152, syscon_systickclksel1, syscon_tracesel,
	syscon_ctimer0clksel, syscon_ctimer1clksel, syscon_ctimer2clksel, syscon_ctimer3clksel,
	syscon_ctimer4clksel, syscon_clkoutset = 162, syscon_adc0clksel = 169, syscon_usb0clksel,
	syscon_fc0clksel = 172, syscon_fc1clksel, syscon_fc2clksel, syscon_fc3clksel, syscon_fc4clksel,
	syscon_fc5clksel, syscon_fc6clksel, syscon_fc7clksel, syscon_fc8clksel, syscon_fc9clksel,
	syscon_sctclksel = 188, syscon_systickclkdiv0 = 192, syscon_systickclkdiv1, syscon_traceclkdiv,
	syscon_tsiclksel = 212, syscon_sincfiltclksel = 216, syscon_slowclkdiv = 222, syscon_tsiclkdiv,
	syscon_ahbclkdiv, syscon_clkoutdiv, syscon_frohfdiv, syscon_wdt0clkdiv, syscon_adc0clkdiv = 229,
	syscon_usb0clkdiv, syscon_sctclkdiv = 237, syscon_pllclkdiv = 241, syscon_ctimer0clkdiv = 244,
	syscon_ctimer1clkdiv, syscon_ctimer2clkdiv, syscon_ctimer3clkdiv, syscon_ctimer4clkdiv,
	syscon_pll1clk0div, syscon_pll1clk1div, syscon_clkunlock, syscon_nvmctrl, syscon_romcr,
	syscon_smartdmaint = 261, syscon_adc1clksel = 281, syscon_adc1clkdiv, syscon_dac0clksel = 292,
	syscon_dac0clkdiv, syscon_dac1clksel, syscon_dac1clkdiv, syscon_dac2clksel, syscon_dac2clkdiv,
	syscon_flexspiclksel, syscon_flexspiclkdiv, syscon_pllclkdivsel = 331, syscon_i3c0fclksel,
	syscon_i3c0fclkstcsel, syscon_i3c0fclkstcdiv, syscon_i3c0fclksdiv, syscon_i3c0fclkdiv, syscon_i3c0fclkssel,
	syscon_micfilfclksel, syscon_micfilfclkdiv, syscon_usdhcclksel = 342, syscon_usdhcclkdiv,
	syscon_flexioclksel, syscon_flexioclkdiv, syscon_flexcan0clksel = 360, syscon_flexcan0clkdiv,
	syscon_flexcan1clksel, syscon_flexcan1clkdiv, syscon_enetrmiiclksel, syscon_enetrmiiclkdiv,
	syscon_enetptprefclksel, syscon_enetptprefclkdiv, syscon_enetphyintfsel, syscon_enetsbdflowctrl,
	syscon_ewm0clksel = 373, syscon_wdt1clksel, syscon_wdt1clkdiv, syscon_ostimerclksel,
	syscon_cmp0fclksel = 380, syscon_cmp0fclkdiv, syscon_cmp0rrclksel, syscon_rrclkdiv,
	syscon_cmp1fclksel, syscon_cmp1fclkdiv, syscon_cmp1rrclksel, syscon_cmp1rrclkdiv,
	syscon_cmp2fclksel, syscon_cmp2fclkdiv, syscon_cmp2rrclksel, syscon_cmp2rrclkdiv,
	syscon_cpuctrl = 512, syscon_cpboot, syscon_cpustat = 514, syscon_pcacctrl = 521,
	syscon_flexcomm0clkdiv = 532, syscon_flexcomm1clkdiv, syscon_flexcomm2clkdiv,
	syscon_flexcomm3clkdiv, syscon_flexcomm4clkdiv, syscon_flexcomm5clkdiv, syscon_flexcomm6clkdiv,
	syscon_flexcomm7clkdiv, syscon_flexcomm8clkdiv, syscon_flexcomm9clkdiv,
	syscon_sai0clksel = 544, syscon_sai1clksel, syscon_sai0clkdiv, syscon_sai1clkdiv,
	syscon_emvsim0clksel, syscon_emvsim1clksel, syscon_emvsim0clkdiv, syscon_emvsim1clkdiv,
	syscon_clockctrl = 646, syscon_i3c1fclksel = 716, syscon_i3c1fclkstcsel, syscon_i3c1fclkstcdiv,
	syscon_i3c1fclksdiv, syscon_i3c1fclkdiv, syscon_i3c1fclkssel, syscon_etbstatus = 724,
	syscon_etbcounterctrl, syscon_etbcounterreload, syscon_etbcountervalue, syscon_graycodelsb,
	syscon_graycodemsb, syscon_binarycodelsb, syscon_binarycodemsb, syscon_autoclkgateoverride = 897,
	syscon_autoclkgataoverridec = 907, syscon_pwm0subctl = 910, syscon_pwm1subctl,
	syscon_ctimerglobalstarten, syscon_eccenablectrl, syscon_jtagid = 1020, syscon_devicetype,
	syscon_deviceid0, syscon_dieid
};


enum {
	port_verid = 0, port_gpclr = 4, port_gpchr, port_config = 8, port_edfr = 16, port_edier,
	port_edcr, port_calib0 = 24, port_calib1, port_pcr0 = 32, port_pcr1, port_pcr2, port_pcr3,
	port_pcr4, port_pcr5, port_pcr6, port_pcr7, port_pcr8, port_pcr9, port_pcr10, port_pcr11,
	port_pcr12, port_pcr13, port_pcr14, port_pcr15, port_pcr16, port_pcr17, port_pcr18,
	port_pcr19, port_pcr20, port_pcr21, port_pcr22, port_pcr23, port_pcr24, port_pcr25,
	port_pcr26, port_pcr27, port_pcr28, port_pcr29, port_pcr30, port_pcr31
};


enum {
	inputmux_sct0inmux0 = 0, inputmux_sct0inmux1, inputmux_sct0inmux2, inputmux_sct0inmux3,
	inputmux_sct0inmux4, inputmux_sct0inmux5, inputmux_sct0inmux6, inputmux_sct0inmux7,
	inputmux_ctimer0cap0, inputmux_ctimer0cap1, inputmux_ctimer0cap2, inputmux_ctimer0cap3,
	inputmux_timer0trig, inputmux_ctimer1cap0 = 16, inputmux_ctimer1cap1, inputmux_ctimer1cap2,
	inputmux_ctimer1cap3, inputmux_timer1trig, inputmux_ctimer2cap0 = 24, inputmux_ctimer2cap1,
	inputmux_ctimer2cap2, inputmux_ctimer2cap3, inputmux_timer2trig, inputmux_smartdmaarchbinmux0 = 40,
	inputmux_smartdmaarchbinmux1, inputmux_smartdmaarchbinmux2, inputmux_smartdmaarchbinmux3,
	inputmux_smartdmaarchbinmux4, inputmux_smartdmaarchbinmux5, inputmux_smartdmaarchbinmux6,
	inputmux_smartdmaarchbinmux7, inputmux_pintsel0, inputmux_pintsel1, inputmux_pintsel2,
	inputmux_pintsel3, inputmux_pintsel4, inputmux_pintsel5, inputmux_pintsel6, inputmux_pintsel7,
	inputmux_freqmesref = 96, inputmux_freqmeastar, inputmux_ctimer3cap0 = 105, inputmux_ctimer3cap1,
	inputmux_ctimer3cap2, inputmux_ctimer3cap3, inputmux_timer3trig, inputmux_ctimer4cap0 = 112,
	inputmux_ctimer4cap1, inputmux_ctimer4cap2, inputmux_ctimer4cap3, inputmux_timer4trig,
	inputmux_cmp0trig = 152, inputmux_adc0trig0 = 160, inputmux_adc0trig1, inputmux_adc0trig2,
	inputmux_adc0trig3, inputmux_adc1trig0 = 176, inputmux_adc1trig1, inputmux_adc1trig2,
	inputmux_adc1trig3, inputmux_dac0trig = 192, inputmux_dac1trig = 200, inputmux_dac2trig = 208,
	inputmux_enc0trig = 216, inputmux_enc0home, inputmux_enc0index, inputmux_enc0phaseb,
	inputmux_enc0phasea, inputmux_enc1trig = 224, inputmux_enc1home, inputmux_enc1index,
	inputmux_enc1phaseb, inputmux_enc1phasea, inputmux_flexpwm0sm0extsync = 232,
	inputmux_flexpwm0sm1extsync, inputmux_flexpwm0sm2extsync, inputmux_flexpwm0sm3extsync,
	inputmux_flexpwm0sm0exta, inputmux_flexpwm0sm1exta, inputmux_flexpwm0sm2exta,
	inputmux_flexpwm0sm3exta, inputmux_flexpwm0extforce, inputmux_flexpwm0fault0,
	inputmux_flexpwm0fault1, inputmux_flexpwm0fault2, inputmux_flexpwm0fault3,
	inputmux_flexpwm1sm0extsync = 248, inputmux_flexpwm1sm1extsync, inputmux_flexpwm1sm2extsync,
	inputmux_flexpwm1sm3extsync, inputmux_flexpwm1sm0exta, inputmux_flexpwm1sm1exta,
	inputmux_flexpwm1sm2exta, inputmux_flexpwm1sm3exta, inputmux_flexpwm1extforce,
	inputmux_flexpwm1fault0, inputmux_flexpwm1fault1, inputmux_flexpwm1fault2,
	inputmux_flexpwm1fault3, inputmux_pwm0extclk = 264, inputmux_pwm1extclk,
	inputmux_evtgtrig0 = 272, inputmux_evtgtrig1, inputmux_evtgtrig2, inputmux_evtgtrig3,
	inputmux_evtgtrig4, inputmux_evtgtrig5, inputmux_evtgtrig6, inputmux_evtgtrig7,
	inputmux_evtgtrig8, inputmux_evtgtrig9, inputmux_evtgtrig10, inputmux_evtgtrig11,
	inputmux_evtgtrig12, inputmux_evtgtrig13, inputmux_evtgtrig14, inputmux_evtgtrig15,
	inputmux_usbfstrig, inputmux_tsitrig = 296, inputmux_exttrig0 = 304, inputmux_exttrig1,
	inputmux_exttrig2, inputmux_exttrig3, inputmux_exttrig4, inputmux_exttrig5, inputmux_exttrig6,
	inputmux_exttrig7, inputmux_cmp1trig = 312, inputmux_cmp2trig = 320,
	inputmux_sincfilterch0 = 328, inputmux_sincfilterch1, inputmux_sincfilterch2,
	inputmux_sincfilterch3, inputmux_sincfilterch4, inputmux_opamp0trig = 352,
	inputmux_opamp1trig, inputmux_opamp2trig, inputmux_flexcomm0trig = 360,
	inputmux_flexcomm1trig = 368, inputmux_flexcomm2trig = 376, inputmux_flexcomm3trig = 384,
	inputmux_flexcomm4trig = 392, inputmux_flexcomm5trig = 400, inputmux_flexcomm6trig = 408,
	inputmux_flexcomm7trig = 416, inputmux_flexcomm8trig = 424, inputmux_flexcomm9trig = 436,
	inputmux_flexiotrig0 = 440, inputmux_flexiotrig1, inputmux_flexiotrig2, inputmux_flexiotrig3,
	inputmux_flexiotrig4, inputmux_flexiotrig5, inputmux_flexiotrig6, inputmux_flexiotrig7,
	inputmux_dma0reqenable0 = 448, inputmux_dma0reqenable0set, inputmux_dma0reqenable0clr,
	inputmux_dma0reqenable0tog, inputmux_dma0reqenable1, inputmux_dma0reqenable1set,
	inputmux_dma0reqenable1clr, inputmux_dma0reqenable1tog, inputmux_dma0reqenable2,
	inputmux_dma0reqenable2set, inputmux_dma0reqenable2clr,inputmux_dma0reqenable2tog,
	inputmux_dma0reqenable3, inputmux_dma0reqenable3set, inputmux_dma0reqenable3clr,
	inputmux_dma1reqenable0 = 480, inputmux_dma1reqenable0set, inputmux_dma1reqenable0clr,
	inputmux_dma1reqenable0tog, inputmux_dma1reqenable1, inputmux_dma1reqenable1set,
	inputmux_dma1reqenable1clr, inputmux_dma1reqenable1tog, inputmux_dma1reqenable2,
	inputmux_dma1reqenable2set, inputmux_dma1reqenable2clr, inputmux_dma1reqenable2tog,
	inputmux_dma1reqenable3, inputmux_dma1reqenable3set, inputmux_dma1reqenable3clr
};
/* clang-format on */


static struct {
	volatile u32 *syscon;
	volatile u32 *port[6];
	volatile u32 *inputmux;

	spinlock_t pltctlSp;
	unsigned int resetFlags;
} n94x_common;


int _mcxn94x_portPinConfig(int pin, int mux, int options)
{
	int port = pin / 32;

	pin %= 32;

	if (port >= (sizeof(n94x_common.port) / sizeof(*n94x_common.port))) {
		return -EINVAL;
	}

	*(n94x_common.port[port] + port_pcr0 + pin) = (((mux & 0xf) << 8) | (options & 0x307f));

	return 0;
}


u64 _mcxn94x_sysconGray2Bin(u64 gray)
{
	u64 ret;

	*(n94x_common.syscon + syscon_graycodelsb) = gray & 0xffffffff;
	*(n94x_common.syscon + syscon_graycodemsb) = gray >> 32;
	hal_cpuDataMemoryBarrier();

	ret = *(n94x_common.syscon + syscon_binarycodelsb);
	ret |= ((u64)*(n94x_common.syscon + syscon_binarycodemsb)) << 32;

	return ret;
}


static int _mcxn94x_sysconGetRegs(int dev, volatile u32 **selr, volatile u32 **divr)
{
	if ((dev < pctl_rom) || (dev > pctl_i3c1stc)) {
		return -1;
	}

	*selr = NULL;
	*divr = NULL;

	switch (dev) {
		case pctl_flexspi:
			*selr = n94x_common.syscon + syscon_flexspiclksel;
			*divr = n94x_common.syscon + syscon_flexspiclkdiv;
			break;

		case pctl_adc0:
			*selr = n94x_common.syscon + syscon_adc0clksel;
			*divr = n94x_common.syscon + syscon_adc0clkdiv;
			break;

		case pctl_adc1:
			*selr = n94x_common.syscon + syscon_adc1clksel;
			*divr = n94x_common.syscon + syscon_adc1clkdiv;
			break;

		case pctl_dac0:
			*selr = n94x_common.syscon + syscon_dac0clksel;
			*divr = n94x_common.syscon + syscon_dac0clkdiv;
			break;

		case pctl_dac1:
			*selr = n94x_common.syscon + syscon_dac1clksel;
			*divr = n94x_common.syscon + syscon_dac1clkdiv;
			break;

		case pctl_dac2:
			*selr = n94x_common.syscon + syscon_dac2clksel;
			*divr = n94x_common.syscon + syscon_dac2clkdiv;
			break;

		case pctl_timer0:
			*selr = n94x_common.syscon + syscon_ctimer0clksel;
			*divr = n94x_common.syscon + syscon_ctimer0clkdiv;
			break;

		case pctl_timer1:
			*selr = n94x_common.syscon + syscon_ctimer1clksel;
			*divr = n94x_common.syscon + syscon_ctimer1clkdiv;
			break;

		case pctl_timer2:
			*selr = n94x_common.syscon + syscon_ctimer2clksel;
			*divr = n94x_common.syscon + syscon_ctimer2clkdiv;
			break;

		case pctl_timer3:
			*selr = n94x_common.syscon + syscon_ctimer3clksel;
			*divr = n94x_common.syscon + syscon_ctimer3clkdiv;
			break;

		case pctl_timer4:
			*selr = n94x_common.syscon + syscon_ctimer4clksel;
			*divr = n94x_common.syscon + syscon_ctimer4clkdiv;
			break;

		case pctl_sct:
			*selr = n94x_common.syscon + syscon_sctclksel;
			*divr = n94x_common.syscon + syscon_sctclkdiv;
			break;

		case pctl_ostimer:
			*selr = n94x_common.syscon + syscon_ostimerclksel;
			break;

		case pctl_ewm:
			*selr = n94x_common.syscon + syscon_ewm0clksel;
			break;

		case pctl_wwdt0:
			*divr = n94x_common.syscon + syscon_wdt0clkdiv;
			break;

		case pctl_wwdt1:
			*selr = n94x_common.syscon + syscon_wdt1clksel;
			*divr = n94x_common.syscon + syscon_wdt1clkdiv;
			break;

		case pctl_usb0fs:
			*selr = n94x_common.syscon + syscon_usb0clksel;
			*divr = n94x_common.syscon + syscon_usb0clkdiv;
			break;

		case pctl_evsim0:
			*selr = n94x_common.syscon + syscon_emvsim0clksel;
			*divr = n94x_common.syscon + syscon_emvsim0clkdiv;
			break;

		case pctl_evsim1:
			*selr = n94x_common.syscon + syscon_emvsim1clksel;
			*divr = n94x_common.syscon + syscon_emvsim1clkdiv;
			break;

		case pctl_cmp0:
			*selr = n94x_common.syscon + syscon_cmp0fclksel;
			*divr = n94x_common.syscon + syscon_cmp0fclkdiv;
			break;

		case pctl_cmp1:
			*selr = n94x_common.syscon + syscon_cmp1fclksel;
			*divr = n94x_common.syscon + syscon_cmp1fclkdiv;
			break;

		case pctl_cmp2:
			*selr = n94x_common.syscon + syscon_cmp2fclksel;
			*divr = n94x_common.syscon + syscon_cmp2fclkdiv;
			break;

		case pctl_cmp0rr:
			*selr = n94x_common.syscon + syscon_cmp0rrclksel;
			break;

		case pctl_cmp1rr:
			*selr = n94x_common.syscon + syscon_cmp1rrclksel;
			break;

		case pctl_cmp2rr:
			*selr = n94x_common.syscon + syscon_cmp2rrclksel;
			break;

		case pctl_fc0:
			*selr = n94x_common.syscon + syscon_fc0clksel;
			*divr = n94x_common.syscon + syscon_flexcomm0clkdiv;
			break;

		case pctl_fc1:
			*selr = n94x_common.syscon + syscon_fc1clksel;
			*divr = n94x_common.syscon + syscon_flexcomm1clkdiv;
			break;

		case pctl_fc2:
			*selr = n94x_common.syscon + syscon_fc2clksel;
			*divr = n94x_common.syscon + syscon_flexcomm2clkdiv;
			break;

		case pctl_fc3:
			*selr = n94x_common.syscon + syscon_fc3clksel;
			*divr = n94x_common.syscon + syscon_flexcomm3clkdiv;
			break;

		case pctl_fc4:
			*selr = n94x_common.syscon + syscon_fc4clksel;
			*divr = n94x_common.syscon + syscon_flexcomm4clkdiv;
			break;

		case pctl_fc5:
			*selr = n94x_common.syscon + syscon_fc5clksel;
			*divr = n94x_common.syscon + syscon_flexcomm5clkdiv;
			break;

		case pctl_fc6:
			*selr = n94x_common.syscon + syscon_fc6clksel;
			*divr = n94x_common.syscon + syscon_flexcomm6clkdiv;
			break;

		case pctl_fc7:
			*selr = n94x_common.syscon + syscon_fc7clksel;
			*divr = n94x_common.syscon + syscon_flexcomm7clkdiv;
			break;

		case pctl_fc8:
			*selr = n94x_common.syscon + syscon_fc8clksel;
			*divr = n94x_common.syscon + syscon_flexcomm8clkdiv;
			break;

		case pctl_fc9:
			*selr = n94x_common.syscon + syscon_fc9clksel;
			*divr = n94x_common.syscon + syscon_flexcomm9clkdiv;
			break;

		case pctl_flexcan0:
			*selr = n94x_common.syscon + syscon_flexcan0clksel;
			*divr = n94x_common.syscon + syscon_flexcan0clkdiv;
			break;

		case pctl_flexcan1:
			*selr = n94x_common.syscon + syscon_flexcan1clksel;
			*divr = n94x_common.syscon + syscon_flexcan1clkdiv;
			break;

		case pctl_flexio:
			*selr = n94x_common.syscon + syscon_flexioclksel;
			*divr = n94x_common.syscon + syscon_flexioclkdiv;
			break;

		case pctl_usdhc:
			*selr = n94x_common.syscon + syscon_usdhcclksel;
			*divr = n94x_common.syscon + syscon_usdhcclkdiv;
			break;

		case pctl_sinc:
			*selr = n94x_common.syscon + syscon_sincfiltclksel;
			break;

		case pctl_i3c0:
			*selr = n94x_common.syscon + syscon_i3c0fclksel;
			*divr = n94x_common.syscon + syscon_i3c0fclkdiv;
			break;

		case pctl_i3c1:
			*selr = n94x_common.syscon + syscon_i3c1fclksel;
			*divr = n94x_common.syscon + syscon_i3c1fclkdiv;
			break;

		case pctl_i3c0s:
			*selr = n94x_common.syscon + syscon_i3c0fclkssel;
			*divr = n94x_common.syscon + syscon_i3c0fclksdiv;
			break;

		case pctl_i3c1s:
			*selr = n94x_common.syscon + syscon_i3c1fclkssel;
			*divr = n94x_common.syscon + syscon_i3c1fclksdiv;
			break;

		case pctl_i3c0stc:
			*selr = n94x_common.syscon + syscon_i3c0fclkstcsel;
			*divr = n94x_common.syscon + syscon_i3c0fclkstcdiv;
			break;

		case pctl_i3c1stc:
			*selr = n94x_common.syscon + syscon_i3c1fclkstcsel;
			*divr = n94x_common.syscon + syscon_i3c1fclkstcdiv;
			break;

		case pctl_sai0:
			*selr = n94x_common.syscon + syscon_sai0clksel;
			*divr = n94x_common.syscon + syscon_sai0clkdiv;
			break;

		case pctl_sai1:
			*selr = n94x_common.syscon + syscon_sai1clksel;
			*divr = n94x_common.syscon + syscon_sai1clkdiv;
			break;

		/* enet TODO */

		case pctl_micfil:
			*selr = n94x_common.syscon + syscon_micfilfclksel;
			*divr = n94x_common.syscon + syscon_micfilfclkdiv;
			break;

		case pctl_tsi:
			*selr = n94x_common.syscon + syscon_tsiclksel;
			*divr = n94x_common.syscon + syscon_tsiclkdiv;
			break;

		default:
			break;
	}

	return 0;
}


static int _mcxn94x_sysconGetDevClk(int dev, unsigned int *sel, unsigned int *div, int *enable)
{
	volatile u32 *selr, *divr;

	if (_mcxn94x_sysconGetRegs(dev, &selr, &divr) < 0) {
		return -1;
	}

	if (sel != NULL) {
		*sel = *selr & 0x7;
	}

	if (div != NULL) {
		*div = *divr & 0xff;
	}

	*enable = (*(n94x_common.syscon + syscon_ahbclkctrlset0 + (dev / 32)) & 1 << (dev & 0x1f)) ? 1 : 0;

	return 0;
}

static void _mcxn94x_sysconSetDevClkState(int dev, int enable)
{
	hal_cpuDataMemoryBarrier();
	if (enable != 0) {
		/* cmp0 and cmp1 fields are "reserved", let's try to control them anyway */
		*(n94x_common.syscon + syscon_ahbclkctrlset0 + (dev / 32)) = 1 << (dev & 0x1f);
	}
	else {
		*(n94x_common.syscon + syscon_ahbclkctrlclr0 + (dev / 32)) = 1 << (dev & 0x1f);
	}
	hal_cpuDataMemoryBarrier();
}


int _mcxn94x_sysconSetDevClk(int dev, unsigned int sel, unsigned int div, int enable)
{
	volatile u32 *selr = NULL, *divr = NULL;

	if (_mcxn94x_sysconGetRegs(dev, &selr, &divr) < 0) {
		return -1;
	}

	/* Disable the clock only if it can be reconfigured */
	if (selr != NULL || divr != NULL) {
		_mcxn94x_sysconSetDevClkState(dev, 0);
	}

	if (selr != NULL) {
		*selr = sel & 0x7;
	}

	if (divr != NULL) {
		*divr = div & 0xff;

		/* Unhalt the divider */
		*divr &= ~(1 << 30);
	}

	_mcxn94x_sysconSetDevClkState(dev, enable);

	return 0;
}


int _mcxn94x_sysconDevReset(int dev, int state)
{
	volatile u32 *reg = n94x_common.syscon + syscon_presetctrl0;

	if ((dev < pctl_rom) || (dev > pctl_sema42)) {
		return -1;
	}

	/* Select relevant AHB register */
	reg += dev / 32;

	/* Need to disable the clock before the reset */
	if (state != 0) {
		_mcxn94x_sysconSetDevClkState(dev, 0);
	}

	if (state != 0) {
		*(reg + (syscon_presetctrlset0 - syscon_presetctrl0)) = 1 << (dev & 0x1f);
	}
	else {
		*(reg + (syscon_presetctrlcrl0 - syscon_presetctrl0)) = 1 << (dev & 0x1f);
	}
	hal_cpuDataMemoryBarrier();

	return 0;
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = ptr;
	int ret = -EINVAL;
	unsigned int sel, div;
	int enable;
	spinlock_ctx_t sp;

	hal_spinlockSet(&n94x_common.pltctlSp, &sp);
	switch (data->type) {
		case pctl_reboot:
			if (data->action == pctl_set) {
				if (data->reboot.magic == PCTL_REBOOT_MAGIC) {
					_hal_scsSystemReset();
				}
			}
			else {
				if (data->action == pctl_get) {
					data->reboot.reason = n94x_common.resetFlags;
					ret = 0;
				}
			}
			break;

		case pctl_devclk:
			if (data->action == pctl_set) {
				ret = _mcxn94x_sysconSetDevClk(data->devClk.dev, data->devClk.sel,
					data->devClk.div, data->devClk.enable);
			}
			else if (data->action == pctl_get) {
				ret = _mcxn94x_sysconGetDevClk(data->devClk.dev, &sel, &div, &enable);
				if (ret >= 0) {
					data->devClk.sel = sel;
					data->devClk.div = div;
					data->devClk.enable = enable;
				}
			}
			else {
				ret = -EINVAL;
			}
			break;

		case pctl_devrst:
			if (data->action != pctl_set) {
				ret = -ENOSYS;
			}
			else {
				ret = _mcxn94x_sysconDevReset(data->devRst.dev, data->devRst.state);
			}
			break;

		case pctl_pinConf:
			if (data->action != pctl_set) {
				ret = -ENOSYS;
			}
			else {
				ret = _mcxn94x_portPinConfig(data->pinConf.pin, data->pinConf.mux,
					data->pinConf.options);
			}
			break;

		case pctl_cpuid:
			if (data->action != pctl_get) {
				ret = -ENOSYS;
			}
			else {
#ifdef MCX_USE_CPU1
				data->cpuid = 1;
#else
				data->cpuid = 0;
#endif
				ret = 0;
			}
			break;

		default:
			ret = -EINVAL;
			break;
	}
	hal_spinlockClear(&n94x_common.pltctlSp, &sp);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&n94x_common.pltctlSp, "pltctl");
}


void _mcxn94x_init(void)
{
	n94x_common.syscon = (void *)0x40000000;
	n94x_common.port[0] = (void *)0x40116000;
	n94x_common.port[1] = (void *)0x40117000;
	n94x_common.port[2] = (void *)0x40118000;
	n94x_common.port[3] = (void *)0x40119000;
	n94x_common.port[4] = (void *)0x4011a000;
	n94x_common.port[5] = (void *)0x40042000;
	n94x_common.inputmux = (void *)0x40006000;

	_hal_scsInit();

	/* resetFlags TODO */
}
