/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMX6ULL processor memory test
 *
 * Copyright 2015, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "cpu.h"


static void test_ddrPutch(char c)
{
	while (!(*((volatile u32 *)0x02020094) & 0x2000));

	*((volatile u32 *)0x02020040) = c;
}


static void test_ddrPrintStr(const char *str)
{
	while (*str != '\0')
		test_ddrPutch(*(str++));
}


static void test_ddrPrintUint(int n)
{
	char buff[12];
	int i;

	if (!n) {
		test_ddrPutch('0');

		return;
	}

	for (i = 0; i < sizeof(buff); ++i) {
		if (n) {
			buff[i] = '0' + (n % 10);
			n /= 10;
		}
		else {
			break;
		}
	}

	--i;

	while(i >= 0)
		test_ddrPutch(buff[i--]);
}


static int test_ddrByteAccessibility(u32 address, u32 size)
{
	volatile u8 *ddr = (u8 *) address;
	u32 i;
	u8 val;
	int errors = 0;

	/* write_value overflow here is intentional. */
	for (i = 0, val = 0; i < size; ++i, ++val)
		ddr[i] = val;

	for (i = 0, val = 0; i < size; ++i, ++val) {
		if(ddr[i] != val)
			++errors;
	}

	return errors;
}


static int test_ddrWordAccessibility(u32 address, u32 size)
{
	volatile u32 *ddr = (u32 *)address;
	u32 i, val;
	int errors = 0;

	for (i = 0; i < (size >> 2); ++i) {
		val = i << 2;
		ddr[i] = val;
	}

	for (i = 0; i < (size >> 2); ++i) {
		val = i << 2;
		if(ddr[i] != val)
			++errors;
	}

	return errors;
}


int test_ddrAccessibility(u32 address, u32 size)
{
	int errors;

	errors = test_ddrByteAccessibility(address, size);
	errors += test_ddrWordAccessibility(address, size);

	return errors;
}

#define BANK_COUNT              8
#define BANK_SELECT_MASK        0x3800
#define BANK_SELECT_SHIFT       11
#define BANK_SET(x)             (((u32)(((u32)(x)) << BANK_SELECT_SHIFT)) & BANK_SELECT_MASK)
#define BANK_GET(x)             (((u32)(((u32)(x)) & BANK_SELECT_MASK)) >> BANK_SELECT_SHIFT)

#define COLUMN_COUNT            1024
#define COLUMN_SELECT_MASK      0x7fe
#define COLUMN_SELECT_SHIFT     1
#define COLUMN_SET(x)           (((u32)(((u32)(x)) << COLUMN_SELECT_SHIFT)) & COLUMN_SELECT_MASK)
#define COLUMN_GET(x)           (((u32)(((u32)(x)) & COLUMN_SELECT_MASK)) >> COLUMN_SELECT_SHIFT)

#define ROW_COUNT               8192
#define ROW_SELECT_MASK         0x7ffc000
#define ROW_SELECT_SHIFT        14
#define ROW_SET(x)              (((u32)(((u32)(x)) << ROW_SELECT_SHIFT)) & ROW_SELECT_MASK)
#define ROW_GET(x)              (((u32)(((u32)(x)) & ROW_SELECT_MASK)) >> ROW_SELECT_SHIFT)

/* DDR3 RAM Addressing layout
 * Physical address:
 *                   0|000 0000 0000 00|00 0|000 0000 0000 000|0
 *        chip select |       row      |bank|      column     | data path
 *            1b      |       13b      | 3b |       10b       | 1b
 */

static const u16 patterns[] = { 0x5555, ~0x5555, 0x3333, ~0x3333, 0x0f0f, ~0x0f0f, 0x00ff, ~0x00ff };

