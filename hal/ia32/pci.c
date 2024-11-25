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


struct {
	spinlock_t spinlock;
} pci_common;


/* Reads word from PCI configuration space */
static u32 _hal_pciGet(u8 bus, u8 dev, u8 func, u8 reg)
{
	hal_outl((u16)0xcf8, 0x80000000 | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)func << 8) | (reg << 2));
	return hal_inl((u16)0xcfc);
}


/* Writes word to PCI configuration space */
static void _hal_pciSet(u8 bus, u8 dev, u8 func, u8 reg, u32 val)
{
	hal_outl((u16)0xcf8, 0x80000000 | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)func << 8) | (reg << 2));
	hal_outl((u16)0xcfc, val);
}


/* Reads PCI capability list */
static int _hal_pciGetCaps(pci_dev_t *dev, void *caps)
{
	pci_cap_t *cap = (pci_cap_t *)caps;
	u32 *data = (u32 *)cap;
	u8 offs, len;

	/* Check if device uses capability list */
	if (!(dev->status & (1 << 4)))
		return EOK;

	/* Get capability list head offset */
	offs = _hal_pciGet(dev->bus, dev->dev, dev->func, 0xd) & 0xff;

	/* Read capability list */
	do {
		if ((offs < 64) || (offs % 4))
			return -EFAULT;

		/* Get capability header */
		offs /= 4;
		*data++ = _hal_pciGet(dev->bus, dev->dev, dev->func, offs++);

		/* Get capability length */
		if ((len = (cap->len >= 4) ? cap->len - 4 : 0) % 4)
			len = (len + 3) & ~3;

		/* Get capability data */
		while (len) {
			*data++ = _hal_pciGet(dev->bus, dev->dev, dev->func, offs++);
			len -= 4;
		}

		offs = cap->next;
		cap->next = (unsigned char)((u8 *)data - (u8 *)caps) + cap->len;
		cap = (pci_cap_t *)((u8 *)data + cap->len);
		data = (u32 *)cap;
	} while (offs);

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
	dv = _hal_pciGet(dev->bus, dev->dev, dev->func, 1);
	if (enable != 0) {
		dv |= (1 << bit);
	}
	else {
		dv &= ~(1 << bit);
	}
	_hal_pciSet(dev->bus, dev->dev, dev->func, 1, dv);
	hal_spinlockClear(&pci_common.spinlock, &sc);

	dev->command = dv & 0xffff;

	return EOK;
}


int hal_pciSetUsbOwnership(pci_usbownership_t *usbownership)
{
	spinlock_ctx_t sc;
	u32 dv;
	pci_dev_t *dev = &usbownership->dev;
	u8 osOwned = usbownership->osOwned;
	u8 reg = (usbownership->eecp) >> 2U; /* eecp is a pci config offset */

	if (dev == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&pci_common.spinlock, &sc);
	dv = _hal_pciGet(dev->bus, dev->dev, dev->func, reg);

	/* set HC OS Owned Semaphore */
	if (osOwned != 0) {
		dv |= (1 << 24);
	}
	else {
		dv &= ~(1 << 24);
	}
	_hal_pciSet(dev->bus, dev->dev, dev->func, reg, dv);

	for (;;) {
		dv = _hal_pciGet(dev->bus, dev->dev, dev->func, reg);

		/*
		 * When transferring ownership we need to wait until HC OS Owned Semaphore (bit 24)
		 * and HC BIOS Owned Semaphore (bit 16) get set appropriately (EHCI Spec, 2.1.7)
		 */

		/* OS took over when HC OS Owned is 1, HC BIOS Owned is 0 */
		if ((osOwned != 0) && ((dv & (1 << 24)) != 0) && ((dv & (1 << 16)) == 0)) {
			break;
		}

		/* BIOS took over when HC OS Owned is 0, HC BIOS Owned is 1 */
		if ((osOwned == 0) && ((dv & (1 << 24)) == 0) && ((dv & (1 << 16)) != 0)) {
			break;
		}
	}
	hal_spinlockClear(&pci_common.spinlock, &sc);

	dev->command = dv & 0xffff;

	return EOK;
}


