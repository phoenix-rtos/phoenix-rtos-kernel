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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <lib/stdint.h>

static int __attribute__ ((section (".boot"))) test_ddrByteAccessibility(uint32_t address, int size)
{
	uint8_t* ddr_ram = (uint8_t*) address;
	uint32_t memory_test_bytes = size;
	uint32_t i;
	uint8_t write_value, read_value;
	int errors_num = 0;

	/* write_value overflow here is intentional. */
	for (i = 0, write_value = 0; i < memory_test_bytes; ++i, ++write_value)
		ddr_ram[i] = write_value;

	for (i = 0, write_value = 0; i < memory_test_bytes; ++i, ++write_value) {
		read_value = ddr_ram[i];
		if(read_value != write_value)
			++errors_num;
	}

	return errors_num;
}

static int __attribute__ ((section (".boot"))) test_ddrWordAccessibility(uint32_t address, int size)
{
	uint32_t* ddr_ram = (uint32_t*) address;
	uint32_t memory_test_words = size / 4;
	uint32_t i, write_value, read_value;
	int errors_num = 0;

	for (i = 0; i < memory_test_words; ++i) {
		write_value = i * 4;
		ddr_ram[i] = write_value;
	}

	for (i = 0; i < memory_test_words; ++i) {
		write_value = i * 4;
		read_value = ddr_ram[i];
		if(read_value != write_value)
			++errors_num;
	}

	return errors_num;
}

int __attribute__ ((section (".boot"))) test_ddrAccessibility(uint32_t address, int size)
{
	int byteErrors = test_ddrByteAccessibility(address, size);
	int wordErrors = test_ddrWordAccessibility(address, size);

	return byteErrors + wordErrors;
}

#define BANK_COUNT				8
#define BANK_SELECT_MASK		0x3800
#define BANK_SELECT_SHIFT		11
#define BANK_SET(x)				(((uint32_t)(((uint32_t)(x)) << BANK_SELECT_SHIFT)) & BANK_SELECT_MASK)
#define BANK_GET(x)				(((uint32_t)(((uint32_t)(x)) & BANK_SELECT_MASK)) >> BANK_SELECT_SHIFT)

#define COLUMN_COUNT			128
#define COLUMN_CHUNK_COUNT      8
#define COLUMN_SELECT_MASK		0x7fe
#define COLUMN_SELECT_SHIFT		1
#define COLUMN_SET(x)			(((uint32_t)(((uint32_t)(x)) << COLUMN_SELECT_SHIFT)) & COLUMN_SELECT_MASK)
#define COLUMN_GET(x)			(((uint32_t)(((uint32_t)(x)) & COLUMN_SELECT_MASK)) >> COLUMN_SELECT_SHIFT)

#define ROW_COUNT				8192
#define ROW_SELECT_MASK			0x7ffc000
#define ROW_SELECT_SHIFT		14
#define ROW_SET(x)				(((uint32_t)(((uint32_t)(x)) << ROW_SELECT_SHIFT)) & ROW_SELECT_MASK)
#define ROW_GET(x)				(((uint32_t)(((uint32_t)(x)) & ROW_SELECT_MASK)) >> ROW_SELECT_SHIFT)

/* DDR3 RAM Addressing layout (Freescale Tower & romek1)
 * Physical address:
 *                   0|000 0000 0000 00|00 0|000 0000 0000 000|0
 *        chip select |       row      |bank|      column     | data path
 *            1b      |       13b      | 3b |       10b       | 1b
 */

#define TEST_PATTERN_COUNT	30
#define TEST_PATTERN_0		0x5555
#define TEST_PATTERN_1		0x3333
#define TEST_PATTERN_2		0x0f0f
#define TEST_PATTERN_3		0x00ff

