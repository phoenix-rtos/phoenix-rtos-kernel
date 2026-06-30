/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Tests for DDR RAM
 *
 * Copyright 2015 Phoenix Systems
 * Author: Jakub Sejdak
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TEST_DDR_H_
#define _TEST_DDR_H_

int test_ddrAccessibility(uint32_t address, int size);

int test_ddrBitCrossTalk(uint32_t address, int size);

int test_ddrBitChargeLeakage(uint32_t address, int size);

int test_ddrFullMemtest(uint32_t address, int size, int iterations);

#endif
