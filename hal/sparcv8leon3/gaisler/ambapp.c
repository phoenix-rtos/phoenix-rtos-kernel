/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * AMBA Plug'n'Play
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "hal/sparcv8leon3/sparcv8leon3.h"

#include "lib/lib.h"

#include "include/errno.h"


#define AMBAPP_AHB_MSTR ((addr_t)0xfffff000)
#define AMBAPP_AHB_SLV  ((addr_t)0xfffff800)

#define AMBAPP_APB_OFF ((addr_t)0xff000) /* PnP offset relative to APB bridge base address */

#define AMBAPP_AHB_NMASTERS 16
#define AMBAPP_AHB_NSLAVES  16
#define AMBAPP_APB_NSLAVES  16

#define AMBAPP_VEN(id)  ((id) >> 24)
#define AMBAPP_DEV(id)  (((id) >> 12) & 0xfff)
#define AMBAPP_VER(id)  (((id) >> 5) & 0x1f)
#define AMBAPP_IRQN(id) ((id) & 0x1f)

#define AMBAPP_AHB_ADDR(bar)           (((bar) & 0xfff00000u) & (((bar) & 0xfff0u) << 16))
#define AMBAPP_AHBIO_ADDR(ioarea, bar) ((ioarea) | ((bar) >> 12))
#define AMBAPP_APB_ADDR(base, bar)     ((base) | ((((bar) & 0xfff00000u) >> 12) & (((bar) & 0xfff0u) << 4)))
#define AMBAPP_TYPE(bar)               ((bar) & 0xfu)


typedef struct {
	u32 id;
	u32 bar;
} ambapp_apb_dev_t;


typedef struct {
	u32 id;
	u32 reserved[3];
	u32 bar[4];
} ambapp_ahb_dev_t;


static void ambapp_fillApbDev(addr_t apb, ambapp_dev_t *dev, ambapp_apb_dev_t *apbdev)
{
	u32 info = hal_cpuLoadPaddr(&apbdev->id);

	dev->vendor = AMBAPP_VEN(info);
	dev->irqn = AMBAPP_IRQN(info);
	dev->bus = BUS_AMBA_APB;

	info = hal_cpuLoadPaddr(&apbdev->bar);
	dev->info.apb.base = (u32 *)AMBAPP_APB_ADDR(apb, info);
	dev->info.apb.type = AMBAPP_TYPE(info);
}


static int ambapp_apbFind(addr_t apb, ambapp_dev_t *dev, unsigned int *instance)
{
	u32 i, id;
	ambapp_apb_dev_t *apbdev = (void *)(apb + AMBAPP_APB_OFF);
	for (i = 0; i < AMBAPP_APB_NSLAVES; i++) {
		id = hal_cpuLoadPaddr(&apbdev[i].id);
		if (AMBAPP_DEV(id) == dev->devId) {
			(*instance)--;
			if (*instance == -1) {
				/* Found desired device, fill struct and return */
				ambapp_fillApbDev(apb, dev, &apbdev[i]);
				return 0;
			}
		}
	}

	return -1;
}


static void ambapp_fillAhbDev(ambapp_dev_t *dev, ambapp_ahb_dev_t *ahbdev)
{
	u8 bar;
	addr_t addr;
	u32 info = hal_cpuLoadPaddr(&ahbdev->id);

	dev->vendor = AMBAPP_VEN(info);
	dev->irqn = AMBAPP_IRQN(info);
	dev->bus = BUS_AMBA_AHB;

	for (bar = 0; bar < 4; bar++) {
		info = hal_cpuLoadPaddr(&ahbdev->bar[bar]);

		if (info == 0) {
			dev->info.ahb.base[bar] = 0;
			dev->info.ahb.type[bar] = 0;
		}
		else {
			addr = AMBAPP_AHB_ADDR(info);
			if (AMBAPP_TYPE(info) == AMBA_TYPE_AHBIO) {
				dev->info.ahb.base[bar] = (u32 *)AMBAPP_AHBIO_ADDR(LEON3_IOAREA, addr);
			}
			else {
				dev->info.ahb.base[bar] = (u32 *)AMBAPP_AHB_ADDR(info);
			}
			dev->info.ahb.type[bar] = AMBAPP_TYPE(info);
		}
	}
}


static void ambapp_addBridge(addr_t *bridges, size_t len, addr_t addr)
{
	size_t i, j;
	addr_t curr;
	for (i = 0; i < len; i++) {
		curr = bridges[i];
		if (curr == 0xffffffffu) {
			bridges[i] = addr;
			return;
		}
		if (addr < curr) {
			for (j = len - 1; j > i; j--) {
				bridges[j] = bridges[j - 1];
			}
			bridges[i] = addr;
			return;
		}
	}
}


static int ambapp_ahbFind(addr_t pnp, u32 ndevs, ambapp_dev_t *dev, unsigned int *instance)
{
	u32 i, id, bar, val;
	addr_t apb;
	ambapp_ahb_dev_t *ahbdev = (ambapp_ahb_dev_t *)pnp;
	addr_t apbBridges[AMBAPP_AHB_NSLAVES];
	hal_memset(apbBridges, 0xff, sizeof(apbBridges));

	for (i = 0; i < ndevs; i++) {
		/* Scan AHB PnP */
		id = hal_cpuLoadPaddr(&ahbdev[i].id);

		if (AMBAPP_DEV(id) == dev->devId) {
			/* Found desired device in AHB */
			(*instance)--;
			if (*instance == -1) {
				ambapp_fillAhbDev(dev, &ahbdev[i]);
				return 0;
			}
		}

		else if (AMBAPP_DEV(id) == CORE_ID_APBCTRL) {
			/* Found APB Bridge */
			for (bar = 0; bar < 4; bar++) {
				val = hal_cpuLoadPaddr(&ahbdev[i].bar[bar]);
				if (AMBAPP_TYPE(val) == AMBA_TYPE_AHBMEM) {
					apb = AMBAPP_AHB_ADDR(val);
					ambapp_addBridge(apbBridges, AMBAPP_AHB_NSLAVES, apb);
				}
			}
		}
	}

	for (i = 0; i < AMBAPP_AHB_NSLAVES; i++) {
		/* Scan APB PnP */
		if (apbBridges[i] == 0xffffffffu) {
			break;
		}
		if (ambapp_apbFind(apbBridges[i], dev, instance) == 0) {
			return 0;
		}
	}

	return -1;
}


int ambapp_findMaster(ambapp_dev_t *dev, unsigned int *instance)
{
	if (ambapp_ahbFind(AMBAPP_AHB_MSTR, AMBAPP_AHB_NMASTERS, dev, instance) < 0) {
		return -ENODEV;
	}
	return EOK;
}


int ambapp_findSlave(ambapp_dev_t *dev, unsigned int *instance)
{
	if (ambapp_ahbFind(AMBAPP_AHB_SLV, AMBAPP_AHB_NSLAVES, dev, instance) < 0) {
		return -ENODEV;
	}
	return EOK;
}
