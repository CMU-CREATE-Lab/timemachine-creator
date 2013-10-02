#ifndef BBOX_H
#define BBOX_H

#include <math.h>
#include <string>

#include "cpp_utils.h"
#include "math_utils.h"

struct Bbox {
  double x, y;
  double width, height;
  Bbox(double x, double y, double width, double height) : x(x), y(y), width(width), height(height) {}
  Bbox() : x(0), y(0), width(0), height(0) {}
  Bbox operator*(double scale) {
    return Bbox(x * scale, y * scale, width * scale, height * scale);
  }
  Bbox operator/(double denom) {
    return Bbox(x / denom, y / denom, width / denom, height / denom);
  }
  Bbox operator+(const Bbox &rhs) {
    return Bbox(x + rhs.x, y + rhs.y, width + rhs.width, height + rhs.height);
  }
  std::string to_string() {
    return string_printf("[bbox x=%g y=%g width=%g height=%g]",
                         x, y, width, height);
  }

  static Bbox scaled_interpolate(double t, double t0, Bbox f0, double t1, Bbox f1) {
    double timeRatio = (t - t0) / (t1 - t0);
    double s1_over_s0 = f1.width / f0.width;
    
    // Compute f(t), but check whether we're merely panning, in which case we shouldn't attempt to do the                                                                         // special scaling (because it'll blow up with f(1) being NaN since we'd be dividing by zero by zero).                                                                                                                                                             
    double f_of_t = (fabs(s1_over_s0 - 1) < 1e-4) ? timeRatio : (pow(s1_over_s0, timeRatio) - 1) / (s1_over_s0 - 1);

    return Bbox(interpolate(f_of_t, 0, f0.x, 1, f1.x),
                interpolate(f_of_t, 0, f0.y, 1, f1.y),
                interpolate(f_of_t, 0, f0.width, 1, f1.width),
                interpolate(f_of_t, 0, f0.height, 1, f1.height));
  }
};

#endif
