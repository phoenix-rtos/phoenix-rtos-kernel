/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * PCI driver
 *
 * Copyright 2018, 2019, 2020, 2024 Phoenix Systems
 * Author: Aleksander Kaminski, Kamil Amanowicz, Lukasz Kosinski, Adam Greloch
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "pci.h"
#include "ia32.h"


static struct {
	spinlock_t spinlock;
} pci_common;


/* Reads word from PCI configuration space */
static u32 _hal_pciGet(u8 bus, u8 dev, u8 func, u8 reg)
{
	hal_outl(0xcf8U, 0x80000000U | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)func << 8) | ((u32)reg << 2));
	return hal_inl(0xcfcU);
}


/* Writes word to PCI configuration space */
static void _hal_pciSet(u8 bus, u8 dev, u8 func, u8 reg, u32 val)
{
	hal_outl(0xcf8U, 0x80000000U | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)func << 8) | ((u32)reg << 2));
	hal_outl(0xcfcU, val);
}


/* Reads PCI capability list */
static int _hal_pciGetCaps(pci_dev_t *dev, void *caps)
{
	pci_cap_t *cap = (pci_cap_t *)caps;
	u32 *data = (u32 *)cap;
	u8 offs, len;

	/* Check if device uses capability list */
	if ((dev->status & (1U << 4)) == 0U) {
		return EOK;
	}

	/* Get capability list head offset */
	offs = (u8)_hal_pciGet(dev->bus, dev->dev, dev->func, 0xdU) & 0xffU;

	/* Read capability list */
	do {
		if (offs < 64U || (offs % 4U) != 0U) {
			return -EFAULT;
		}

		/* Get capability header */
		offs /= 4U;
		*data++ = _hal_pciGet(dev->bus, dev->dev, dev->func, offs++);

		/* Get capability length */
		len = (cap->len >= 4U) ? cap->len - 4U : 0U;
		if ((len % 4U) != 0U) {
			len = (len + 3U) & (u8)~3U;
		}

		/* Get capability data */
		while (len != 0U) {
			*data++ = _hal_pciGet(dev->bus, dev->dev, dev->func, offs++);
			len -= 4U;
		}

		offs = cap->next;
		cap->next = (unsigned char)((ptr_t)data - (ptr_t)caps) + cap->len;
		cap = (pci_cap_t *)((u8 *)data + cap->len);
		data = (u32 *)cap;
	} while (offs != 0U);

	return EOK;
}


/* Sets a bit in PCI configuration command register */
int _hal_pciSetCmdRegBit(pci_dev_t *dev, u8 bit, u8 enable)
{
	spinlock_ctx_t sc;
	u32 dv;

	if (dev == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&pci_common.spinlock, &sc);
	dv = _hal_pciGet(dev->bus, dev->dev, dev->func, 1U);
	if (enable != 0U) {
		dv |= (1UL << bit);
	}
	else {
		dv &= ~(1UL << bit);
	}
	_hal_pciSet(dev->bus, dev->dev, dev->func, 1U, dv);
	hal_spinlockClear(&pci_common.spinlock, &sc);

	dev->command = (unsigned short)dv & 0xffffU;

	return EOK;
}


int hal_pciSetUsbOwnership(pci_usbownership_t *usbownership)
{
	spinlock_ctx_t sc;
	u32 dv;
	pci_dev_t *dev = &usbownership->dev;
	u8 osOwned = (u8)usbownership->osOwned;
	u8 reg = (u8)(usbownership->eecp) >> 2U; /* eecp is a pci config offset */

	hal_spinlockSet(&pci_common.spinlock, &sc);
	dv = _hal_pciGet(dev->bus, dev->dev, dev->func, reg);

	/* set HC OS Owned Semaphore */
	if (osOwned != 0U) {
		dv |= (1UL << 24);
	}
	else {
		dv &= ~(1UL << 24);
	}
	_hal_pciSet(dev->bus, dev->dev, dev->func, reg, dv);

	for (;;) {
		dv = _hal_pciGet(dev->bus, dev->dev, dev->func, reg);

		/*
		 * When transferring ownership we need to wait until HC OS Owned Semaphore (bit 24)
		 * and HC BIOS Owned Semaphore (bit 16) get set appropriately (EHCI Spec, 2.1.7)
		 */

		/* OS took over when HC OS Owned is 1, HC BIOS Owned is 0 */
		if ((osOwned != 0U) && ((dv & (1UL << 24)) != 0U) && ((dv & (1UL << 16)) == 0U)) {
			break;
		}

		/* BIOS took over when HC OS Owned is 0, HC BIOS Owned is 1 */
		if ((osOwned == 0U) && ((dv & (1UL << 24)) == 0U) && ((dv & (1UL << 16)) != 0U)) {
			break;
		}
	}
	hal_spinlockClear(&pci_common.spinlock, &sc);

	dev->command = (unsigned short)dv & 0xffffU;

	return EOK;
}


