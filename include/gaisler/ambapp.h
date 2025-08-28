/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GRLIB AMBA Plug&Play definitions
 *
 * Copyright 2023, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PHOENIX_GAISLER_AMBAPP_H_
#define _PHOENIX_GAISLER_AMBAPP_H_

/* GRLIB Cores' IDs - pages 10-18 GRLIB IP CORE Manual
 * https://www.gaisler.com/products/grlib/grip.pdf#page=10
 */

/* Processor license functions */
#define CORE_ID_LEON3    0x003u
#define CORE_ID_LEON3FT  0x053u
#define CORE_ID_DSU3     0x004u
#define CORE_ID_L3STAT   0x098u
#define CORE_ID_LEON4    0x048u
#define CORE_ID_LEON4FT  0x048u
#define CORE_ID_L4STAT   0x047u
#define CORE_ID_DSU4     0x049u
#define CORE_ID_LEON5    0x0bau
#define CORE_ID_LEON5FT  0x0bau
#define CORE_ID_DSU5     0x0bbu
#define CORE_ID_NOEL_V   0x0bdu
#define CORE_ID_NOEL_VFT 0x0bdu
#define CORE_ID_RVDM     0x0beu
#define CORE_ID_L2CACHE  0x04bu
#define CORE_ID_L2C_LITE 0x0d0u
#define CORE_ID_GRIOMMU  0x04fu
#define CORE_ID_GRIOMMU2 0x0d3u

/* Processor support functions */
#define CORE_ID_GPTIMER    0x011u
#define CORE_ID_GRCLKGATE  0x02cu
#define CORE_ID_GRDMAC     0x095u
#define CORE_ID_GRDMAC2    0x0c0u
#define CORE_ID_GRTIMER    0x038u
#define CORE_ID_GRWATCHDOG 0x0c9u
#define CORE_ID_IRQMP      0x00du
#define CORE_ID_IRQAMP     0x00du

/* Memory controllers and supporting cores */
#define CORE_ID_DDRSPA     0x025u
#define CORE_ID_DDR2SPA    0x02eu
#define CORE_ID_MCTRL      0x00fu
#define CORE_ID_SDCTRL     0x009u
#define CORE_ID_SRCTRL     0x008u
#define CORE_ID_SSRCTRL    0x00au
#define CORE_ID_FTADDR     0x0aeu
#define CORE_ID_FTMCTRL    0x054u
#define CORE_ID_FTSDCTRL   0x055u
#define CORE_ID_FTSDCTRL64 0x058u
#define CORE_ID_FTSRCTRL   0x051u
#define CORE_ID_FTSRCTRL8  0x056u
#define CORE_ID_NANDFCTRL  0x059u
#define CORE_ID_NANDFCTRL2 0x0c5u
#define CORE_ID_SPIMCTRL   0x045u
#define CORE_ID_AHBSTAT    0x052u
#define CORE_ID_MEMSCRUB   0x057u

/* AMBA Bus control */
#define CORE_ID_AHB2AHB   0x020u
#define CORE_ID_AHB2AVLA  0x096u
#define CORE_ID_AHB2AXIB  0x09fu
#define CORE_ID_AHBBRIDGE 0x020u
#define CORE_ID_APBCTRL   0x006u
#define CORE_ID_AHBTRACE  0x017u
#define CORE_ID_MMA       0x07fu

/* PCI interface */
#define CORE_ID_GRPCI2       0x07cu
#define CORE_ID_PCITARGET    0x012u
#define CORE_ID_PCIMTF_GRPCI 0x014u
#define CORE_ID_PCITRACE     0x015u
#define CORE_ID_PCIDMA       0x016u
#define CORE_ID_PCIARB       0x010u

/* On-chip memory functions */
#define CORE_ID_AHBRAM   0x00eu
#define CORE_ID_AHBDPRAM 0x00fu
#define CORE_ID_AHBROM   0x01bu
#define CORE_ID_FTAHBRAM 0x050u

