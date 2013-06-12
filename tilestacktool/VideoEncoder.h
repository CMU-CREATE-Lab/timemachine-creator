#ifndef INCLUDE_VIDEO_ENCODER_H
#define INCLUDE_VIDEO_ENCODER_H

#include <stdio.h>

class VideoEncoder {
public:
  virtual void write_pixels(unsigned char *pixels, size_t len) = 0;
  virtual void close() = 0;
};

#endif
