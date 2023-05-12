/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "gr716.h"
#include "../sparcv8leon3.h"
#include "../../spinlock.h"
#include "../../../include/arch/gr716.h"

#define GRGPREG_BASE ((void *)0x8000D000)
#define CGU_BASE0    ((void *)0x80006000)
#define CGU_BASE1    ((void *)0x80007000)


/* GPIO */

enum {
	gpio_data = 0,    /* Port data reg : 0x00 */
	gpio_out,         /* Output reg : 0x04 */
	gpio_dir,         /* Port direction reg : 0x08 */
	gpio_imask,       /* Interrupt mask reg : 0x0C */
	gpio_ipol,        /* Interrupt polarity reg : 0x10 */
	gpio_iedge,       /* Interrupt edge reg : 0x14 */
					  /* reserved - 0x18 */
	gpio_cap = 7,     /* Port capability reg : 0x1C */
	gpio_irqmapr,     /* Interrupt map register n : 0x20 - 0x3C */
	gpio_iavail = 16, /* Interrupt available reg : 0x40 */
	gpio_iflag,       /* Interrupt flag reg : 0x44 */
	gpio_ipen,        /* Interrupt enable reg : 0x48 */
	gpio_pulse,       /* Pulse reg : 0x4C */

	gpio_ie_lor, /* Interrupt enable logical OR reg : 0x50 */
	gpio_po_lor, /* Port output logical OR reg : 0x54 */
	gpio_pd_lor, /* Port direction logical OR reg : 0x58 */
	gpio_im_lor, /* Interrupt mask logical OR reg : 0x5C */

	gpio_ie_land, /* Interrupt enable logical AND reg : 0x60 */
	gpio_po_land, /* Port output logical AND reg : 0x64 */
	gpio_pd_land, /* Port direction logical AND reg : 0x68 */
	gpio_im_land, /* Interrupt mask logical AND reg : 0x6C */

	gpio_ie_lxor, /* Interrupt enable logical XOR reg : 0x70 */
	gpio_po_lxor, /* Port output logical XOR reg : 0x74 */
	gpio_pd_lxor, /* Port direction logical XOR reg : 0x78 */
	gpio_im_lxor, /* Interrupt mask logical XOR reg : 0x7C */

	gpio_ie_sc,      /* Interrupt enable logical set/clear reg : 0x80 - 0x8C */
	gpio_po_sc = 36, /* Port output logical set/clear reg : 0x90 - 0x9C */
	gpio_pd_sc = 40, /* Port direction logical set/clear reg : 0xA0 - 0xAC */
	gpio_im_sc = 44  /* Interrupt mask logical set/clear reg : 0xB0 - 0xBC */
};

/* System configuration registers */

enum {
	cfg_gp0 = 0,   /* Sys IO config GPIO 0-7      : 0x00 */
	cfg_gp1,       /* Sys IO config GPIO 8-15     : 0x04 */
	cfg_gp2,       /* Sys IO config GPIO 16-23    : 0x08 */
	cfg_gp3,       /* Sys IO config GPIO 24-31    : 0x0C */
	cfg_gp4,       /* Sys IO config GPIO 32-39    : 0x10 */
	cfg_gp5,       /* Sys IO config GPIO 40-47    : 0x14 */
	cfg_gp6,       /* Sys IO config GPIO 48-55    : 0x18 */
	cfg_gp7,       /* Sys IO config GPIO 56-63    : 0x1C */
	cfg_pullup0,   /* Pull-up config GPIO 0-31    : 0x20 */
	cfg_pullup1,   /* Pull-up config GPIO 32-63   : 0x24 */
	cfg_pulldn0,   /* Pull-down config GPIO 0-31  : 0x28 */
	cfg_pulldn1,   /* Pull-down config GPIO 32-63 : 0x2C */
	cfg_lvds,      /* LVDS config                 : 0x30 */
	cfg_prot = 16, /* Sys IO config protection    : 0x40 */
	cfg_eirq,      /* Sys IO config err interrupt : 0x44 */
	cfg_estat      /* Sys IO config err status    : 0x48 */
};


/* Clock gating unit */

enum {
	cgu_unlock = 0, /* Unlock register        : 0x00 */
	cgu_clk_en,     /* Clock enable register  : 0x04 */
	cgu_core_reset, /* Core reset register    : 0x08 */
	cgu_override,   /* Override register - only primary CGU : 0x0C */
};


struct {
	spinlock_t pltctlSp;

	volatile u32 *GRGPIO_0;
	volatile u32 *GRGPIO_1;
	volatile u32 *grgpreg_base;
	volatile u32 *cgu_base0;
	volatile u32 *cgu_base1;
} gr716_common;


/* GPIO */

static inline int _gr716_gpioPinToPort(u8 pin)
{
	return (pin >> 5);
}


int _gr716_gpioWritePin(u8 pin, u8 val)
{
	int err = 0;
	u32 msk = val << (pin & 0x1F);

	switch (_gr716_gpioPinToPort(pin)) {
		case GPIO_PORT_0:
			*(gr716_common.GRGPIO_0 + gpio_out) = (*(gr716_common.GRGPIO_0 + gpio_out) & ~msk) | msk;
			break;
		case GPIO_PORT_1:
			*(gr716_common.GRGPIO_1 + gpio_out) = (*(gr716_common.GRGPIO_1 + gpio_out) & ~msk) | msk;
			break;
		default:
			err = -1;
			break;
	}

	return err;
}


int _gr716_gpioReadPin(u8 pin, u8 *val)
{
	int err = 0;

	switch (_gr716_gpioPinToPort(pin)) {
		case GPIO_PORT_0:
			*val = (*(gr716_common.GRGPIO_0 + gpio_out) >> (pin & 0x1F)) & 0x1;
			break;
		case GPIO_PORT_1:
			*val = (*(gr716_common.GRGPIO_1 + gpio_out) >> (pin & 0x1F)) & 0x1;
			break;
		default:
			err = -1;
			break;
	}

	return err;
}


