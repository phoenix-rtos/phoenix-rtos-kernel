#include "hal/types.h"

typedef u32 crc32_t;

#define LIB_CRC32_INIT 0xffffffff


extern crc32_t lib_crc32NextByte(crc32_t crc, u8 byte);


extern crc32_t lib_crc32Finalize(crc32_t crc);


typedef struct {
	u32 buf;
	int bits;
	char outBuf[3];
} lib_base64_ctx;


extern void lib_base64Init(lib_base64_ctx *ctx);


extern size_t lib_base64EncodeByte(lib_base64_ctx *ctx, const u8 byte);


extern size_t lib_base64Finalize(lib_base64_ctx *ctx);
