#include <assert.h>

#include <stdexcept>
#include <vector>

#include "utils.h"
#include "marshal.h"

#include "ImageReader.h"

using namespace std;

////// ImageReader

auto_ptr<ImageReader> ImageReader::open(const string &filename) {
  string format = filename_suffix(filename);
  if (iequals(format, "jpg")) return auto_ptr<ImageReader>(new JpegReader(filename));
  if (iequals(format, "kro")) return auto_ptr<ImageReader>(new KroReader(filename));
  throw_error("Unrecognized image format from filename %s", filename.c_str());
}

////// JpegReader

JpegReader::JpegReader(const string &filename) : filename(filename) {
  in = fopen(filename.c_str(), "rb");
  if (!in) throw_error("Can't open %s for reading", filename.c_str());

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, in);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  m_bands_per_pixel = cinfo.output_components;
  m_bits_per_band = 8;
  m_width = cinfo.output_width;
  m_height = cinfo.output_height;
}

void JpegReader::read_rows(unsigned char *pixels, unsigned int nrows) const {
  vector<unsigned char*> rowptrs(nrows);
  for (unsigned i = 0; i < nrows; i++) {
    rowptrs[i] = pixels + i * bytes_per_row();
  }
  unsigned nread = 0;
  while (nread < nrows) {
    nread += jpeg_read_scanlines(&cinfo, &rowptrs[nread], nrows - nread);
  }
}

void JpegReader::close() {
  if (in) {
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(in);
    in = NULL;
  }
}

JpegReader::~JpegReader() {
  close();
}

////// KroReader

KroReader::KroReader(const string &filename) : filename(filename) {
  in = fopen(filename.c_str(), "rb");
  if (!in) throw_error("Can't open %s for reading", filename.c_str());

  unsigned char header[20];
  if (1 != fread(header, sizeof(header), 1, in)) {
    throw_error("Can't read header from %s", filename.c_str());
  }
  unsigned int magic = read_u32_be(&header[0]);
  if (magic != 0x4b52f401) {
    throw_error("Incorrect magic number for KRO file: 0x%x", magic);
  }
  m_width = read_u32_be(&header[4]);
  m_height = read_u32_be(&header[8]);
  m_bits_per_band = read_u32_be(&header[12]);
  m_bands_per_pixel = read_u32_be(&header[16]); 
}

void KroReader::read_rows(unsigned char *pixels, unsigned int nrows) const {
  if (1 != fread(pixels, bytes_per_row() * nrows, 1, in)) {
    throw_error("Can't read pixels from %s", filename.c_str());
  }
}

void KroReader::close() {
  if (in) {
    fclose(in);
    in = NULL;
  }
}

KroReader::~KroReader() {
  close();
}
