#ifndef IMAGE_READER_H
#define IMAGE_READER_H

#include <memory>
#include <stdio.h>
#include <string>

#include <jpeglib.h>

class ImageReader {
 protected:
  int m_width;
  int m_height;
  int m_bands_per_pixel;
  int m_bits_per_band;
 public:
  unsigned int width() const { return m_width; }
  unsigned int height() const { return m_height; }
  unsigned int bands_per_pixel() const { return m_bands_per_pixel; }
  unsigned int bits_per_band() const { return m_bits_per_band; }
  unsigned int bytes_per_pixel() const { return m_bands_per_pixel * m_bits_per_band / 8; }
  unsigned int bytes_per_row() const { return bytes_per_pixel() * m_width; }

  virtual void read_rows(unsigned char *pixels, unsigned int nrows) const = 0;
  virtual void close() = 0;
  virtual ~ImageReader() {}
  static std::auto_ptr<ImageReader> open(const std::string &filename);
};

class JpegReader : public ImageReader {
  FILE *in;
  std::string filename;
  mutable struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  
 public:
  JpegReader(const std::string &filename);
  virtual void read_rows(unsigned char *pixels, unsigned int nrows) const;
  virtual void close();
  virtual ~JpegReader();
};

class KroReader : public ImageReader {
  FILE *in;
  std::string filename;
  
 public:
  KroReader(const std::string &filename);
  virtual void read_rows(unsigned char *pixels, unsigned int nrows) const;
  virtual void close();
  virtual ~KroReader();
};


#endif