/* Serial communication */
#define CORE_ID_AHBUART     0x007u
#define CORE_ID_AHBJTAG     0x01cu
#define CORE_ID_APBPS2      0x060u
#define CORE_ID_APBUART     0x00cu
#define CORE_ID_CAN_OC      0x019u
#define CORE_ID_CANMUX      0x081u
#define CORE_ID_GRCAN       0x03du
#define CORE_ID_GRCANFD     0x0b5u
#define CORE_ID_GRHSSL      0x0c8u
#define CORE_ID_GRSPFI      0x0bcu
#define CORE_ID_GRSPW2      0x029u
#define CORE_ID_GRSPW2_DMA  0x08au
#define CORE_ID_GRSPWROUTER 0x03eu
#define CORE_ID_GRSPWTDP    0x097u
#define CORE_ID_GRSRIO      0x0a8u
#define CORE_ID_GRWIZL      0x0c7u
#define CORE_ID_I2C2AHB     0x00bu
#define CORE_ID_I2CMST      0x028u
#define CORE_ID_I2CSLV      0x03eu
#define CORE_ID_SOCBRIDGE   0x0c4u
#define CORE_ID_SPI2AHB     0x05cu
#define CORE_ID_SPICTRL     0x02du
#define CORE_ID_SPIMASTER   0x0a6u
#define CORE_ID_SPISLAVE    0x0a7u

/* Ethernet interface */
#define CORE_ID_GRETH      0x01du
#define CORE_ID_GRETH_GBIT 0x01du
#define CORE_ID_RGMII      0x093u

/* USB interface */
#define CORE_ID_GRUSBHC 0x027u
#define CORE_ID_GRUSBDC 0x022u

/* MIL-STD-1553B interface */
#define CORE_ID_GR1553B    0x04du
#define CORE_ID_GRB1553BRM 0x072u

/* Encryption and compression */
#define CORE_ID_GRAES     0x073u
#define CORE_ID_GRAES_DMA 0x07bu
#define CORE_ID_GRECC     0x074u
#define CORE_ID_GRSHYLOC  0x0b7u

/* FPGA control and error mitigation */
#define CORE_ID_GRSCRUB 0x0c1u

/* Graphics functions */
#define CORE_ID_APBVGA   0x061u
#define CORE_ID_SVGACTRL 0x063u

/* Auxiliary functions */
#define CORE_ID_GRACECTRL 0x067u
#define CORE_ID_GRADCDAC  0x036u
#define CORE_ID_GRFIFO    0x035u
#define CORE_ID_GRGPIO    0x01au
#define CORE_ID_GRGPREG   0x087u
#define CORE_ID_GRGPRBANK 0x08fu
#define CORE_ID_GRPULSE   0x037u
#define CORE_ID_GRPWM     0x04au
#define CORE_ID_GRSYSMON  0x066u
#define CORE_ID_GRVERSION 0x03au

/* Spacecraft data handling functions */
#define CORE_ID_GRTM      0x030u
#define CORE_ID_GRTM_DESC 0x084u
#define CORE_ID_GRTM_VC   0x085u
#define CORE_ID_GRTM_PAHB 0x088u
#define CORE_ID_GRGEFFE   0x086u
#define CORE_ID_GRTMRX    0x082u
#define CORE_ID_GRTC      0x031u
#define CORE_ID_GRTCTX    0x083u
#define CORE_ID_GRCTM     0x033u
#define CORE_ID_SPWCUC    0x089u
#define CORE_ID_GRPW      0x032u
#define CORE_ID_GRPWRX    0x08eu
#define CORE_ID_GRPWTX    0x08du
#define CORE_ID_APB2PW    0x03bu
#define CORE_ID_PW2APB    0x03cu
#define CORE_ID_AHB2PP    0x039u
#define CORE_ID_GRRM      0x09au

#define AMBA_TYPE_APBIO  0x1u
#define AMBA_TYPE_AHBMEM 0x2u
#define AMBA_TYPE_AHBIO  0x3u

#define BUS_AMBA_AHB 0x0u
#define BUS_AMBA_APB 0x1u


typedef struct _ambapp_dev_t {
	unsigned short devId;
	unsigned char vendor;
	unsigned char irqn;
	unsigned int bus;
	union {
		struct {
			unsigned int *base[4];
			unsigned int type[4];
		} ahb;
		struct {
			unsigned int *base;
			unsigned int type;
		} apb;
	} info;
} __attribute__((packed)) ambapp_dev_t;


#endif
