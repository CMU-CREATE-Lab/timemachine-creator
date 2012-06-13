#include <assert.h>

#include <stdexcept>
#include <vector>

#include "marshal.h"
#include "cpp_utils.h"

#include "ImageWriter.h"

ImageWriter::ImageWriter(int width, int height, int bands_per_pixel, int bits_per_band) :
  m_width(width), m_height(height), m_bands_per_pixel(bands_per_pixel), m_bits_per_band(bits_per_band) {}

std::auto_ptr<ImageWriter> ImageWriter::open(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band) {
  std::string format = filename_suffix(filename);
  if (iequals(format, "kro")) return std::auto_ptr<ImageWriter>(new KroWriter(filename, width, height, bands_per_pixel, bits_per_band));
  //if (iequals(format, "jpg")) return auto_ptr<ImageWriter>(new JpegWriter(filename));
  throw_error("Unrecognized image format from %s", filename.c_str());
  assert(0);
}

void ImageWriter::write(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band, unsigned char *pixels) {
  std::auto_ptr<ImageWriter> writer(open(filename, width, height, bands_per_pixel, bits_per_band));
  writer->write_rows(pixels, height);
  writer->close();
}

KroWriter::KroWriter(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band) :
  ImageWriter(width, height, bands_per_pixel, bits_per_band), filename(filename) {
  out = fopen(filename.c_str(), "wb");
  if (!out) throw_error("Can't open %s for writing", filename.c_str());
  setvbuf(out, NULL, _IOFBF, 1024*1024); // increase buffering to 1MB
  unsigned char header[20];
  write_u32_be(&header[0], 0x4b52f401); // KRO\001
  write_u32_be(&header[4], width);
  write_u32_be(&header[8], height);
  write_u32_be(&header[12], bits_per_band);
  write_u32_be(&header[16], bands_per_pixel);
  if (1 != fwrite(header, sizeof(header), 1, out)) {
    throw_error("Can't write header to %s", filename.c_str());
  }
}

void KroWriter::write_rows(const unsigned char *pixels, unsigned int nrows) const {
  if (1 != fwrite(pixels, nrows * bytes_per_row(), 1, out)) {
    throw_error("Error writing pixels to %s", filename.c_str());
  }
}

void KroWriter::close() {
  if (out) {
    fclose(out);
    out = NULL;
  }
}

KroWriter::~KroWriter() {
  close();
}


//JpegWriter::JpegWriter(const char *filename) : filename(filename) {
//  in = fopen(filename, "rb");
//  if (!in) throw runtime_error(string_printf("Can't open %s for writeing", filename));
//
//  cinfo.err = jpeg_std_error(&jerr);
//  jpeg_create_decompress(&cinfo);
//  jpeg_stdio_src(&cinfo, in);
//  jpeg_write_header(&cinfo, TRUE);
//  jpeg_start_decompress(&cinfo);
//  bands_per_pixel = cinfo.output_components;
//  bits_per_band = 8;
//  width = cinfo.output_width;
//  height = cinfo.output_height;
//}
//
//void JpegWriter::write_rows(const unsigned char *rgb_pixels, unsigned int nrows) const {
//  vector<unsigned char*> rowptrs(nrows);
//  for (unsigned i = 0; i < nrows; i++) {
//    rowptrs[i] = rgb_pixels + i * bytes_per_row();
//  }
//  unsigned nwrite = 0;
//  while (nwrite < nrows) {
//    nwrite += jpeg_write_scanlines(&cinfo, &rowptrs[nwrite], nrows - nwrite);
//  }
//}
//
//void JpegWriter::close() {
//  if (in) {
//    jpeg_finish_decompress(&cinfo);
//    jpeg_destroy_decompress(&cinfo);
//    fclose(in);
//    in = NULL;
//  }
//}
//
//JpegWriter::~JpegWriter() {
//  close();
//}
