
#ifndef _HAL_ARMV7M_TYPES_H_
#define _HAL_ARMV7M_TYPES_H_

#define NULL 0

#ifndef __ASSEMBLY__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u32 cycles_t;

/* FIXME: offs_t should be eradicated */
typedef s64 offs_t;
typedef offs_t off_t;

typedef unsigned int size_t;
typedef unsigned long long time_t;

typedef u32 ptr_t;

/* Object identifier - contains server port and object id */
typedef u32 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;

#endif

#endif
