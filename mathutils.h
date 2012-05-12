#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <algorithm>
#include <cmath>

template <class T>
T limit(T x, T minval, T maxval) {
  return std::max(std::min(x, maxval), minval);
}

inline int iround(double x) {
  return (x > 0.0) ? floor(x + 0.5) : ceil(x - 0.5);
}

#endif
