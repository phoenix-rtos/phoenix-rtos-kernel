/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL console
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005-2008 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "console.h"
#include "string.h"
#include "spinlock.h"
#include "pmap.h"

#include "../../../include/errno.h"


#define VRAM_MONO     (void *)0xb0000
#define VRAM_COLOR    (void *)0xb8000

#define BASE_MONO     0x3b4  /* crtc index register address mono */
#define BASE_COLOR    0x3d4  /* crtc index register address color */

#define CRTC_CURSORH  0x0e   /* cursor address mid */
#define CRTC_CURSORL  0x0f   /* cursor address low */

#define MAIN_MISCIN   0x3cc  /* miscellaneous input register */


struct {
	spinlock_t spinlock;
	unsigned char rows;
	unsigned char maxcol;
	unsigned char row;
	unsigned char col;
	u16 *vram;
	void *crtc;
} console;


void hal_consolePrint(int attr, const char *s)
{
	const char *p;

	hal_spinlockSet(&console.spinlock);

	for (p = s; *p; p++) {
		switch (*p) {
		case '\n':
			console.row++;
		case '\r':
			console.col = 0;
			break;

		/* delete seqence */
		case '\b':
		case '\177':
			if (console.col)
				console.col--;
			else {
				if (console.row) {
					console.row--;
					console.col = console.maxcol - 1;
				}
			}
			break;

		default:
			*(console.vram + console.row * console.maxcol + console.col) = *p | attr << 8;
			console.col++;
			break;
		}

		/* end of line is reached */
		if (console.col == console.maxcol) {
			console.row++;
			console.col = 0;
		}

		/* scroll down */
		if (console.row == console.rows) {
			hal_memcpy(console.vram, console.vram + console.maxcol, console.maxcol * (console.rows - 1) * 2);
			console.row--;
			console.col = 0;
			hal_memsetw(console.vram + console.maxcol * (console.rows - 1), attr << 8 | ' ', console.maxcol);
		}

		/* Set hardware cursor position */
		hal_outb(console.crtc, CRTC_CURSORH);
		hal_outb(console.crtc + 1, (console.row * console.maxcol + console.col) >> 8);
		hal_outb(console.crtc, CRTC_CURSORL);
		hal_outb(console.crtc + 1, (console.row * console.maxcol + console.col) & 0xff);
	}
	hal_spinlockClear(&console.spinlock);

	return;
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	int isColor = 0;

	/* Test monitor type */
	isColor = (hal_inb((void *)MAIN_MISCIN) & 0x01);

	console.rows = 25;
	console.maxcol = 80;
	console.row = 0;
	console.col = 0;

	console.vram = VADDR_KERNEL + (isColor ? VRAM_COLOR : VRAM_MONO);
	console.crtc = isColor ? (void *)BASE_COLOR : (void *)BASE_MONO;

	hal_memsetw(console.vram, ' ' | ATTR_NORMAL << 8, console.rows * console.maxcol);

	hal_spinlockCreate(&console.spinlock, "console.spinlock");
	return;
}
