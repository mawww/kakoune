// Implementation of MD5 hash function. Originally written by Alexander
// Peslyak. Modified by WaterJuice, adapted by Jean-Louis Fuchs for kakoune
// retaining Public Domain license.
//
// This is free and unencumbered software released into the public domain

#include "assert.hh"
#include "md5.hh"

#include <memory.h>
#include <stdio.h>

namespace Kakoune {

namespace md5 {

#define F(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z) ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))

#define STEP(f, a, b, c, d, x, t, s)                                           \
  (a) += f((b), (c), (d)) + (x) + (t);                                         \
  (a) = (((a) << (s)) | (((a)&0xffffffff) >> (32 - (s))));                     \
  (a) += (b);

static void *TransformFunction(Context *ctx, void const *data,
                               uintmax_t size) {
  uint8_t *ptr;
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t saved_a;
  uint32_t saved_b;
  uint32_t saved_c;
  uint32_t saved_d;

#define GET(n) (ctx->block[(n)])
#define SET(n)                                                                 \
  (ctx->block[(n)] =                                                           \
       ((uint32_t)ptr[(n)*4 + 0] << 0) | ((uint32_t)ptr[(n)*4 + 1] << 8) |     \
       ((uint32_t)ptr[(n)*4 + 2] << 16) | ((uint32_t)ptr[(n)*4 + 3] << 24))

  ptr = (uint8_t *)data;

  a = ctx->a;
  b = ctx->b;
  c = ctx->c;
  d = ctx->d;

  do {
    saved_a = a;
    saved_b = b;
    saved_c = c;
    saved_d = d;

    // Round 1
    STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
    STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
    STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
    STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
    STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
    STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
    STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
    STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
    STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
    STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
    STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
    STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
    STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
    STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
    STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
    STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

    // Round 2
    STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
    STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
    STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
    STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
    STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
    STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
    STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
    STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
    STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
    STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
    STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
    STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
    STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
    STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
    STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
    STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

    // Round 3
    STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
    STEP(H, d, a, b, c, GET(8), 0x8771f681, 11)
    STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
    STEP(H, b, c, d, a, GET(14), 0xfde5380c, 23)
    STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
    STEP(H, d, a, b, c, GET(4), 0x4bdecfa9, 11)
    STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
    STEP(H, b, c, d, a, GET(10), 0xbebfbc70, 23)
    STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
    STEP(H, d, a, b, c, GET(0), 0xeaa127fa, 11)
    STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
    STEP(H, b, c, d, a, GET(6), 0x04881d05, 23)
    STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
    STEP(H, d, a, b, c, GET(12), 0xe6db99e5, 11)
    STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
    STEP(H, b, c, d, a, GET(2), 0xc4ac5665, 23)

    // Round 4
    STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
    STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
    STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
    STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
    STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
    STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
    STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
    STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
    STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
    STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
    STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
    STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
    STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
    STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
    STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
    STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

    a += saved_a;
    b += saved_b;
    c += saved_c;
    d += saved_d;

    ptr += 64;
  } while (size -= 64);

  ctx->a = a;
  ctx->b = b;
  ctx->c = c;
  ctx->d = d;

  return ptr;
}

void initialise(Context *context) {
  context->a = 0x67452301;
  context->b = 0xefcdab89;
  context->c = 0x98badcfe;
  context->d = 0x10325476;

  context->lo = 0;
  context->hi = 0;
}

void update(Context *context, void const *buffer, uint32_t buffer_size) {
  uint32_t saved_lo;
  uint32_t used;
  uint32_t free;

  saved_lo = context->lo;
  if ((context->lo = (saved_lo + buffer_size) & 0x1fffffff) < saved_lo) {
    context->hi++;
  }
  context->hi += (uint32_t)(buffer_size >> 29);

  used = saved_lo & 0x3f;

  if (used) {
    free = 64 - used;

    if (buffer_size < free) {
      memcpy(&context->buffer[used], buffer, buffer_size);
      return;
    }

    memcpy(&context->buffer[used], buffer, free);
    buffer = (uint8_t *)buffer + free;
    buffer_size -= free;
    TransformFunction(context, context->buffer, 64);
  }

  if (buffer_size >= 64) {
    buffer =
        TransformFunction(context, buffer, buffer_size & ~(unsigned long)0x3f);
    buffer_size &= 0x3f;
  }

  memcpy(context->buffer, buffer, buffer_size);
}

void finalise(Context *context, Hash *digest) {
  uint32_t used;
  uint32_t free;

  used = context->lo & 0x3f;

  context->buffer[used++] = 0x80;

  free = 64 - used;

  if (free < 8) {
    memset(&context->buffer[used], 0, free);
    TransformFunction(context, context->buffer, 64);
    used = 0;
    free = 64;
  }

  memset(&context->buffer[used], 0, free - 8);

  context->lo <<= 3;
  context->buffer[56] = (uint8_t)(context->lo);
  context->buffer[57] = (uint8_t)(context->lo >> 8);
  context->buffer[58] = (uint8_t)(context->lo >> 16);
  context->buffer[59] = (uint8_t)(context->lo >> 24);
  context->buffer[60] = (uint8_t)(context->hi);
  context->buffer[61] = (uint8_t)(context->hi >> 8);
  context->buffer[62] = (uint8_t)(context->hi >> 16);
  context->buffer[63] = (uint8_t)(context->hi >> 24);

  TransformFunction(context, context->buffer, 64);

  digest->bytes[0] = (uint8_t)(context->a);
  digest->bytes[1] = (uint8_t)(context->a >> 8);
  digest->bytes[2] = (uint8_t)(context->a >> 16);
  digest->bytes[3] = (uint8_t)(context->a >> 24);
  digest->bytes[4] = (uint8_t)(context->b);
  digest->bytes[5] = (uint8_t)(context->b >> 8);
  digest->bytes[6] = (uint8_t)(context->b >> 16);
  digest->bytes[7] = (uint8_t)(context->b >> 24);
  digest->bytes[8] = (uint8_t)(context->c);
  digest->bytes[9] = (uint8_t)(context->c >> 8);
  digest->bytes[10] = (uint8_t)(context->c >> 16);
  digest->bytes[11] = (uint8_t)(context->c >> 24);
  digest->bytes[12] = (uint8_t)(context->d);
  digest->bytes[13] = (uint8_t)(context->d >> 8);
  digest->bytes[14] = (uint8_t)(context->d >> 16);
  digest->bytes[15] = (uint8_t)(context->d >> 24);
}

void calculate(void const *buffer, uint32_t buffer_size, Hash *digest) {
  Context context;

  initialise(&context);
  update(&context, buffer, buffer_size);
  finalise(&context, digest);
}

void hash_to_hex(Hash digest, char* str, size_t str_size) {
    size_t i;
    kak_assert(MD5_HEX_SIZE <= str_size);
    for (i = 0; i < MD5_HASH_SIZE; i++) {
        snprintf(str, 3, "%02X", digest.bytes[i]);
        str += 2;
    }
    *str = 0;
}

}

}
