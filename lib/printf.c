/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - printf
 *
 * Copyright 2012, 2014, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Pawel Krezolek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdarg.h>

#include "hal/hal.h"

#include "log/log.h"

/* Flags used for printing */
#define FLAG_SIGNED       0x1UL
#define FLAG_64BIT        0x2UL
#define FLAG_SPACE        0x10UL
#define FLAG_ZERO         0x20UL
#define FLAG_PLUS         0x40UL
#define FLAG_HEX          0x80UL
#define FLAG_LARGE_DIGITS 0x100UL


static char *printf_sprintf_int(char *out, u64 num64, u32 flags, int min_number_len)
{
	/* MISRA Rule 10.4: changed type by adding U */
	const char *digits = (flags & FLAG_LARGE_DIGITS) != 0U ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp_buf[32];
	char sign = '\0';
	char *tmp = tmp_buf;

	u32 num32 = (u32)num64;
	u32 num_high = (u32)(num64 >> 32);

	/* MISRA Rule 10.4: changed type by adding U */
	/* parasoft-begin-suppress MISRAC2012-RULE_10_3
	 * "This function uses clever tricks to convert radix of the incoming num64"
	 */
	if ((flags & FLAG_SIGNED) != 0U) {
		if ((flags & FLAG_64BIT) != 0U) {
			if ((s32)num_high < 0) {
				num64 = -(s64)num64;
				num32 = (u32)num64;
				num_high = (u32)(num64 >> 32);
				sign = '-';
			}
		}
		else if ((s32)num32 < 0) {
			num32 = -(s32)num32;
			sign = '-';
		}

		if (sign == '\0') {
			if ((flags & FLAG_SPACE) != 0U) {
				sign = ' ';
			}
			else if ((flags & FLAG_PLUS) != 0U) {
				sign = '+';
			}
		}
	}
	/* parasoft-end-suppress MISRAC2012-RULE_10_3 */

	if ((flags & FLAG_64BIT) != 0U && num_high == 0x0U) {
		flags &= ~FLAG_64BIT;
	}

	if (num64 == 0U) {
		*tmp++ = '0';
	}
	else if ((flags & FLAG_HEX) != 0U) {
		if ((flags & FLAG_64BIT) != 0U) {
			int i;

			for (i = 0; i < 8; ++i) {
				*tmp++ = digits[num32 & 0x0fU];
				num32 >>= 4;
			}

			while (num_high != 0U) {
				*tmp++ = digits[num_high & 0x0fU];
				num_high >>= 4;
			}
		}
		else {
			while (num32 != 0U) {
				*tmp++ = digits[num32 & 0x0fU];
				num32 >>= 4;
			}
		}
	}
	else {
		if ((flags & FLAG_64BIT) != 0U) {  // TODO: optimize
			while (num64 != 0U) {
				*tmp++ = digits[num64 % 10U];
				num64 /= 10U;
			}
		}
		else {
			while (num32 != 0U) {
				*tmp++ = digits[num32 % 10U];
				num32 /= 10U;
			}
		}
	}

	const int digits_cnt = tmp - tmp_buf;
	int pad_len = min_number_len - digits_cnt - (sign != '\0' ? 1 : 0);

	/* pad, if needed */
	if (pad_len > 0 && (flags & FLAG_ZERO) == 0U) {
		while (pad_len-- > 0) {
			*out++ = ' ';
		}
	}

	if (sign != '\0') {
		*out++ = sign;
	}

	/* pad, if needed */
	if (pad_len > 0 && (flags & FLAG_ZERO) != 0U) {
		while (pad_len-- > 0) {
			*out++ = '0';
		}
	}

	/* copy reversed */
	while ((--tmp) >= tmp_buf) {
		*out++ = *tmp;
	}

	return out;
}


void lib_putch(char s)
{
	char c[2];

	c[0] = s;
	c[1] = '\0';

	/* MISRA Rule 17.7: Unused return value, (void) added */
	(void)log_write(c, 1);
}


