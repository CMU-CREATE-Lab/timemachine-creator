#ifndef GP_TILE_IDX_H
#define GP_TILE_IDX_H

#include <string>

class GPTileIdx {
public:
  int level, x, y;
  GPTileIdx(int level, int x, int y);
  std::string basename() const;
  std::string path() const;
  bool operator<(const GPTileIdx &rhs) const;
  std::string to_string() const;
  static unsigned long long idx(int level, int x, int y) {
    return 
      (((unsigned long long) level)            << 56) |
      (((unsigned long long) (y & 0x0fffffff)) << 28) |
      (((unsigned long long) (x & 0x0fffffff)) <<  0);
  }
};

#endif
