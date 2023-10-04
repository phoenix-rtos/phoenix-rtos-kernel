/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * PCI driver
 *
 * Copyright 2019, 2020 Phoenix Systems
 * Author: Kamil Amanowicz, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PCI_H_
#define _HAL_PCI_H_

#include "include/arch/ia32.h"


extern int hal_pciSetBusmaster(pci_dev_t *dev, u8 enable);


extern int hal_pciGetDevice(pci_id_t *id, pci_dev_t *dev, void *caps);


extern void _hal_pciInit(void);


#endif