int _gr716_gpioGetPinDir(u8 pin, u8 *dir)
{
	int err = 0;

	switch (_gr716_gpioPinToPort(pin)) {
		case GPIO_PORT_0:
			*dir = (*(gr716_common.GRGPIO_0 + gpio_dir) >> (pin & 0x1F)) & 0x1;
			break;
		case GPIO_PORT_1:
			*dir = (*(gr716_common.GRGPIO_1 + gpio_dir) >> (pin & 0x1F)) & 0x1;
			break;
		default:
			err = -1;
			break;
	}

	return err;
}


int _gr716_gpioSetPinDir(u8 pin, u8 dir)
{
	int err = 0;
	u32 msk = dir << (pin & 0x1F);

	switch (_gr716_gpioPinToPort(pin)) {
		case GPIO_PORT_0:
			*(gr716_common.GRGPIO_0 + gpio_dir) = (*(gr716_common.GRGPIO_0 + gpio_dir) & ~msk) | msk;
			break;
		case GPIO_PORT_1:
			*(gr716_common.GRGPIO_1 + gpio_dir) = (*(gr716_common.GRGPIO_1 + gpio_dir) & ~msk) | msk;
			break;
		default:
			err = -1;
			break;
	}
	return err;
}


int _gr716_getIOCfg(u8 pin, u8 *opt, u8 *dir, u8 *pullup, u8 *pulldn)
{
	if (pin > 63 || _gr716_gpioGetPinDir(pin, dir) < 0) {
		return -1;
	}

	*opt = (*(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8)) >> ((pin % 8) << 2)) & 0xf;

	*pullup = (*(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32)) >> (pin % 32)) & 0x1;

	*pulldn = (*(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32)) >> (pin % 32)) & 0x1;

	return 0;
}


int _gr716_setIOCfg(u8 pin, u8 opt, u8 dir, u8 pullup, u8 pulldn)
{
	volatile u32 old_cfg;

	if (pin > 63 || _gr716_gpioSetPinDir(pin, dir) < 0) {
		return -1;
	}

	old_cfg = *(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8));

	*(gr716_common.grgpreg_base + cfg_gp0 + (pin / 8)) =
		(old_cfg & ~(0xf << ((pin % 8) << 2))) | (opt << ((pin % 8) << 2));

	old_cfg = *(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32));

	*(gr716_common.grgpreg_base + cfg_pullup0 + (pin / 32)) =
		(old_cfg & ~(1 << (pin % 32))) | (pullup << (pin % 32));

	old_cfg = *(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32));

	*(gr716_common.grgpreg_base + cfg_pulldn0 + (pin / 32)) =
		(old_cfg & ~(1 << (pin % 32))) | (pulldn << (pin % 32));

	return 0;
}


/* CGU setup - section 26.2 GR716 manual */

void _gr716_cguClkEnable(u32 cgu, u32 device)
{
	volatile u32 *cgu_base = (cgu == cgu_primary) ? gr716_common.cgu_base0 : gr716_common.cgu_base1;
	u32 msk = 1 << device;

	*(cgu_base + cgu_unlock) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_core_reset) |= msk;
	*(cgu_base + cgu_clk_en) |= msk;
	*(cgu_base + cgu_clk_en) &= ~msk;
	*(cgu_base + cgu_core_reset) &= ~msk;
	*(cgu_base + cgu_clk_en) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_unlock) &= ~msk;
}


void _gr716_cguClkDisable(u32 cgu, u32 device)
{
	volatile u32 *cgu_base = (cgu == cgu_primary) ? gr716_common.cgu_base0 : gr716_common.cgu_base1;
	u32 msk = 1 << device;

	*(cgu_base + cgu_unlock) |= msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_clk_en) &= ~msk;
	hal_cpuDataStoreBarrier();
	*(cgu_base + cgu_unlock) &= ~msk;
}


void hal_wdgReload(void)
{
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;
	spinlock_ctx_t sc;
	int ret = -1;

	hal_spinlockSet(&gr716_common.pltctlSp, &sc);

	switch (data->type) {
		case pctl_cguctrl:
			if (data->action == pctl_set) {
				if (data->cguctrl.state == disable) {
					_gr716_cguClkDisable(data->cguctrl.cgu, data->cguctrl.cgudev);
				}
				else {
					_gr716_cguClkEnable(data->cguctrl.cgu, data->cguctrl.cgudev);
				}
				ret = 0;
			}
			break;

		case pctl_iocfg:
			if (data->action == pctl_set) {
				ret = _gr716_setIOCfg(data->iocfg.pin, data->iocfg.opt,
					data->iocfg.dir, data->iocfg.pullup, data->iocfg.pulldn);
			}
			else if (data->action == pctl_get) {
				ret = _gr716_getIOCfg(data->iocfg.pin, &data->iocfg.opt,
					&data->iocfg.dir, &data->iocfg.pullup, &data->iocfg.pulldn);
			}
			break;
	}
	hal_spinlockClear(&gr716_common.pltctlSp, &sc);

	return ret;
}


void _hal_platformInit(void)
{
	hal_spinlockCreate(&gr716_common.pltctlSp, "pltctl");

	gr716_common.GRGPIO_0 = GRGPIO0_BASE;
	gr716_common.GRGPIO_1 = GRGPIO1_BASE;
	gr716_common.grgpreg_base = GRGPREG_BASE;
	gr716_common.cgu_base0 = CGU_BASE0;
	gr716_common.cgu_base1 = CGU_BASE1;
}
