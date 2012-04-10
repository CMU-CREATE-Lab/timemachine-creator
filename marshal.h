#ifndef MARSHAL_H
#define MARSHAL_H

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
  assert(sizeof(src) == 8);
  assert(sizeof(unsigned long long) == 8);
  write_u64(dest, *(unsigned long long*)&src);
}

inline unsigned int read_u32(unsigned char *src) {
  return (src[0]<<0) + (src[1]<<8) + (src[2]<<16) + (src[3]<<24);
}

inline unsigned long long read_u64(unsigned char *src) {
  return read_u32(src) + (((unsigned long long)read_u32(src + 4)) << 32);
}

inline double read_double_64(unsigned char *src) {
  unsigned long long d = read_u64(src);
  assert(sizeof(d) == 8);
  assert(sizeof(double) == 8);
  return *(double*)&d;
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
