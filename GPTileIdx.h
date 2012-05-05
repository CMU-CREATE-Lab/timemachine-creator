#ifndef GP_TILE_IDX_H
#define GP_TILE_IDX_H

#include <string>

class GPTileIdx {
public:
  int level, x, y;
  GPTileIdx(int level, int x, int y);
  std::string basename() const;
  std::string path() const;
  bool operator<(const GPTileIdx &rhs) const {
    if (level < rhs.level) return true;
    if (level == rhs.level) {
      if (y < rhs.y) return true;
      if (y == rhs.y) {
	if (x < rhs.x) return true;
      }
    }
    return false;
  }
};

#endif
