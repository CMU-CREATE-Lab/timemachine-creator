#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include <png.h>

#include "png_util.h"

using namespace std;

namespace {
  void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\nAborting\n");
    exit(1);
  }
};

void write_png(const char *filename,
               unsigned width, unsigned height,
               unsigned bands_per_pixel, unsigned bit_depth,
               unsigned char *pixels)
{
  png_byte color_type = 0;
  switch (bands_per_pixel) {
  case 3: color_type = PNG_COLOR_TYPE_RGB; break;
  case 4: color_type = PNG_COLOR_TYPE_RGBA; break;
  default: die("Don't know how to create PNG file with %d bands", bands_per_pixel); break;
  }

  if (bit_depth != 8 && bit_depth != 16) {
    die("Don't know how to create PNG file with bit depth %d", bit_depth);
  }

  FILE *out = fopen(filename, "wb");
  if (!out) {
    die("Can't open %s for writing\n", filename);
  }

  png_structp p_str = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  assert(p_str);
  png_infop p_info = png_create_info_struct(p_str);
  assert(p_info);
  png_init_io(p_str, out);
  png_set_IHDR(p_str, p_info, width, height,
               bit_depth, color_type, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  png_write_info(p_str, p_info);

  vector<unsigned char*> row_pointers(height);
  for (unsigned y = 0; y < height; y++) {
    row_pointers[y] = pixels + y * (width * bands_per_pixel * bit_depth / 8);
  }

  png_write_image(p_str, &row_pointers[0]);
  png_write_end(p_str, NULL);
  fclose(out);
}
