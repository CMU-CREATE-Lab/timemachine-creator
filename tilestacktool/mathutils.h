#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <algorithm>
#include <cmath>

template <class T>
T limit(T x, T minval, T maxval) {
  return std::max(std::min(x, maxval), minval);
}

inline int iround(double x) {
  return (x > 0.0) ? (int)floor(x + 0.5) : (int)ceil(x - 0.5);
}

inline long lround(double x) {
  return (x > 0.0) ? (long)floor(x + 0.5) : (long)ceil(x - 0.5);
}

#endif