int lib_vsprintf(char *out, const char *format, va_list args)
{
	char *const out_start = out;
	out[0] = '\0';

	for (;;) {
		char fmt = *format++;
		if (fmt == '\0') {
			goto end;
		}
		if (fmt != '%') {
			*out++ = fmt;
			continue;
		}

		fmt = *format++;
		if (fmt == '\0') {
			*out++ = '%';
			goto end;
		}

		/* precision, padding (set default to 6 digits) */
		u32 flags = 0, min_number_len = 0;

		for (;;) {
			if (fmt == ' ') {
				flags |= FLAG_SPACE;
			}
			else if (fmt == '0') {
				flags |= FLAG_ZERO;
			}
			else if (fmt == '+') {
				flags |= FLAG_PLUS;
			}
			else {
				break;
			}

			fmt = *format++;
			if (fmt == '\0') {
				goto bad_format;
			}
		}

		/* leading number digits-cnt */
		while (fmt >= '0' && fmt <= '9') {
			min_number_len = min_number_len * 10U + ((unsigned int)fmt - (unsigned int)'0');
			fmt = *format++;
			if (fmt == '\0') {
				goto bad_format;
			}
		}

		/* fractional number digits-cnt (only a single digit is acceptable in this impl.) */
		if (fmt == '.') {
			goto bad_format;
		}

		if (fmt == 'l') {
			fmt = *format++;
			if (fmt == '\0') {
				goto bad_format;
			}

			if (fmt == 'l') {
				flags |= FLAG_64BIT;
				fmt = *format++;
				if (fmt == '\0') {
					goto bad_format;
				}
			}
		}

		if (fmt == 'z') {
			fmt = *format++;
			if (fmt == '\0') {
				goto bad_format;
			}
			if (sizeof(void *) == sizeof(u64)) {  // FIXME "size_t" is undefined?
				flags |= FLAG_64BIT;
			}
		}

		u64 number = 0;

		switch (fmt) {
			case 's': {
				const char *s = va_arg(args, char *);
				if (s == NULL) {
					s = "(null)";
				}
				const unsigned int s_len = hal_strlen(s);
				hal_memcpy(out, s, s_len);
				out += s_len;
			} break;

			case 'c': {
				const char c = (char)va_arg(args, int);
				*out++ = c;
			} break;

			case 'X':
				flags |= FLAG_LARGE_DIGITS;
			case 'x':
				flags |= FLAG_HEX;
				goto get_number;
			case 'd':
			case 'i':
				flags |= FLAG_SIGNED;
			case 'u':
				goto get_number;
			case 'p': {
				const void *s = va_arg(args, void *);
				if (s == NULL) {
					*out++ = '(';
					*out++ = 'n';
					*out++ = 'i';
					*out++ = 'l';
					*out++ = ')';
					break;
				}
				number = (u64)(size_t)s;
				flags |= (FLAG_ZERO | FLAG_HEX);
				if (sizeof(void *) == sizeof(u64)) {
					flags |= FLAG_64BIT;
				}
				min_number_len = sizeof(void *) * 2U;
				goto handle_number;
			} break;

			case '%':
				*out++ = '%';
				break;
			default:
				*out++ = '%';
				*out++ = fmt;
				break;
		}
		continue;

	get_number:;
		if ((flags & FLAG_64BIT) != 0U) {
			number = va_arg(args, u64);
		}
		else {
			number = va_arg(args, u32);
		}

	handle_number:;
		out = printf_sprintf_int(out, number, flags, (int)min_number_len);
		continue;
	}

bad_format:
end:

	*out = '\0';
	return out - out_start;
}


