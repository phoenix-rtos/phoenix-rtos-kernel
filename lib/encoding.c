#include "encoding.h"

#define LIB_CRC32POLY_LE 0xedb88320


crc32_t lib_crc32NextByte(crc32_t crc, u8 byte)
{
	int b;
	crc = (crc ^ (byte & 0xFF));
	for (b = 0; b < 8; b++) {
		crc = (crc >> 1) ^ ((crc & 1) ? LIB_CRC32POLY_LE : 0);
	}
	return crc;
}


crc32_t lib_crc32Finalize(crc32_t crc)
{
	return ~crc;
}


static const char base64_table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


void lib_base64Init(lib_base64_ctx *ctx)
{
	ctx->buf = 0;
	ctx->bits = 0;
}


size_t lib_base64EncodeByte(lib_base64_ctx *ctx, const u8 byte)
{
	int i = 0;
	ctx->buf <<= 8;
	ctx->buf |= byte;
	ctx->bits += 8;

	while (ctx->bits >= 6) {
		ctx->bits -= 6;
		ctx->outBuf[i++] = base64_table[(ctx->buf >> ctx->bits) & 0x3F];
	}
	return i;
}


size_t lib_base64Finalize(lib_base64_ctx *ctx)
{
	int i = 0;
	if (ctx->bits > 0) {
		ctx->outBuf[i++] = base64_table[(ctx->buf << (6 - ctx->bits)) & 0x3F];
		ctx->outBuf[i++] = '=';
		if (ctx->bits == 2) {
			ctx->outBuf[i++] = '=';
		}
	}
	return i;
}
