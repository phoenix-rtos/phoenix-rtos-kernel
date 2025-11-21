/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * PCI driver
 *
 * Copyright 2019, 2020, 2024 Phoenix Systems
 * Author: Kamil Amanowicz, Lukasz Kosinski, Adam Greloch
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PCI_H_
#define _PH_HAL_PCI_H_

#include "include/arch/ia32/ia32.h"


int hal_pciSetUsbOwnership(pci_usbownership_t *usbownership);


int hal_pciGetDevice(pci_id_t *id, pci_dev_t *dev, void *caps);


int hal_pciSetConfigOption(pci_pcicfg_t *pcicfg);


void _hal_pciInit(void);


int _hal_pciSetCmdRegBit(pci_dev_t *dev, u8 bit, u8 enable);


#endif
