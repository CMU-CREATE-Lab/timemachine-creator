#include "Tilestack.h"
#include "SimpleZlib.h"

ResidentTilestack::ResidentTilestack(const TilestackInfo &ti)
{
  *((TilestackInfo*)this) = ti;
  set_nframes(nframes);
  size_t size = bytes_per_frame() * nframes;
  try {
    if (size / 1e9 > 0.5) fprintf(stderr, "Allocating %f GB in ResidentTilestack\n", size / 1e9);
    all_pixels.resize(size);
  } catch (std::bad_alloc) {
    fprintf(stderr, "Ran out of memory trying to allocate ResidentTilestack of dimensions width:%d x height:%d x nframes:%d (%lld bytes)\n",
            tile_width, tile_height, nframes, (long long) size);
    throw;
  }
  for (unsigned i = 0; i < nframes; i++) {
    pixels[i] = &all_pixels[bytes_per_frame() * i];
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

void ResidentTilestack::instantiate_pixels(unsigned frame) const {
  assert(0); // we instantiate all the pixels in our constructor
}
