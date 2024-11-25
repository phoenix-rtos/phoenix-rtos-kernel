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

#ifndef _HAL_PCI_H_
#define _HAL_PCI_H_

#include "include/arch/ia32/ia32.h"


extern int hal_pciSetUsbOwnership(pci_usbownership_t *usbownership);


extern int hal_pciGetDevice(pci_id_t *id, pci_dev_t *dev, void *caps);


extern int hal_pciSetConfigOption(pci_pcicfg_t *pcicfg);


extern void _hal_pciInit(void);


#endif
