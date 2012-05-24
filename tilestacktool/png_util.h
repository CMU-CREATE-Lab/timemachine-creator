#ifndef PNG_UTIL_H
#define PNG_UTIL_H

void write_png(const char *filename,
               unsigned width, unsigned height,
               unsigned bands_per_pixel, unsigned bit_depth,
               unsigned char *pixels);

#endif

