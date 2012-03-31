#include <jpeglib.h>

// UNTESTED CODE!
#if 0
void write_jpeg(string filename, unsigned char *rgb_pixels, int width, int height) {
  FILE *out = fopen(filename.c_str(), "w");
  struct jpeg_compress_struct cinfo;
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, out);
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 95, TRUE);
  jpeg_start_compress(&cinfo, TRUE);
  jpeg_write_scanlines(&cinfo, rgb_pixels, height);
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(out);
}

#endif
