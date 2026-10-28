#ifndef OCARINA_NDS_YAZ0_H
#define OCARINA_NDS_YAZ0_H

#include <nds.h>
#include <stdbool.h>

u32 yaz0DecompressedSize(const u8 *src, u32 srcLen);
bool yaz0Decompress(const u8 *src, u32 srcLen, u8 *dst, u32 dstLen);

/* Sequential source used by the streamed decoder. Must fill exactly len bytes. */
typedef bool (*Yaz0FillFn)(void *ctx, u8 *dst, u32 len);

/* Decodes while pulling the compressed stream in bufSize chunks, so the caller
 * never needs the whole blob in RAM (SD reads feed it in Phase 1). */
bool yaz0DecompressStreamed(Yaz0FillFn fill, void *ctx, u32 srcLen,
                            u8 *dst, u32 dstLen, u8 *buf, u32 bufSize);

#endif
