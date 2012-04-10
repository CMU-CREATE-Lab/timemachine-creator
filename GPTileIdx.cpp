#include "GPTileIdx.h"

using namespace std;

GPTileIdx::GPTileIdx(int level, int x, int y) :
  level(level), x(x), y(y) {
}

string GPTileIdx::basename() const {
  string ret(level+1, 'r');
  for (int i = 0; i < level; i++) {
    ret[level-i] = '0' + ((x >> i) & 1) + (((y >> i) & 1) << 1);
  }
  return ret;
}

string GPTileIdx::path() const {
  string bn = basename();
  string prefix;
  for (unsigned i = 0; i+3 < bn.size(); i += 3) {
    prefix += bn.substr(i, 3) + "/";
  }
  return prefix + bn;
}


