/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * String helper routines
 *
 * Copyright 2018, 2023 Phoenix Systems
 * Author: Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_LIB_STRUTIL_H_
#define _PH_LIB_STRUTIL_H_


char *lib_strdup(const char *str);


void lib_splitname(char *path, char **base, const char **dir);


#endif /* _LIB_STRUTIL_H_ */
