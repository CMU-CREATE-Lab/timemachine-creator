#ifndef FRAME_H
#define FRAME_H

#include "Bbox.h"

struct Frame {
  double frameno;
  Bbox bounds;
  Frame(double frameno, const Bbox &bounds) : frameno(frameno), bounds(bounds) {}
  Frame() : frameno(-1) {}
};

#endif
