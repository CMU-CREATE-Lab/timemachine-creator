#include "Tilestack.h"
#include "SimpleZlib.h"

ResidentTilestack::ResidentTilestack(unsigned int nframes, unsigned int tile_width,
                                     unsigned int tile_height, unsigned int bands_per_pixel,
                                     unsigned int bits_per_band, unsigned int pixel_format,
                                     unsigned int compression_format)
{
  set_nframes(nframes);
  this->tile_width = tile_width;
  this->tile_height = tile_height;
  this->bands_per_pixel = bands_per_pixel;
  this->bits_per_band = bits_per_band;
  this->pixel_format = pixel_format;
  this->compression_format = compression_format;
  tile_size =  tile_width * tile_height * bands_per_pixel * bits_per_band / 8;
  all_pixels.resize(tile_size * nframes);
  for (unsigned i = 0; i < nframes; i++) {
    pixels[i] = &all_pixels[tile_size * i];
  }
}

void Tilestack::write(Writer *w) const {
  unsigned long long filepos = 0;

  // Write compressed with zlib

  std::vector<unsigned char> header(8);
  write_u64(&header[0], 0x326b7473656c6974LL); // ASCII 'tilestk2'
  w->write(header);
  filepos += header.size();

  std::vector<unsigned char> buf;
  unsigned flush_threshold = 24 * 1024 * 1024; // Buffer ~24 MB before flushing

  std::vector<unsigned char> compressed_frame;

  std::vector<TOCEntry> write_toc(nframes);

  for (unsigned i = 0; i < nframes; i++) {
    frame_pixels(i);
    Zlib::compress(compressed_frame, pixels[i], bytes_per_frame());
    buf.insert(buf.end(), compressed_frame.begin(), compressed_frame.end());
    write_toc[i].timestamp = toc[i].timestamp;
    write_toc[i].address = filepos;
    write_toc[i].length = compressed_frame.size();
    filepos += compressed_frame.size();
    if (buf.size() >= flush_threshold) {
      w->write(buf);
      buf.resize(0);
    }
  }
  w->write(buf);
  buf.resize(0);

  size_t tocentry_size = 24;
  std::vector<unsigned char> tocdata(tocentry_size * nframes);
  for (unsigned i = 0; i < nframes; i++) {
    write_double64(&tocdata[i*tocentry_size +  0], write_toc[i].timestamp);
    write_u64(&tocdata[i*tocentry_size +  8], write_toc[i].address);
    write_u64(&tocdata[i*tocentry_size + 16], write_toc[i].length);
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
  write_u32(&footer[36], ZLIB_COMPRESSION);
  write_u64(&footer[40], 0x646e65326b747374LL); // ASCII: 'tstk2end'
  w->write(footer);
}

// Old:  write uncompressed pixels
// void ResidentTilestack::write(Writer *w) {
//   std::vector<unsigned char> header(8);
//   write_u64(&header[0], 0x326b7473656c6974LL); // ASCII 'tilestk2'
//   w->write(header);
//   w->write(all_pixels);
//   size_t tocentry_size = 24;
//   std::vector<unsigned char> tocdata(tocentry_size * nframes);
//   size_t address = header.size();
//   for (unsigned i = 0; i < nframes; i++) {
//     write_double64(&tocdata[i*tocentry_size +  0], toc[i].timestamp);
//     write_u64(&tocdata[i*tocentry_size +  8], address);
//     write_u64(&tocdata[i*tocentry_size + 16], tile_size);
//     address += tile_size;
//   }
//   w->write(tocdata);
//   size_t footer_size = 48;
//   std::vector<unsigned char> footer(footer_size);
// 
//   write_u64(&footer[ 0], nframes);
//   write_u64(&footer[ 8], tile_width);
//   write_u64(&footer[16], tile_height);
//   write_u32(&footer[24], bands_per_pixel);
//   write_u32(&footer[28], bits_per_band);
//   write_u32(&footer[32], pixel_format);
//   write_u32(&footer[36], compression_format);
//   write_u64(&footer[40], 0x646e65326b747374LL); // ASCII: 'tstk2end'
//   w->write(footer);
// }

void ResidentTilestack::instantiate_pixels(unsigned frame) const {
  assert(0); // we instantiate all the pixels in our constructor
}
