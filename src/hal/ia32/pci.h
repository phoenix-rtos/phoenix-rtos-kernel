/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * PCI driver
 *
 * Copyright 2019 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PCI_H_
#define _HAL_PCI_H_

#include "../../../include/arch/ia32.h"


int hal_pciSetBusmaster(pci_device_t *dev, u8 enable);


int hal_pciGetDevice(pci_id_t *id, pci_device_t *dev, pci_cap_list_t *cap_list);


#endif /* _HAL_PCI_H_ */