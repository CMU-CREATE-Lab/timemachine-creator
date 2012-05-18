#include "Tilestack.h"

ResidentTilestack::ResidentTilestack(unsigned int nframes, unsigned int tile_width,
                                     unsigned int tile_height, unsigned int bands_per_pixel,
                                     unsigned int bits_per_band, unsigned int pixel_format,
                                     unsigned int compression_format)
{
  this->nframes = nframes;
  this->tile_width = tile_width;
  this->tile_height = tile_height;
  this->bands_per_pixel = bands_per_pixel;
  this->bits_per_band = bits_per_band;
  this->pixel_format = pixel_format;
  this->compression_format = compression_format;
  tile_size =  tile_width * tile_height * bands_per_pixel * bits_per_band / 8;
  all_pixels.resize(tile_size * nframes);
  pixels.resize(nframes);
  toc.resize(nframes);
  for (unsigned i = 0; i < nframes; i++) {
    pixels[i] = &all_pixels[tile_size * i];
  }
}

void ResidentTilestack::write(Writer *w) {
  std::vector<unsigned char> header(8);
  write_u64(&header[0], 0x326b7473656c6974); // ASCII 'tilestk2'
  w->write(header);
  w->write(all_pixels);
  size_t tocentry_size = 24;
  std::vector<unsigned char> tocdata(tocentry_size * nframes);
  size_t address = header.size();
  for (unsigned i = 0; i < nframes; i++) {
    write_double64(&tocdata[i*tocentry_size +  0], toc[i].timestamp);
    write_u64(&tocdata[i*tocentry_size +  8], address);
    write_u64(&tocdata[i*tocentry_size + 16], tile_size);
    address += tile_size;
  }
  w->write(tocdata);
  size_t footer_size = 48;
  std::vector<unsigned char> footer(footer_size);

  write_u64(&footer[ 0], nframes);
  write_u64(&footer[ 8], tile_width);
  write_u64(&footer[16], tile_height);
  write_u32(&footer[24], bands_per_pixel);
  write_u32(&footer[28], bits_per_band);
  write_u32(&footer[32], pixel_format);
  write_u32(&footer[36], compression_format);
  write_u64(&footer[40], 0x646e65326b747374); // ASCII: 'tstk2end'
  w->write(footer);
}

void ResidentTilestack::instantiate_pixels(unsigned frame) {
  assert(0); // we instantiate all the pixels in our constructor
}
