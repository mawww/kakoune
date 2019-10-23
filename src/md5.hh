#ifndef md5_hh_INCLUDED
#define md5_hh_INCLUDED

// Implementation of MD5 hash function. Originally written by Alexander
// Peslyak. Modified by WaterJuice, adapted by Jean-Louis Fuchs for kakoune
// retaining Public Domain license.
//
// This is free and unencumbered software released into the public domain

#include <stdint.h>
#include <stdio.h>

namespace Kakoune {

namespace md5 {

typedef struct {
  uint32_t lo;
  uint32_t hi;
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint8_t buffer[64];
  uint32_t block[16];
} Context;

#define MD5_HASH_SIZE (128 / 8)
#define MD5_HEX_SIZE MD5_HASH_SIZE * 2 + 1

typedef struct {
  uint8_t bytes[MD5_HASH_SIZE];
} Hash;

void initialise(Context *context);

void update(Context *context, void const *buffer, uint32_t buffer_size);

void finalise(Context *context, Hash *digest);

void calculate(void const *buffer, uint32_t buffer_size, Hash *digest);

void hash_to_hex(Hash digest, char* str, size_t str_size);

}

}
#endif // md5_hh_INCLUDED