uint16_t __attribute__ ((section (".boot"))) generateTestVector(int pattern, int column)
{
	if(pattern == 0)	return TEST_PATTERN_0;
	if(pattern == 1)	return ~TEST_PATTERN_0;
	if(pattern == 2)	return TEST_PATTERN_1;
	if(pattern == 3)	return ~TEST_PATTERN_1;
	if(pattern == 4)	return TEST_PATTERN_2;
	if(pattern == 5)	return ~TEST_PATTERN_2;
	if(pattern == 6)	return TEST_PATTERN_3;
	if(pattern == 7)	return ~TEST_PATTERN_3;
	else {
		/* Starting from pattern 8 we have to change vector for each column. */
		int flip_shift = (pattern - 8) >> 1;
		uint16_t vector = 0x0;
		int flip;
		if(pattern & 0x1)
			vector = ~vector;

		if(flip_shift == 0) {
			if(column & 0x1)
				vector = ~vector;
		}
		else {
			flip = column >> flip_shift;
			if(flip & 0x1)
				vector = ~vector;
		}

		return vector;
	}

	return 0;
}

int __attribute__ ((section (".boot"))) test_ddrBitCrossTalk(uint32_t address, int size)
{
	int bank, row, column, pattern;
	uint16_t *addr;
	uint16_t write_value, read_value;
	int errors_num = 0;

	for(bank = 0; bank < BANK_COUNT; ++bank) {
		for(pattern = 0; pattern < TEST_PATTERN_COUNT; ++pattern) {
			/* Write test pattern vectors. */
			for(row = 0; row < ROW_COUNT; ++row) {
				for(column = 0; column < COLUMN_COUNT * COLUMN_CHUNK_COUNT; ++column) {
					write_value = generateTestVector(pattern, column);
					addr = (uint16_t*) (0x80000000 | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
					addr[0] = write_value;
				}
			}

			/* Read and compare test vectors. */
			for(row = 0; row < ROW_COUNT; ++row) {
				for(column = 0; column < COLUMN_COUNT * COLUMN_CHUNK_COUNT; ++column) {
					write_value = generateTestVector(pattern, column);
					addr = (uint16_t*) (0x80000000 | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
					read_value = addr[0];
					if(read_value != write_value)
						++errors_num;
				}
			}
		}
	}

	return errors_num;
}

int __attribute__ ((section (".boot"))) test_ddrBitChargeLeakage(uint32_t address, int size)
{
	int bank, row, column, i;
	uint16_t *addr;
	uint16_t read_value;
	int errors_num = 0;

	for(bank = 0; bank < BANK_COUNT; ++bank) {
		for(row = 1; row < ROW_COUNT - 1; ++row) {
			/* Write pattern to tested row. */
			for(column = 0; column < COLUMN_COUNT * COLUMN_CHUNK_COUNT; ++column) {
				addr = (uint16_t*) (0x80000000 | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
				addr[0] = 0xffff;
			}

			/* Read multiple times neighboring rows. */
			for(i = 0; i < 10000; ++i) {
				/* Read previous row. */
				addr = (uint16_t*) (0x80000000 | ROW_SET(row - 1) | BANK_SET(bank) | COLUMN_SET(0));
				read_value = addr[0];

				/* Read next row. */
				addr = (uint16_t*) (0x80000000 | ROW_SET(row + 1) | BANK_SET(bank) | COLUMN_SET(0));
				read_value = addr[0];
			}

			/* Check if tested row has changed value. */
			for(column = 0; column < COLUMN_COUNT * COLUMN_CHUNK_COUNT; ++column) {
				addr = (uint16_t*) (0x80000000 | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
				read_value = addr[0];
				if(read_value != 0xffff) {
					++errors_num;
					break;
				}
			}
		}
	}

	return errors_num;
}

int __attribute__ ((section (".boot"))) test_ddrFullMemtest(uint32_t address, int size, int iterations)
{
	int i, accessibility_errors = 0, crosstalk_errors = 0, leakage_errors = 0;

	for(i = 0; i < iterations; ++i) {
		accessibility_errors = test_ddrAccessibility(address, size);
		crosstalk_errors = test_ddrBitCrossTalk(address, size);
		leakage_errors = test_ddrBitChargeLeakage(address, size);
	}

	return accessibility_errors + crosstalk_errors + leakage_errors;
}
