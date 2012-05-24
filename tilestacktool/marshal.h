#ifndef MARSHAL_H
#define MARSHAL_H

#include <string.h>

inline void write_u32(unsigned char *dest, unsigned int src) {
  dest[0] = src >>  0;
  dest[1] = src >>  8;
  dest[2] = src >> 16;
  dest[3] = src >> 24;
}

inline void write_u64(unsigned char *dest, unsigned long long src) {
  write_u32(dest + 0, (unsigned int) src);
  write_u32(dest + 4, (unsigned int) (src >> 32));
}

inline void write_double64(unsigned char *dest, double src) {
  unsigned long long u64;
  assert(sizeof(src) == sizeof(u64));
  memcpy(&u64, &src, sizeof(dest));
  write_u64(dest, u64);
}

inline unsigned int read_u32(unsigned char *src) {
  return (src[0]<<0) + (src[1]<<8) + (src[2]<<16) + (src[3]<<24);
}

inline unsigned long long read_u64(unsigned char *src) {
  return read_u32(src) + (((unsigned long long)read_u32(src + 4)) << 32);
}

inline double read_double_64(unsigned char *src) {
  unsigned long long u64 = read_u64(src);
  double d;
  assert(sizeof(u64) == sizeof(d));
  memcpy(&d, &u64, sizeof(d));

  return d;
}

inline void write_u32_be(unsigned char *dest, unsigned int src) {
  dest[0] = src >> 24;
  dest[1] = src >> 16;
  dest[2] = src >>  8;
  dest[3] = src >>  0;
}

inline unsigned int read_u32_be(unsigned char *src) {
  return (src[0]<<24) + (src[1]<<16) + (src[2]<<8) + (src[3]<<0);
}

#endif
