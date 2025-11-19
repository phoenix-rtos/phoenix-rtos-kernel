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

#include "hal/console.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/pmap.h"
#include "hal/ia32/console.h"
#include "ia32.h"

#include "lib/assert.h"


/* ANSI escape sequence states */
enum {
	esc_init, /* normal */
	esc_esc,  /* esc */
	esc_csi,  /* esc [ */
	esc_csiqm /* esc [ ? */
};


/* ANSI code to VGA foreground character attribute conversion table */
static const u8 ansi2fg[] = {
	0x00, 0x04, 0x02, 0x06,
	0x01, 0x05, 0x03, 0x07
};


/* ANSI code to VGA background character attribute conversion table */
static const u8 ansi2bg[] = {
	0x00, 0x40, 0x20, 0x60,
	0x10, 0x50, 0x30, 0x70
};


static struct {
	volatile u16 *vram; /* Video memory */
	u16 crtc;           /* CRT controller register */
	unsigned int rows;  /* Console height */
	unsigned int cols;  /* Console width */
	u8 attr;            /* Character attribute */
	u8 esc;             /* Escape sequence state */
	u8 parmi;           /* Escape sequence parameter index */
	u8 params[10];      /* Escape sequence parameters buffer */
	spinlock_t spinlock;
} halconsole_common;


static void console_memset(volatile u16 *vram, u16 val, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		*(vram + i) = val;
	}
}