int lib_vprintf(const char *format, va_list ap)
{
	int i = 0;
	char fmt, c;
	const char *s;
	u32 flags, min_number_len;
	u64 number;
	char buff[24];
	char *sptr, *eptr;

	s = CONSOLE_CYAN;
	while (*s != '\0') {
		lib_putch(*(s++));
	}

	for (;;) {
		fmt = *format++;
		if (fmt == '\0') {
			goto end;
		}

		if (fmt != '%') {
			lib_putch(fmt);
			i++;
			continue;
		}

		fmt = *format++;

		if (fmt == '\0') {
			lib_putch('%');
			i++;
			goto end;
		}

		/* precission, padding (set default to 6 digits) */
		flags = 0;
		min_number_len = 0;

		for (;;) {
			if (fmt == ' ') {
				flags |= FLAG_SPACE;
			}
			else if (fmt == '0') {
				flags |= FLAG_ZERO;
			}
			else if (fmt == '+') {
				flags |= FLAG_PLUS;
			}
			else {
				break;
			}

			fmt = *format++;

			if (fmt == '\0') {
				goto end;
			}
		}

		/* leading number digits-cnt */
		while (fmt >= '0' && fmt <= '9') {
			min_number_len = min_number_len * 10U + ((unsigned int)fmt - (unsigned int)'0');
			fmt = *format++;

			if (fmt == '\0') {
				goto end;
			}
		}

		/* fractional number digits-cnt (only a single digit is acceptable in this impl.) */
		if (fmt == '.') {
			goto end;
		}


		if (fmt == 'l') {
			fmt = *format++;

			if (fmt == '\0') {
				goto end;
			}

			if (fmt == 'l') {
				flags |= FLAG_64BIT;
				fmt = *format++;

				if (fmt == '\0') {
					goto end;
				}
			}
		}

		if (fmt == 'z') {
			fmt = *format++;

			if (fmt == '\0') {
				goto end;
			}

			if (sizeof(void *) == sizeof(u64)) {
				flags |= FLAG_64BIT;
			}
		}

		number = 0;

		switch (fmt) {
			case 's': {
				s = va_arg(ap, char *);

				if (s == NULL) {
					s = "(null)";
				}

				while (*s != '\0') {
					lib_putch(*s++);
					++i;
				}

				break;
			}

			case 'c': {
				c = (char)va_arg(ap, int);
				lib_putch(c);
				i++;

				break;
			}

			case 'X':
				flags |= FLAG_LARGE_DIGITS;
			case 'x':
				flags |= FLAG_HEX;
				goto get_number;

			case 'd':
			case 'i':
				flags |= FLAG_SIGNED;
			case 'u':
				goto get_number;

			case 'p': {
				const void *s = va_arg(ap, void *);
				if (s == NULL) {
					lib_putch('(');
					lib_putch('n');
					lib_putch('i');
					lib_putch('l');
					lib_putch(')');
					i += 5;
					break;
				}

				number = (u64)(size_t)s;
				flags |= (FLAG_ZERO | FLAG_HEX);

				if (sizeof(void *) == sizeof(u64)) {
					flags |= FLAG_64BIT;
				}

				min_number_len = sizeof(void *) * 2U;
				goto handle_number;

				break;
			}

			case '%':
				lib_putch('%');
				i++;
				break;

			default:
				lib_putch('%');
				lib_putch(fmt);
				i += 2;
				break;
		}

		continue;

	get_number:;

		if ((flags & FLAG_64BIT) != 0U) {
			number = va_arg(ap, u64);
		}
		else {
			number = va_arg(ap, u32);
		}

	handle_number:;
		eptr = printf_sprintf_int(buff, number, flags, (int)min_number_len);
		sptr = buff;

		while (sptr != eptr) {
			lib_putch(*sptr++);
			++i;
		}

		continue;
	}

end:
	s = CONSOLE_NORMAL;
	while (*s != '\0') {
		lib_putch(*(s++));
	}

	return i;
}


int lib_printf(const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = lib_vprintf(format, ap);
	va_end(ap);

	return ret;
}


int lib_sprintf(char *out, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = lib_vsprintf(out, format, ap);
	va_end(ap);

	return ret;
}
