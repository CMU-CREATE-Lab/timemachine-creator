#ifndef GP_TILE_IDX_H
#define GP_TILE_IDX_H

#include <string>

class GPTileIdx {
public:
  int level, x, y;
  GPTileIdx(int level, int x, int y);
  std::string basename() const;
  std::string path() const;
};

#endif
