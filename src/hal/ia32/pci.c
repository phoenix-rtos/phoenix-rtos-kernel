/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * PCI driver
 *
 * Copyright 2018, 2019 Phoenix Systems
 * Author: Aleksander Kaminski, Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#include "../../../include/errno.h"

#include "spinlock.h"
#include "syspage.h"
#include "string.h"
#include "pmap.h"
#include "spinlock.h"
#include "cpu.h"
#include "pci.h"

extern struct {
	tss_t tss;
	u32 dr5;

	spinlock_t lock;
} cpu;


/* Function reads word from PCI configuration space */
static u32 _hal_pciGet(u8 bus, u8 dev, u8 func, u8 reg)
{
	hal_outl((void *)0xcf8, 0x80000000 | ((u32)bus << 16 ) | ((u32)dev << 11) | ((u32)func << 8) | (reg << 2));
	return hal_inl((void *)0xcfc);
}


/* Function reads word from PCI configuration space */
static u32 hal_pciGet(u8 bus, u8 dev, u8 func, u8 reg)
{
	u32 v;

	hal_spinlockSet(&cpu.lock);
	v = _hal_pciGet(bus, dev, func, reg);
	hal_spinlockClear(&cpu.lock);

	return v;
}


/* Function writes word to PCI configuration space */
static u32 _hal_pciSet(u8 bus, u8 dev, u8 func, u8 reg, u32 v)
{
	hal_outl((void *)0xcf8, 0x80000000 | ((u32)bus << 16 ) | ((u32)dev << 11) | ((u32)func << 8) | (reg << 2));
	hal_outl((void *)0xcfc, v);

	return v;
}


int _hal_pciGetCap(pci_device_t *dev, u32 *data, u8 offset)
{
	u8 off, len;
	pci_cap_t *cap = (void *)data;

	/* anything below 64 is invalid */
	if (offset < 64)
		return -1;

	/* 4 byte alignment */
	if (offset % 4)
		return -1;

	off = offset / 4;

	/* get pci cap generic fields and get cap lenght*/
	*data++ = _hal_pciGet(dev->bus, dev->device, dev->function, off++);

	if (cap->len >= 4)
		len = cap->len - 4;
	else
		len = 0;

	/* pad it to 4 bytes */
	if (len % 4)
		len = (len + 3) & ~3;

	/* read the rest of the cap */
	while (len) {
		*data++ = _hal_pciGet(dev->bus, dev->device, dev->function, off++);
		len -= 4;
	}

	return 0;
}


int _hal_pciGetCapList(pci_device_t *dev, pci_cap_list_t *cap_list)
{
	pci_cap_t *cap;
	unsigned char *data;
	u8 off;

	/* check whether device uses capability list at all */
	if (!(dev->status & (1 << 4)))
		return EOK;

	/* get first cap from list */
	cap = (pci_cap_t *)&cap_list->data[0];
	_hal_pciGetCap(dev, (u32 *)cap, dev->cap_ptr);
	data = &cap_list->data[0];

	/* get the rest of the list */
	while (cap->next) {
		off = cap->next;
		cap->next = (unsigned char)((int)cap - (int)&cap_list->data[0]) + cap->len;
		cap = (pci_cap_t *)&data[cap->len];
		if (_hal_pciGetCap(dev, (u32 *)cap, off))
			return -ENXIO;
		data = (unsigned char *)cap;
	}

	return EOK;
}


int hal_pciSetBusmaster(pci_device_t *dev, u8 enable)
{
	u32 dv;

	if (dev == NULL)
		return -EINVAL;

	hal_spinlockSet(&cpu.lock);
	dv = _hal_pciGet(dev->bus, dev->device, dev->function, 1);
	dv &= ~(!enable << 2);
	dv |= !!enable << 2;
	_hal_pciSet(dev->bus, dev->device, dev->function, 1, dv);
	hal_spinlockClear(&cpu.lock);

	dev->command = dv & 0xffff;

	return EOK;
}


