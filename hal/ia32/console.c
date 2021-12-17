/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * VGA console
 *
 * Copyright 2012, 2016, 2020, 2021 Phoenix Systems
 * Copyright 2001, 2005-2008 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../console.h"
#include "../string.h"
#include "../spinlock.h"
#include "../pmap.h"
#include "ia32.h"


/* ANSI escape sequence states */
enum {
	esc_init, /* normal */
	esc_esc,  /* esc */
	esc_csi,  /* esc [ */
	esc_csiqm /* esc [ ? */
};


/* ANSI code to VGA foreground character attribute conversion table */
static const unsigned char ansi2fg[] = {
	0x00, 0x04, 0x02, 0x06,
	0x01, 0x05, 0x03, 0x07
};


/* ANSI code to VGA background character attribute conversion table */
static const unsigned char ansi2bg[] = {
	0x00, 0x40, 0x20, 0x60,
	0x10, 0x50, 0x30, 0x70
};


struct {
	volatile u16 *vram;      /* Video memory */
	void *crtc;              /* CRT controller register */
	unsigned int rows;       /* Console height */
	unsigned int cols;       /* Console width */
	unsigned char attr;      /* Character attribute */
	unsigned char esc;       /* Escape sequence state */
	unsigned char parmi;     /* Escape sequence parameter index */
	unsigned char parms[10]; /* Escape sequence parameters buffer */
	spinlock_t spinlock;
} halconsole_common;


static void console_memset(volatile u16 *vram, u16 val, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		*(vram + i) = val;
}


static void console_memmove(volatile u16 *dst, volatile u16 *src, unsigned int n)
{
	unsigned int i;

	if (dst < src) {
		for (i = 0; i < n; i++)
			dst[i] = src[i];
	}
	else {
		for (i = n; i--;)
			dst[i] = src[i];
	}
}


