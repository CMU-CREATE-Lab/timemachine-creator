#ifndef IMAGE_WRITER_H
#define IMAGE_WRITER_H

#include <memory>
#include <stdio.h>
#include <string>

#include <jpeglib.h>

class ImageWriter {
 protected:
  int m_width;
  int m_height;
  int m_bands_per_pixel;
  int m_bits_per_band;
 public:
  ImageWriter(int width, int height, int bands_per_pixel, int bits_per_band);
  int width() const { return m_width; }
  int height() const { return m_height; }
  int bands_per_pixel() const { return m_bands_per_pixel; }
  int bits_per_band() const { return m_bits_per_band; }
  int bytes_per_pixel() const { return m_bands_per_pixel * m_bits_per_band / 8; }
  int bytes_per_row() const { return bytes_per_pixel() * m_width; }

  virtual void write_rows(const unsigned char *pixels, unsigned int nrows) const = 0;
  virtual void close() = 0;
  virtual ~ImageWriter() {}
  static std::auto_ptr<ImageWriter> open(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band);
  static void write(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band, unsigned char *pixels);
};

class KroWriter : public ImageWriter {
  FILE *out;
  std::string filename;
  
 public:
  KroWriter(const std::string &filename, int width, int height, int bands_per_pixel, int bits_per_band);
  virtual void write_rows(const unsigned char *pixels, unsigned int nrows) const;
  virtual void close();
  virtual ~KroWriter();
};

//class JpegWriter : public ImageWriter {
//  FILE *in;
//  std::string filename;
//  mutable struct jpeg_decompress_struct cinfo;
//  struct jpeg_error_mgr jerr;
//  
// public:
//  JpegWriter(const char *filename);
//  virtual void write_rows(unsigned char *rgb_pixels, unsigned int nrows) const;
//  virtual void close();
//  virtual ~JpegWriter();
//};


#endif
