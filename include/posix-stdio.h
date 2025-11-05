/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - stdio
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_POSIX_STDIO_H_
#define _PH_POSIX_STDIO_H_


#ifndef SEEK_SET
#define SEEK_SET 0 /* Seek relative to start of file */
#endif

#ifndef SEEK_CUR
#define SEEK_CUR 1 /* Seek relative to current position */
#endif

#ifndef SEEK_END
#define SEEK_END 2 /* Seek relative to end of file */
#endif


#endif