static void _hal_consolePrint(const char *s)
{
	unsigned int i, row, col, pos;
	char c;

	spinlock_ctx_t sc;

	hal_spinlockSet(&halconsole_common.spinlock, &sc);

	/* Print from current cursor position */
	hal_outb(halconsole_common.crtc, 0x0f);
	pos = hal_inb((void *)((addr_t)halconsole_common.crtc + 1));
	hal_outb(halconsole_common.crtc, 0x0e);
	pos |= (u16)hal_inb((void *)((addr_t)halconsole_common.crtc + 1)) << 8;
	row = pos / halconsole_common.cols;
	col = pos % halconsole_common.cols;

	while ((c = *s++)) {
		/* Control character */
		if ((c < ' ') || (c == '\177')) {
			switch (c) {
				case '\b':
				case '\177':
					if (col) {
						col--;
					}
					else if (row) {
						row--;
						col = halconsole_common.cols - 1;
					}
					break;

				case '\n':
					row++;
				case '\r':
					col = 0;
					break;

				case '\e':
					hal_memset(halconsole_common.parms, 0, sizeof(halconsole_common.parms));
					halconsole_common.parmi = 0;
					halconsole_common.esc = esc_esc;
					break;
			}
		}
		/* Process character according to escape sequence state */
		else {
			switch (halconsole_common.esc) {
				case esc_init:
					*(halconsole_common.vram + row * halconsole_common.cols + col) = (u16)halconsole_common.attr << 8 | c;
					col++;
					break;

				case esc_esc:
					switch (c) {
						case '[':
							hal_memset(halconsole_common.parms, 0, sizeof(halconsole_common.parms));
							halconsole_common.parmi = 0;
							halconsole_common.esc = esc_csi;
							break;

						default:
							halconsole_common.esc = esc_init;
							break;
					}
					break;

				case esc_csi:
					switch (c) {
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							halconsole_common.parms[halconsole_common.parmi] *= 10;
							halconsole_common.parms[halconsole_common.parmi] += c - '0';
							break;

						case ';':
							if (halconsole_common.parmi + 1 < sizeof(halconsole_common.parms))
								halconsole_common.parmi++;
							break;

						case '?':
							halconsole_common.esc = esc_csiqm;
							break;

						case 'H':
							if (halconsole_common.parms[0] < 1)
								halconsole_common.parms[0] = 1;
							else if (halconsole_common.parms[0] > halconsole_common.rows)
								halconsole_common.parms[0] = halconsole_common.rows;

							if (halconsole_common.parms[1] < 1)
								halconsole_common.parms[1] = 1;
							else if (halconsole_common.parms[1] > halconsole_common.cols)
								halconsole_common.parms[1] = halconsole_common.cols;

							row = halconsole_common.parms[0] - 1;
							col = halconsole_common.parms[1] - 1;
							halconsole_common.esc = esc_init;
							break;

						case 'J':
							switch (halconsole_common.parms[0]) {
								case 0:
									console_memset(halconsole_common.vram + row * halconsole_common.cols + col, (u16)halconsole_common.attr << 8 | ' ', halconsole_common.cols * (halconsole_common.rows - row) - col);
									break;

								case 1:
									console_memset(halconsole_common.vram, (u16)halconsole_common.attr << 8 | ' ', row * halconsole_common.cols + col + 1);
									break;

								case 2:
									console_memset(halconsole_common.vram, (u16)halconsole_common.attr << 8 | ' ', halconsole_common.rows * halconsole_common.cols);
									break;
							}
							halconsole_common.esc = esc_init;
							break;

						case 'm':
							i = 0;
							do {
								switch (halconsole_common.parms[i]) {
									case 0:
										halconsole_common.attr = 0x07;
										break;

									case 1:
										halconsole_common.attr = 0x0f;
										break;

									case 30:
									case 31:
									case 32:
									case 33:
									case 34:
									case 35:
									case 36:
									case 37:
										halconsole_common.attr = (halconsole_common.attr & 0xf0) | ansi2fg[(halconsole_common.parms[i] - 30) & 0x7];
										break;

									case 40:
									case 41:
									case 42:
									case 43:
									case 44:
									case 45:
									case 46:
									case 47:
										halconsole_common.attr = ansi2bg[(halconsole_common.parms[i] - 40) & 0x7] | (halconsole_common.attr & 0x0f);
										break;
								}
							} while (i++ < halconsole_common.parmi);

						default:
							halconsole_common.esc = esc_init;
							break;
					}
					break;

				case esc_csiqm:
					switch (c) {
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							halconsole_common.parms[halconsole_common.parmi] *= 10;
							halconsole_common.parms[halconsole_common.parmi] += c - '0';
							break;

						case ';':
							if (halconsole_common.parmi + 1 < sizeof(halconsole_common.parms))
								halconsole_common.parmi++;
							break;

						case 'h':
							switch (halconsole_common.parms[0]) {
								case 25:
									hal_outb(halconsole_common.crtc, 0x0a);
									hal_outb((void *)((addr_t)halconsole_common.crtc + 1), hal_inb((void *)((addr_t)halconsole_common.crtc + 1)) & ~0x20);
									break;
							}
							halconsole_common.esc = esc_init;
							break;

						case 'l':
							switch (halconsole_common.parms[0]) {
								case 25:
									hal_outb(halconsole_common.crtc, 0x0a);
									hal_outb((void *)((addr_t)halconsole_common.crtc + 1), hal_inb((void *)((addr_t)halconsole_common.crtc + 1)) | 0x20);
									break;
							}
							halconsole_common.esc = esc_init;
							break;

						default:
							halconsole_common.esc = esc_init;
							break;
					}
					break;
			}
		}

		/* End of line */
		if (col == halconsole_common.cols) {
			row++;
			col = 0;
		}

		/* Scroll down */
		if (row == halconsole_common.rows) {
			i = halconsole_common.cols * (halconsole_common.rows - 1);
			console_memmove(halconsole_common.vram, halconsole_common.vram + halconsole_common.cols, i);
			console_memset(halconsole_common.vram + i, (u16)halconsole_common.attr << 8 | ' ', halconsole_common.cols);
			row--;
			col = 0;
		}

		/* Update cursor */
		i = row * halconsole_common.cols + col;
		hal_outb(halconsole_common.crtc, 0x0e);
		hal_outb((void *)((addr_t)halconsole_common.crtc + 1), i >> 8);
		hal_outb(halconsole_common.crtc, 0x0f);
		hal_outb((void *)((addr_t)halconsole_common.crtc + 1), i);
		*((u8 *)(halconsole_common.vram + i) + 1) = halconsole_common.attr;
	}

	hal_spinlockClear(&halconsole_common.spinlock, &sc);
}


void hal_consolePrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD)
		_hal_consolePrint(CONSOLE_BOLD);
	else if (attr != ATTR_USER)
		_hal_consolePrint(CONSOLE_CYAN);

	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consolePutch(char c)
{
	const char str[] = { c, '\0' };
	_hal_consolePrint(str);
}


__attribute__ ((section (".init"))) void _hal_consoleInit(void)
{
	unsigned char color;

	/* Check color support */
	color = hal_inb((void *)0x3cc) & 0x01;

	/* Initialize VGA */
	halconsole_common.vram = (u16 *)(VADDR_KERNEL + (color ? 0xb8000 : 0xb0000));
	halconsole_common.crtc = (void *)(color ? 0x3d4 : 0x3b4);

	/* Default 80x25 text mode with cyan color attribute */
	halconsole_common.rows = 25;
	halconsole_common.cols = 80;
	halconsole_common.attr = 0x03;
	hal_spinlockCreate(&halconsole_common.spinlock, "console.spinlock");
}