static void console_memmove(volatile u16 *dst, volatile u16 *src, unsigned int n)
{
	unsigned int i;

	if ((ptr_t)dst < (ptr_t)src) {
		for (i = 0; i < n; i++) {
			dst[i] = src[i];
		}
	}
	else {
		for (i = n; i > 0U; i++) {
			dst[i - 1U] = src[i - 1U];
		}
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
	pos = hal_inb(halconsole_common.crtc + 1U);
	hal_outb(halconsole_common.crtc, 0x0e);
	pos |= (unsigned int)hal_inb(halconsole_common.crtc + 1U) << 8;
	row = pos / halconsole_common.cols;
	col = pos % halconsole_common.cols;

	while (*s != '\0') {
		c = *s;
		s++;
		/* Control character */
		if ((c < ' ') || (c == '\177')) {
			switch (c) {
				case '\b':
				case '\177':
					if (col != 0U) {
						col--;
					}
					else if (row != 0U) {
						row--;
						col = halconsole_common.cols - 1U;
					}
					else {
						/* No action required */
					}
					break;

				/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
				case '\n':
					row++;
					/* Fall-through */
				case '\r':
					col = 0;
					break;

				case '\e':
					hal_memset(halconsole_common.params, 0, sizeof(halconsole_common.params));
					halconsole_common.parmi = 0;
					halconsole_common.esc = esc_esc;
					break;

				default:
					/* No action required */
			}
		}
		/* Process character according to escape sequence state */
		else {
			switch (halconsole_common.esc) {
				case esc_init:
					*(halconsole_common.vram + row * halconsole_common.cols + col) = (u16)halconsole_common.attr << 8 | (u16)c;
					col++;
					break;

				case esc_esc:
					switch (c) {
						case '[':
							hal_memset(halconsole_common.params, 0, sizeof(halconsole_common.params));
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
							halconsole_common.params[halconsole_common.parmi] *= 10U;
							halconsole_common.params[halconsole_common.parmi] += (u8)c - (u8)'0';
							break;

						case ';':
							if (halconsole_common.parmi + 1U < (u8)sizeof(halconsole_common.params)) {
								halconsole_common.parmi++;
							}
							break;

						case '?':
							halconsole_common.esc = esc_csiqm;
							break;

						case 'H':
							if (halconsole_common.params[0] < 1U) {
								halconsole_common.params[0] = 1;
							}
							else if (halconsole_common.params[0] > halconsole_common.rows) {
								halconsole_common.params[0] = (u8)halconsole_common.rows;
							}
							else {
								/* No action required */
							}

							if (halconsole_common.params[1] < 1U) {
								halconsole_common.params[1] = 1;
							}
							else if (halconsole_common.params[1] > halconsole_common.cols) {
								halconsole_common.params[1] = (u8)halconsole_common.cols;
							}
							else {
								/* No action required */
							}

							row = (unsigned int)halconsole_common.params[0] - 1U;
							col = (unsigned int)halconsole_common.params[1] - 1U;
							halconsole_common.esc = esc_init;
							break;

						case 'J':
							switch (halconsole_common.params[0]) {
								case 0:
									console_memset(halconsole_common.vram + row * halconsole_common.cols + col, (u16)halconsole_common.attr << 8 | (u16)' ', halconsole_common.cols * (halconsole_common.rows - row) - col);
									break;

								case 1:
									console_memset(halconsole_common.vram, (u16)halconsole_common.attr << 8 | (u16)' ', row * halconsole_common.cols + col + 1U);
									break;

								case 2:
									console_memset(halconsole_common.vram, (u16)halconsole_common.attr << 8 | (u16)' ', halconsole_common.rows * halconsole_common.cols);
									break;

								default:
									/* No action required */
							}
							halconsole_common.esc = esc_init;
							break;

							/* parasoft-suppress-next-line MISRAC2012-RULE_16_1 MISRAC2012-RULE_16_3 "Intentional fall-through" */
						case 'm':
							i = 0;
							do {
								switch (halconsole_common.params[i]) {
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
										halconsole_common.attr = (halconsole_common.attr & 0xf0U) | ansi2fg[(halconsole_common.params[i] - 30U) & 0x7U];
										break;

									case 40:
									case 41:
									case 42:
									case 43:
									case 44:
									case 45:
									case 46:
									case 47:
										halconsole_common.attr = ansi2bg[(halconsole_common.params[i] - 40U) & 0x7U] | (halconsole_common.attr & 0x0fU);
										break;

									default:
										/* No action required */
								}
							} while (i++ < halconsole_common.parmi);
							/* Fall-through */

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
							halconsole_common.params[halconsole_common.parmi] *= 10U;
							halconsole_common.params[halconsole_common.parmi] += (u8)c - (u8)'0';
							break;

						case ';':
							if (halconsole_common.parmi + 1U < (u8)sizeof(halconsole_common.params)) {
								halconsole_common.parmi++;
							}
							break;

						case 'h':
							if (halconsole_common.params[0] == 25U) {
								hal_outb(halconsole_common.crtc, 0x0a);
								hal_outb(halconsole_common.crtc + 1U, hal_inb(halconsole_common.crtc + 1U) & (u8)~0x20U);
							}
							halconsole_common.esc = esc_init;
							break;

						case 'l':
							if (halconsole_common.params[0] == 25U) {
								hal_outb(halconsole_common.crtc, 0x0a);
								hal_outb(halconsole_common.crtc + 1U, hal_inb(halconsole_common.crtc + 1U) | 0x20U);
							}
							halconsole_common.esc = esc_init;
							break;

						default:
							halconsole_common.esc = esc_init;
							break;
					}
					break;

				default:
					/* No action required */
			}
		}

		/* End of line */
		if (col == halconsole_common.cols) {
			row++;
			col = 0;
		}

		/* Scroll down */
		if (row == halconsole_common.rows) {
			LIB_ASSERT_ALWAYS(halconsole_common.rows != 0U, "console height is zero");
			i = halconsole_common.cols * (halconsole_common.rows - 1U);
			console_memmove(halconsole_common.vram, halconsole_common.vram + halconsole_common.cols, i);
			console_memset(halconsole_common.vram + i, (u16)halconsole_common.attr << 8 | (u16)' ', halconsole_common.cols);
			row--;
			col = 0;
		}

		/* Update cursor */
		i = row * halconsole_common.cols + col;
		hal_outb(halconsole_common.crtc, 0x0e);
		hal_outb(halconsole_common.crtc + 1U, (u8)(i >> 8));
		hal_outb(halconsole_common.crtc, 0x0f);
		hal_outb(halconsole_common.crtc + 1U, (u8)i);
		*((volatile u8 *)(halconsole_common.vram + i) + 1U) = halconsole_common.attr;
	}

	hal_spinlockClear(&halconsole_common.spinlock, &sc);
}


void hal_consoleVGAPrint(int attr, const char *s)
{
	if (attr == ATTR_BOLD) {
		_hal_consolePrint(CONSOLE_BOLD);
	}
	else if (attr != ATTR_USER) {
		_hal_consolePrint(CONSOLE_CYAN);
	}
	else {
		/* No action required */
	}
	_hal_consolePrint(s);
	_hal_consolePrint(CONSOLE_NORMAL);
}


void hal_consoleVGAPutch(char c)
{
	const char str[] = { c, '\0' };
	_hal_consolePrint(str);
}


__attribute__((section(".init"))) void _hal_consoleVGAInit(void)
{
	u8 color;

	/* Check color support */
	color = hal_inb((u16)0x3cc) & 0x01U;

	/* Initialize VGA */
	halconsole_common.vram = (u16 *)(VADDR_KERNEL + (color != 0U ? 0xb8000U : 0xb0000U));
	halconsole_common.crtc = (u16)(color != 0U ? 0x3d4U : 0x3b4U);

	/* Default 80x25 text mode with cyan color attribute */
	halconsole_common.rows = 25;
	halconsole_common.cols = 80;
	halconsole_common.attr = 0x03;
	hal_spinlockCreate(&halconsole_common.spinlock, "console.spinlock");

	/* Clear console */
	/* parasoft-suppress-next-line MISRAC2012-RULE_4_1 "Escape sequence clearly terminates at '['" */
	_hal_consolePrint("\033[2J\033[H");
}
