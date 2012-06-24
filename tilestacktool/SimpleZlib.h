#ifndef SIMPLE_ZLIB_H
#define SIMPLE_ZLIB_H

#include <vector>

class Zlib {
 public:
  static bool compress(std::vector<unsigned char> &dest, const unsigned char *src, size_t src_len);
  static bool uncompress(std::vector<unsigned char> &dest, const unsigned char *src, size_t src_len);
};

#endif
