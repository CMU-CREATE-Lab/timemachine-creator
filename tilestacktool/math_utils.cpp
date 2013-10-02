#include "math_utils.h"

double interpolate(double x, double x0, double y0, double x1, double y1) {
  return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