u16 generateTestVector(int pattern, int column)
{
	int flip_shift = (pattern - 8) >> 1;
	u16 vector = 0;
	int flip;

	if (pattern < sizeof(patterns) / sizeof(patterns[0]))
		return patterns[pattern];

	/* Starting from pattern 8 we have to change vector for each column. */
	if (pattern & 1)
		vector = ~vector;

	if (!flip_shift) {
		if (column & 1)
			vector = ~vector;
	}
	else {
		flip = column >> flip_shift;
		if (flip & 1)
			vector = ~vector;
	}

	return vector;
}


int test_ddrBitCrossTalk(u32 address)
{
	int bank, row, column, pattern;
	volatile u16 *addr;
	int errors = 0;

	for (bank = 0; bank < BANK_COUNT; ++bank) {
		test_ddrPrintStr("\nCross talk: Testing bank #");
		test_ddrPrintUint(bank);

		for (pattern = 0; pattern < 30; ++pattern) {
			/* Write test pattern vectors. */
			for (row = 0; row < ROW_COUNT; ++row) {
				for (column = 0; column < COLUMN_COUNT; ++column) {
					addr = (u16 *)(address | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
					*addr = generateTestVector(pattern, column);
				}
			}

			/* Read and compare test vectors. */
			for (row = 0; row < ROW_COUNT; ++row) {
				for (column = 0; column < COLUMN_COUNT; ++column) {
					addr = (u16 *)(address | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
					if (*addr != generateTestVector(pattern, column))
						++errors;
				}
			}
		}
	}

	return errors;
}


int test_ddrBitChargeLeakage(u32 address)
{
	int bank, row, column, i;
	volatile u16 *addr;
	volatile u16 read_value;
	int errors = 0;

	for (bank = 0; bank < BANK_COUNT; ++bank) {
		test_ddrPrintStr("\nCharge leakage: Testing bank #");
		test_ddrPrintUint(bank);

		for (row = 1; row < ROW_COUNT - 1; ++row) {
			/* Write pattern to tested row. */
			for (column = 0; column < COLUMN_COUNT; ++column) {
				addr = (u16 *) (address | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
				*addr = 0xffff;
			}

			/* Read multiple times neighboring rows. */
			for (i = 0; i < 10000; ++i) {
				/* Read previous row. */
				addr = (u16 *) (address | ROW_SET(row - 1) | BANK_SET(bank) | COLUMN_SET(0));
				read_value = *addr;

				/* Read next row. */
				addr = (u16 *) (address | ROW_SET(row + 1) | BANK_SET(bank) | COLUMN_SET(0));
				read_value = *addr;
			}

			/* Check if tested row has changed value. */
			for (column = 0; column < COLUMN_COUNT; ++column) {
				addr = (u16 *) (address | ROW_SET(row) | BANK_SET(bank) | COLUMN_SET(column));
				read_value = *addr;
				if (read_value != 0xffff)
					++errors;
			}
		}
	}

	return errors;
}


int test_ddrAll(void)
{
	unsigned int i = 0;
	u32 address = 0x80000000, size = 128 * 1024 * 1024;
	int errors;

	test_ddrPrintStr("\033[2J");
	test_ddrPrintStr("\033[0;0f");
	test_ddrPrintStr("Phoenix-RTOS memtest\n");
	test_ddrPrintStr("\nStarting test");

	while (1) {
		++i;

		test_ddrPrintStr("\n\nPass #");
		test_ddrPrintUint(i);

		test_ddrPrintStr("\nAccessibility test");
		errors = test_ddrAccessibility(address, size);
		test_ddrPrintStr("\nErrors: ");
		test_ddrPrintUint(errors);

		test_ddrPrintStr("\nCrosstalk test");
		errors = test_ddrBitCrossTalk(address);
		test_ddrPrintStr("\nErrors: ");
		test_ddrPrintUint(errors);

		test_ddrPrintStr("\nLeakage test");
		errors = test_ddrBitChargeLeakage(address);
		test_ddrPrintStr("\nErrors: ");
		test_ddrPrintUint(errors);
	}
}
