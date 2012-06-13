#include "cpp_utils.h"

#include "GPTileIdx.h"

GPTileIdx::GPTileIdx(int level, int x, int y) :
  level(level), x(x), y(y) {
}

std::string GPTileIdx::basename() const {
  std::string ret(level+1, 'r');
  for (int i = 0; i < level; i++) {
    ret[level-i] = '0' + ((x >> i) & 1) + (((y >> i) & 1) << 1);
  }
  return ret;
}

std::string GPTileIdx::path() const {
  if (x < 0 || y < 0 || x >= (1<<level) || y >= (1<<level)) {
    throw_error("Coordinate out of bounds for GPTileIdx");
  }
  std::string bn = basename();
  std::string prefix;
  for (unsigned i = 0; i+3 < bn.size(); i += 3) {
    prefix += bn.substr(i, 3) + "/";
  }
  return prefix + bn;
}

bool GPTileIdx::operator<(const GPTileIdx &rhs) const {
  if (level < rhs.level) return true;
  if (level == rhs.level) {
    if (y < rhs.y) return true;
    if (y == rhs.y) {
      if (x < rhs.x) return true;
    }
  }
  return false;
}

std::string GPTileIdx::to_string() const {
  return string_printf("[GPTileIdx l=%d x=%d y=%d %s]",
		       level, x, y, path().c_str());
}