int hal_pciGetDevice(pci_id_t *id, pci_device_t *dev, pci_cap_list_t *cap_list)
{
	unsigned int b, d, f, i;
	u32 dv, cl, mask, val;
	int res = EOK;

	if (id == NULL || dev == NULL)
		return -EINVAL;

	for (b = dev->bus; b < 256; b++) {
		for (d = dev->device; d < 32; d++) {
			for (f = dev->function; f < 8; f++) {
				dv = hal_pciGet(b, d, f, 0);

				if (dv == 0xffffffff) {
					/* since function 0 is manadatory no more checks are necessary for this device */
					f = 8;
					continue;
				}

				/* get header type */
				val = ((hal_pciGet(b, d, f, 3) >> 16) & 0xff);

				/* check multi-function if it's f0 */
				if (!f && !(val & 0x80))
					f = 8;

				/* check for bridge - since we scan all buses anyway we can just ignore it */
				if (val & 0x7f)
					continue;

				/* check whether this is device we look for */
				if (id->vendor != PCI_ANY && id->vendor != (dv & 0xffff))
					continue;

				if (id->device != PCI_ANY && id->device != (dv >> 16))
					continue;

				cl = hal_pciGet(b, d, f, 2) >> 16;

				if (id->class_code != PCI_ANY && id->class_code != cl)
					continue;

				val = hal_pciGet(b, d, f, 0xb);

				if (id->subdevice != PCI_ANY && id->subdevice != ((val >> 16) & 0xffff))
					continue;

				if (id->subvendor != PCI_ANY && id->subvendor != (val & 0xffff))
					continue;

				/* everything checks out so fill dev structure */
				dev->bus = b;
				dev->device = d;
				dev->function = f;
				dev->vendor_id = dv & 0xffff;
				dev->device_id = dv >> 16;
				dev->class_code = cl;
				dev->subsystem_vendor_id = val & 0xffff;
				dev->subsystem_id = (val >> 16) & 0xffff;

				hal_spinlockSet(&cpu.lock);
				dv = _hal_pciGet(b, d, f, 1);
				dev->status = dv >> 16;
				dev->command = dv & 0xffff;

				val = _hal_pciGet(b, d, f, 2);
				dev->progif = (val >> 8) & 0xff;
				dev->revision = val & 0xff;

				val = _hal_pciGet(b, d, f, 2);
				dev->cache_line_size = val & 0xff;
				dev->latency_tmr = (val >> 8) & 0xff;
				dev->type = (val >> 16) & 0xff;
				dev->bist = (val >> 24) & 0xff;

				dev->cis_ptr = _hal_pciGet(b, d, f, 0xa);

				dev->exp_rom_addr = _hal_pciGet(b, d, f, 0xc);

				val = _hal_pciGet(b, d, f, 0xd);
				dev->cap_ptr = val & 0xff;

				val = _hal_pciGet(b, d, f, 0xf);
				dev->irq = val & 0xff;
				dev->irq_pin = (val >> 8) & 0xff;
				dev->min_grant = (val >> 16) & 0xff;
				dev->max_latency = (val >> 24) & 0xff;

				/* Get resources - base address registers */
				for (i = 0; i < 6; i++) {
					dev->resources[i].base = _hal_pciGet(b, d, f, 4 + i);

					/* Get resource limit/size */
					_hal_pciSet(b, d, f, 4 + i, 0xffffffff);
					dev->resources[i].limit = _hal_pciGet(b, d, f, 4 + i);
					mask = ~0xf;
					if (dev->resources[i].base & 0x1)
						mask = ~0x3;

					dev->resources[i].limit = (~(dev->resources[i].limit & mask)) + 1;
					_hal_pciSet(b, d, f, 4 + i, dev->resources[i].base);
					dev->resources[i].flags = dev->resources[i].base & ~mask;
					dev->resources[i].base &= mask;
				}

				if (cap_list)
					res = _hal_pciGetCapList(dev, cap_list);

				hal_spinlockClear(&cpu.lock);

				return res;
			}
		}
	}

	return -ENODEV;
}