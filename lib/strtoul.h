/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - ASCII to unsigned integer conversion
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIB_STROUL_H_
#define _LIB_STROUL_H_


extern unsigned long lib_strtoul(char *nptr, char **endptr, int base);


extern long lib_strtol(char *nptr, char **endptr, int base);


#endif