int hal_pciSetConfigOption(pci_pcicfg_t *pcicfg)
{
	pci_dev_t *dev = &pcicfg->dev;
	u8 enable = pcicfg->enable;

	switch (pcicfg->cfg) {
		case pci_cfg_interruptdisable:
			return _hal_pciSetCmdRegBit(dev, 10, enable);
		case pci_cfg_memoryspace:
			return _hal_pciSetCmdRegBit(dev, 1, enable);
		case pci_cfg_busmaster:
			return _hal_pciSetCmdRegBit(dev, 2, enable);
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

	if ((id == NULL) || (dev == NULL))
		return -EINVAL;

	for (b = dev->bus;; b++) {
		for (d = dev->dev; d < 32; d++) {
			for (f = dev->func; f < 8; f++) {
				hal_spinlockSet(&pci_common.spinlock, &sc);

				do {
					if ((val0 = _hal_pciGet(b, d, f, 0)) == 0xffffffff)
						break;

					if ((id->vendor != PCI_ANY) && (id->vendor != (val0 & 0xffff)))
						break;

					if ((id->device != PCI_ANY) && (id->device != (val0 >> 16)))
						break;

					val2 = _hal_pciGet(b, d, f, 0x2);

					cl = val2 >> 16;
					progif = (val2 >> 8) & 0xff;

					if ((id->cl != PCI_ANY) && (id->cl != cl))
						break;

					if ((id->progif != PCI_ANY) && (id->progif != progif))
						break;

					valB = _hal_pciGet(b, d, f, 0xb);

					if ((id->subdevice != PCI_ANY) && (id->subdevice != (valB >> 16)))
						break;

					if ((id->subvendor != PCI_ANY) && (id->subvendor != (valB & 0xffff)))
						break;

					dev->bus = b;
					dev->dev = d;
					dev->func = f;
					dev->vendor = val0 & 0xffff;
					dev->device = val0 >> 16;
					dev->cl = cl;
					dev->subvendor = valB & 0xffff;
					dev->subdevice = valB >> 16;

					val0 = _hal_pciGet(b, d, f, 0x1);
					dev->status = val0 >> 16;
					dev->command = val0 & 0xffff;

					dev->progif = progif;
					dev->revision = val2 & 0xff;
					dev->type = (_hal_pciGet(b, d, f, 0x3) >> 16) & 0xff;
					dev->irq = _hal_pciGet(b, d, f, 0xf) & 0xff;

					/* Get resources */
					for (i = 0; i < 6; i++) {
						dev->resources[i].base = _hal_pciGet(b, d, f, 0x4 + i);

						/* Get resource flags and size */
						_hal_pciSet(b, d, f, 0x4 + i, 0xffffffff);
						dev->resources[i].limit = _hal_pciGet(b, d, f, 0x4 + i);
						mask = (dev->resources[i].base & 0x1) ? ~0x3 : ~0xf;
						dev->resources[i].limit = (~(dev->resources[i].limit & mask)) + 1;
						_hal_pciSet(b, d, f, 0x4 + i, dev->resources[i].base);
						dev->resources[i].flags = dev->resources[i].base & ~mask;
						dev->resources[i].base &= mask;
					}

					if (caps != NULL)
						res = _hal_pciGetCaps(dev, caps);

					hal_spinlockClear(&pci_common.spinlock, &sc);

					return res;
				} while (0);

				hal_spinlockClear(&pci_common.spinlock, &sc);
			}
			dev->func = 0;
		}
		dev->dev = 0;

		if (b == 0xff)
			break;
	}

	return -ENODEV;
}


void _hal_pciInit(void)
{
	hal_spinlockCreate(&pci_common.spinlock, "pci_common.spinlock");
}