int hal_pciSetConfigOption(pci_pcicfg_t *pcicfg)
{
	pci_dev_t *dev = &pcicfg->dev;
	u8 enable = (u8)pcicfg->enable;

	switch (pcicfg->cfg) {
		case pci_cfg_interruptdisable:
			return _hal_pciSetCmdRegBit(dev, 10U, enable);
		case pci_cfg_memoryspace:
			return _hal_pciSetCmdRegBit(dev, 1U, enable);
		case pci_cfg_busmaster:
			return _hal_pciSetCmdRegBit(dev, 2U, enable);
		default:
			return -EINVAL;
	}
}


int hal_pciGetDevice(pci_id_t *id, pci_dev_t *dev, void *caps)
{
	spinlock_ctx_t sc;
	unsigned char b, d, f, i;
	u32 val0, cl, progif, mask, valB, val2;
	int res = EOK;

	if ((id == NULL) || (dev == NULL)) {
		return -EINVAL;
	}

	b = dev->bus;

	for (;;) {
		for (d = dev->dev; d < 32U; d++) {
			for (f = dev->func; f < 8U; f++) {
				hal_spinlockSet(&pci_common.spinlock, &sc);

				do {
					val0 = _hal_pciGet(b, d, f, 0);
					if (val0 == 0xffffffffU) {
						break;
					}

					if ((id->vendor != PCI_ANY) && (id->vendor != (val0 & 0xffffU))) {
						break;
					}

					if ((id->device != PCI_ANY) && (id->device != (val0 >> 16))) {
						break;
					}

					val2 = _hal_pciGet(b, d, f, 0x2U);

					cl = val2 >> 16;
					progif = (val2 >> 8) & 0xffU;

					if ((id->cl != PCI_ANY) && (id->cl != cl)) {
						break;
					}

					if ((id->progif != PCI_ANY) && (id->progif != progif)) {
						break;
					}

					valB = _hal_pciGet(b, d, f, 0xbU);

					if ((id->subdevice != PCI_ANY) && (id->subdevice != (valB >> 16))) {
						break;
					}

					if ((id->subvendor != PCI_ANY) && (id->subvendor != (valB & 0xffffU))) {
						break;
					}

					dev->bus = b;
					dev->dev = d;
					dev->func = f;
					dev->vendor = (unsigned short)val0 & 0xffffU;
					dev->device = (unsigned short)(val0 >> 16);
					dev->cl = (unsigned short)cl;
					dev->subvendor = (unsigned short)(valB & 0xffffU);
					dev->subdevice = (unsigned short)(valB >> 16);

					val0 = _hal_pciGet(b, d, f, 0x1);
					dev->status = (unsigned short)(val0 >> 16);
					dev->command = (unsigned short)(val0 & 0xffffU);

					dev->progif = (unsigned char)progif;
					dev->revision = (unsigned char)(val2 & 0xffU);
					dev->type = (unsigned char)((_hal_pciGet(b, d, f, 0x3) >> 16) & 0xffU);
					dev->irq = (unsigned char)(_hal_pciGet(b, d, f, 0xf) & 0xffU);

					/* Get resources */
					for (i = 0; i < 6U; i++) {
						dev->resources[i].base = _hal_pciGet(b, d, f, 0x4U + i);

						/* Get resource flags and size */
						_hal_pciSet(b, d, f, 0x4U + i, 0xffffffffU);
						dev->resources[i].limit = _hal_pciGet(b, d, f, 0x4U + i);
						mask = (dev->resources[i].base & 0x1U) != 0U ? ~0x3U : ~0xfU;
						dev->resources[i].limit = (~(dev->resources[i].limit & mask)) + 1U;
						_hal_pciSet(b, d, f, 0x4U + i, dev->resources[i].base);
						dev->resources[i].flags = (unsigned char)(dev->resources[i].base & ~mask);
						dev->resources[i].base &= mask;
					}

					if (caps != NULL) {
						res = _hal_pciGetCaps(dev, caps);
					}

					hal_spinlockClear(&pci_common.spinlock, &sc);

					return res;
				} while (0);

				hal_spinlockClear(&pci_common.spinlock, &sc);
			}
			dev->func = 0;
		}
		dev->dev = 0;

		if (b == 0xffU) {
			break;
		}
		b++;
	}

	return -ENODEV;
}


void _hal_pciInit(void)
{
	hal_spinlockCreate(&pci_common.spinlock, "pci_common.spinlock");
}
