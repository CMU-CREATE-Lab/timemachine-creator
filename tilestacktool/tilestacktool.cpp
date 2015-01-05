#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef _WIN32
	#include <unistd.h>
#endif

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "marshal.h"
#include "cpp_utils.h"
#include "png_util.h"
#include "GPTileIdx.h"
#include "ImageReader.h"
#include "ImageWriter.h"

#include "io.h"

#include "mwc.h"
#include "SimpleZlib.h"
#include "Tilestack.h"
#include "tilestacktool.h"
#include "warp.h"
#include "H264Encoder.h"
#include "VP8Encoder.h"
#include "ProresHQEncoder.h"

#define TODO(x) do { fprintf(stderr, "%s:%d: error: TODO %s\n", __FILE__, __LINE__, x); abort(); } while (0)
const double PI = 4.0*atan(1.0);

unsigned int tilesize = 512;
bool create_parent_directories = false;
bool delete_source_tiles = false;
std::vector<std::string> source_tiles_to_delete;
std::string render_js_path_override;

void usage(const char *fmt, ...);

const char *version() { return "0.3.3"; }

class LRUTilestack : public Tilestack {
  unsigned int lru_size;
  mutable std::list<int> lru;

public:
  LRUTilestack() : lru_size(5) {}

  // Not really deleting the LRU, but rather the least recently created
  // (to avoid the overhead of recording use)
  void delete_lru() const {
    delete[] pixels[lru.back()];
    pixels[lru.back()] = 0;
    lru.pop_back();
  }

  virtual void create(unsigned frame) const {
    while (lru.size() > lru_size) delete_lru();
    lru.push_front(frame);
    pixels[frame] = new unsigned char[bytes_per_frame()];
  }

  virtual ~LRUTilestack() {
    while (!lru.empty()) delete_lru();
  }
};

class TilestackReader : public LRUTilestack {
  static int stacks_read;
  static int compressed_tiles_read;
  static int uncompressed_tiles_read;
public:
  simple_shared_ptr<Reader> reader;

  TilestackReader(simple_shared_ptr<Reader> reader) : reader(reader) {
    read();
    stacks_read++;
  }

  virtual ~TilestackReader() {}

  static std::string stats() {
    int total_tiles_read = compressed_tiles_read + uncompressed_tiles_read;
    std::string stats = "";
    stats += string_printf("Read %d tiles from %d tilestacks.",
                           total_tiles_read, stacks_read);
    if (total_tiles_read) {
      stats += string_printf("  %.1f tiles per tilestack.  %.0f%% tiles compressed.",
                             (double) total_tiles_read / stacks_read,
                             100.0 * compressed_tiles_read / total_tiles_read);
    }
    return stats;
  }

protected:
  virtual void instantiate_pixels(unsigned frame) const {
    //fprintf(stderr, "TileStackReader %llx instantiating frame %d\n", (unsigned long long) this, frame);
    assert(!pixels[frame]);
    create(frame);
    switch (compression_format) {
    case NO_COMPRESSION:
      uncompressed_tiles_read++;
      if (toc[frame].length != bytes_per_frame()) {
        throw_error("TilestackReader: Frame %d has %d bytes, but should have %d bytes",
                    frame, (int) toc[frame].length, (int)bytes_per_frame());
      }
      reader->read(pixels[frame], toc[frame].address, toc[frame].length);
      break;
    case ZLIB_COMPRESSION:
      compressed_tiles_read++;
      {
        std::vector<unsigned char> compressed_frame = reader->read(toc[frame].address, toc[frame].length);
        std::vector<unsigned char> uncompressed_frame;
        Zlib::uncompress(uncompressed_frame, &compressed_frame[0], compressed_frame.size());
        if (uncompressed_frame.size() != bytes_per_frame()) {
          throw_error("TilestackReader: Frame %d has %d bytes, but should have %d bytes",
                      frame, (int) uncompressed_frame.size(), (int)bytes_per_frame());
        }
        std::copy(uncompressed_frame.begin(), uncompressed_frame.end(), pixels[frame]);
      }
      break;
    default:
      throw_error("Unknown compression type in tilestack: %d", compression_format);
      break;
    }
  }

  void read() {
    size_t footer_size = 48;
    size_t filelen = reader->length();
    if (filelen < footer_size) {
      throw_error("Tilestack must be at least %d bytes long", (int)footer_size);
    }
    std::vector<unsigned char> footer = reader->read(filelen - footer_size, footer_size);
    unsigned int tmp_nframes = (unsigned int) read_u64(&footer[ 0]);
    tile_width =   (unsigned int) read_u64(&footer[ 8]);
    tile_height =  (unsigned int) read_u64(&footer[16]);
    bands_per_pixel =             read_u32(&footer[24]);
    bits_per_band =               read_u32(&footer[28]);
    pixel_format =                read_u32(&footer[32]);
    compression_format =          read_u32(&footer[36]);
    unsigned long long magic =    read_u64(&footer[40]);
    if (magic != 0x646e65326b747374LL) {
      throw_error("Tilestack footer has incorrect magic (%08x:%08x)", (unsigned int)(magic >> 32), (unsigned int)magic);
    }
    if (tmp_nframes > 1000000) {
      throw_error("Tilestack appears to have more than 1e6 frames?");
    }
    set_nframes(tmp_nframes);

    size_t tocentry_size = 24;
    size_t toclen = tocentry_size * nframes;
    std::vector<unsigned char> tocdata = reader->read(filelen - footer_size - toclen, toclen);
    for (unsigned i = 0; i < nframes; i++) {
      toc[i].timestamp = read_double_64(&tocdata[i*tocentry_size +  0]);
      toc[i].address =   read_u64      (&tocdata[i*tocentry_size +  8]);
      toc[i].length =    read_u64      (&tocdata[i*tocentry_size + 16]);
    }
  }
};

int TilestackReader::stacks_read;
int TilestackReader::compressed_tiles_read;
int TilestackReader::uncompressed_tiles_read;

AutoPtrStack<Tilestack> tilestackstack;

void load(std::string filename)
{
  simple_shared_ptr<Reader> reader(FileReader::open(filename));
  simple_shared_ptr<Tilestack> tilestack(new TilestackReader(reader));
  tilestackstack.push(tilestack);
}

void loadraw(std::string filename, const TilestackInfo &ti)
{
  // Compute number of frames
  long long size = file_size(filename);
  if (size < 0) throw_error("loadraw: can't access %s", filename.c_str());
  if (size == 0) throw_error("loadraw: %s is zero length", filename.c_str());
  if (size % ti.bytes_per_frame()) {
    throw_error("loadraw: format given has %ld bytes per frame, but file has %ld, which doesn't divide evenly",
                (long) ti.bytes_per_frame(), (long) size);
  }
  TilestackInfo ti_with_nframes = ti;
  ti_with_nframes.nframes = size / ti.bytes_per_frame();
  simple_shared_ptr<Tilestack> tilestack(new ResidentTilestack(ti_with_nframes));
  FILE *in = fopen_utf8(filename, "rb");
  if (!in) throw_error("loadraw: can't open %s for reading", filename.c_str());
  // Assumes frames are contiguous
  int nread = fread(tilestack->frame_pixels(0), ti.bytes_per_frame(), ti_with_nframes.nframes, in);
  if (nread != (int)ti_with_nframes.nframes) throw_error("loadraw: couldn't read %ld bytes (%s)", (long) size,
                                                         strerror(errno));
  fclose(in);
  tilestackstack.push(tilestack);
}

// gamma > 1 brightens midtones; < 1 darkens midtones

class VizBand {
  double one_over_gamma;
  double maxval;
  double scale_over_maxval;

public:
  VizBand(JSON params, unsigned int band) {
    double gamma = get_param(params, "gamma", band, 1);
    one_over_gamma = 1.0 / gamma;
    double scale = get_param(params, "gain", band, 1);
    // To get full and correct dynamic range, maxval should be 256 for 8-bit values (not 255)
    maxval = get_param(params, "maxval", band, 256);
    scale_over_maxval = scale / maxval;
  }

  double get_param(JSON params, const char *name, unsigned int band, double default_val) {
    if (!params.hasKey(name)) {
      return default_val;
    }
    JSON param = params[name];
    if (param.isArray()) {
      return param[band].doub();
    } else {
      return param.doub();
    }
  }

  double apply(double val) const {
    return maxval * pow(std::max(0.0, val * scale_over_maxval), one_over_gamma);
  }
};

class VizTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> src;
  std::vector<VizBand> viz_bands;

public:
  VizTilestack(simple_shared_ptr<Tilestack> src, JSON params) : src(src) {

    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->src.get());
    for (unsigned i = 0; i < bands_per_pixel; i++) {
      viz_bands.push_back(VizBand(params, i));
    }
    set_nframes(this->src->nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    toc[frame].timestamp = src->toc[frame].timestamp;

    unsigned char *srcptr = src->frame_pixels(frame);
    int src_bytes_per_pixel = src->bytes_per_pixel();
    unsigned char *destptr = pixels[frame];
    int dest_bytes_per_pixel = bytes_per_pixel();

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned x = 0; x < tile_width; x++) {
        for (unsigned band = 0; band < bands_per_pixel; band++) {
          set_pixel_band(destptr, band, viz_bands[band].apply(src->get_pixel_band(srcptr, band)));
        }
        srcptr += src_bytes_per_pixel;
        destptr += dest_bytes_per_pixel;
      }
    }
  }
};

void viz(JSON params) {
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  simple_shared_ptr<Tilestack> dest(new VizTilestack(src, params));
  tilestackstack.push(dest);
}

class CastTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> src;

public:
  CastTilestack(simple_shared_ptr<Tilestack> src, int pixel_format, int bits_per_band) : src(src) {
    (*(TilestackInfo*)this) = (*(TilestackInfo*)src.get());
    this->pixel_format = pixel_format;
    this->bits_per_band = bits_per_band;
    set_nframes(nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    toc[frame].timestamp = src->toc[frame].timestamp;

    unsigned char *srcptr = src->frame_pixels(frame);
    int src_bytes_per_pixel = src->bytes_per_pixel();
    unsigned char *destptr = pixels[frame];
    int dest_bytes_per_pixel = bytes_per_pixel();

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned x = 0; x < tile_width; x++) {
        for (unsigned band = 0; band < bands_per_pixel; band++) {
          set_pixel_band(destptr, band, src->get_pixel_band(srcptr, band));
        }
        srcptr += src_bytes_per_pixel;
        destptr += dest_bytes_per_pixel;
      }
    }
  }
};

simple_shared_ptr<Tilestack> ensure_resident(simple_shared_ptr<Tilestack> src) {
  if (dynamic_cast<ResidentTilestack*>(src.get())) {
    // Already resident
    return src;
  } else {
    TilestackInfo ti = *src;
    ti.compression_format = 0;
    simple_shared_ptr<Tilestack> copy(new ResidentTilestack(ti));
    for (unsigned frame = 0; frame < src->nframes; frame++) {
      memcpy(copy->frame_pixels(frame), src->frame_pixels(frame), src->bytes_per_frame());
    }
    return copy;
  }
}

simple_shared_ptr<Tilestack> cast_pixel_format(simple_shared_ptr<Tilestack> src, unsigned int pixel_format, unsigned int bits_per_band) {
  if (src->pixel_format == pixel_format && src->bits_per_band == bits_per_band) {
    return src;
  } else {
    return simple_shared_ptr<Tilestack>(new CastTilestack(src, pixel_format, bits_per_band));
  }
}

struct VecRef {
  float *vals;
  size_t size;
  size_t delta;
  VecRef(float *vals, size_t size, size_t delta=1) : vals(vals), size(size), delta(delta) {}
  VecRef(std::vector<float> &vec) : vals(&vec[0]), size(vec.size()), delta(1) {}
  VecRef slice(size_t begin) {
    return VecRef(vals + (delta * begin), size - begin, delta);
  }
  VecRef slice(size_t begin, size_t end) {
    return VecRef(vals + (delta * begin), end - begin, delta);
  }
  float sum() {
    float sum = 0;
    float *ptr = vals;
    for (size_t i = 0; i < size; i++) {
      sum += *ptr;
      ptr += delta;
    }
    return sum;
  }
};

// Dot product of a and b.  Will use all elements of a.  b must be at least as long as a. if b is longer than a,
// extra elements are ignored
inline float dot_product(const VecRef &a, const VecRef &b) {
  assert(b.size >= a.size);
  float ret = 0.0;
  float *aptr = a.vals, *bptr = b.vals;
  for (size_t i = 0; i < a.size; i++) {
    ret += (*aptr) * (*bptr);
    aptr += a.delta;
    bptr += b.delta;
  }
  return ret;
}

void normalized_convolution(VecRef out, VecRef kernel, VecRef in) {
  assert(out.size == in.size);
  assert(kernel.size % 2 == 1);
  size_t kernel_radius = (kernel.size - 1) / 2;
  size_t i = 0;
  float *outptr = out.vals;

  for (; i < std::min(kernel_radius, out.size); i++) {
    size_t kernel_slice_begin = kernel_radius - i;
    size_t kernel_slice_end = std::min(kernel_slice_begin + in.size, kernel.size);
    VecRef kernel_slice = kernel.slice(kernel_slice_begin, kernel_slice_end);
    *outptr = dot_product(kernel_slice, in) / kernel_slice.sum();
    outptr += out.delta;
  }
  float kernel_sum = kernel.sum();
  for (; i + kernel_radius < out.size; i++) {
    *outptr = dot_product(kernel, in) / kernel_sum;
    in = in.slice(1);
    outptr += out.delta;
  }
  for (; i< out.size; i++) {
    size_t kernel_slice_begin = 0;
    size_t kernel_slice_end = kernel_radius + (out.size - i);
    VecRef kernel_slice = kernel.slice(kernel_slice_begin, kernel_slice_end);
    *outptr = dot_product(kernel_slice, in) / kernel_slice.sum();
    in = in.slice(1);
    outptr += out.delta;
  }
}

class LRUConvolve : public LRUTilestack {
protected:
  simple_shared_ptr<Tilestack> src;
  mutable std::vector<float> kernel;
  virtual void compute(unsigned frame) const = 0;

public:
  LRUConvolve(simple_shared_ptr<Tilestack> src, std::vector<float> kernel) : kernel(kernel) {
    this->src = cast_pixel_format(src, PIXEL_FORMAT_FLOATING_POINT, 32);
    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->src.get());
    set_nframes(nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    toc[frame].timestamp = src->toc[frame].timestamp;
    compute(frame);
  }
};

class HConvolve : public LRUConvolve {
public:
  HConvolve(simple_shared_ptr<Tilestack> src, std::vector<float> kernel) : LRUConvolve(src, kernel) {}

protected:
  virtual void compute(unsigned frame) const {
    VecRef kernel_ref(kernel);

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned band = 0; band < bands_per_pixel; band++) {
        normalized_convolution(VecRef((float*)get_pixel_band_ptr(frame_pixel(frame, 0, y), band), tile_width, bands_per_pixel),
                               kernel_ref,
                               VecRef((float*)get_pixel_band_ptr(src->frame_pixel(frame, 0, y), band), tile_width, bands_per_pixel));
      }
    }
  }
};

class VConvolve : public LRUConvolve {
public:
  VConvolve(simple_shared_ptr<Tilestack> src, std::vector<float> kernel) : LRUConvolve(src, kernel) {}

protected:
  virtual void compute(unsigned frame) const {
    VecRef kernel_ref(kernel);

    for (unsigned x = 0; x < tile_width; x++) {
      for (unsigned band = 0; band < bands_per_pixel; band++) {
        normalized_convolution(VecRef((float*)get_pixel_band_ptr(frame_pixel(frame, x, 0), band), tile_width, bands_per_pixel * tile_height),
                               kernel_ref,
                               VecRef((float*)get_pixel_band_ptr(src->frame_pixel(frame, x, 0), band), tile_width, bands_per_pixel * tile_height));
      }
    }
  }
};

simple_shared_ptr<Tilestack> tconvolve(simple_shared_ptr<Tilestack> src, std::vector<float> kernel) {
  src = ensure_resident(cast_pixel_format(src, PixelInfo::PIXEL_FORMAT_FLOATING_POINT, 32));
  simple_shared_ptr<Tilestack> dest(new ResidentTilestack(*src));

  for (unsigned frame = 0; frame < src->nframes; frame++) {
    dest->toc[frame].timestamp = src->toc[frame].timestamp;
  }

  VecRef kernel_ref(kernel);
  for (unsigned y = 0; y < src->tile_height; y++) {
    for (unsigned x = 0; x < src->tile_width; x++) {
      for (unsigned band = 0; band < src->bands_per_pixel; band++) {
        normalized_convolution(VecRef((float*)dest->get_pixel_band_ptr(dest->frame_pixel(0, x, y), band),
                                      dest->nframes, dest->bands_per_pixel * dest->tile_width * dest->tile_height),
                               kernel_ref,
                               VecRef((float*)src->get_pixel_band_ptr(src->frame_pixel(0, x, y), band),
                                      src->nframes, src->bands_per_pixel * src->tile_width * src->tile_height));
      }
    }
  }

  return dest;
}

std::vector<float> gaussian_kernel(double sigma)
{
  assert(sigma >= 0);
  size_t radius = (size_t)(sigma * 4);
  std::vector<float> kernel(radius * 2 + 1);

  for (size_t i = 0; i <= radius; i++) {
    kernel[radius - i] = kernel[radius + i] = exp(-0.5*(i*i)/(sigma*sigma));
  }

  // Normalize
  double sum = VecRef(kernel).sum();
  for (size_t i = 0; i < kernel.size(); i++) kernel[i] /= sum;

  return kernel;
}

class ConcatenationTilestack : public LRUTilestack {
  std::vector<simple_shared_ptr<Tilestack> > srcs;

public:
  ConcatenationTilestack(std::vector<simple_shared_ptr<Tilestack> > srcs) : srcs(srcs) {
    assert(srcs.size());
    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->srcs[0].get());
    int nframes = 0;
    for (unsigned i = 0; i < srcs.size(); i++) {
      nframes += srcs[i]->nframes;
    }
    set_nframes(nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    unsigned start_frame = 0;
    for (unsigned i = 0; i < srcs.size(); i++) {
      if (frame - start_frame < srcs[i]->nframes) {
        unsigned char *srcptr = srcs[i]->frame_pixels(frame - start_frame);
        memcpy(pixels[frame], srcptr, bytes_per_frame());
        toc[frame].timestamp = srcs[i]->toc[frame - start_frame].timestamp;
        return;
      } else {
        start_frame += srcs[i]->nframes;
      }
    }
    throw_error("Attempt to instantiate pixels beyond end of concatenation tilestack");
  }
};

void cat() {
  std::vector<simple_shared_ptr<Tilestack> > srcs(tilestackstack.size());
  for (int i = srcs.size() - 1; i >= 0; i--) {
    srcs[i] = tilestackstack.pop();
  }
  assert(!tilestackstack.size());
  simple_shared_ptr<Tilestack> concatenation(new ConcatenationTilestack(srcs));
  tilestackstack.push(concatenation);
}

class CompositeTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> base;
  simple_shared_ptr<Tilestack> overlay;

public:
  CompositeTilestack(simple_shared_ptr<Tilestack> &base, simple_shared_ptr<Tilestack> &overlay) :
    base(base), overlay(overlay) {
    (*(TilestackInfo*)this) = (TilestackInfo&)(*base);
    if (base->nframes != overlay->nframes) {
      throw_error("composite: base nframes (%d) is not equal to overlay frames (%d)",
                  base->nframes, overlay->nframes);
    }
    if (base->tile_width != overlay->tile_width ||
        base->tile_height != overlay->tile_height) {
      throw_error("composite: base dimensions (%d,%d) are not equal to overlay dimensions (%d,%d)",
                  base->tile_width, base->tile_height,
                  overlay->tile_width, overlay->tile_height);
    }
    if (base->bands_per_pixel > overlay->bands_per_pixel) {
      throw_error("composite: base bands per pixel (%d) is more than overlay bands per pixel (%d)",
                  base->bands_per_pixel, overlay->bands_per_pixel);
    }
    set_nframes(base->nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    unsigned char *base_pixel = base->frame_pixels(frame);
    unsigned char *overlay_pixel = overlay->frame_pixels(frame);
    unsigned char *composite_pixel = pixels[frame];

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned x = 0; x < tile_width; x++) {
        double alpha = overlay->get_pixel_band(overlay_pixel, overlay->bands_per_pixel-1) / ((1 << overlay->bits_per_band) - 1);
        for (unsigned b = 0; b < bands_per_pixel; b++) {
          set_pixel_band(composite_pixel, b,
                         overlay->get_pixel_band(overlay_pixel, b) * alpha + base->get_pixel_band(base_pixel, b) * (1 - alpha));
        }
        base_pixel += base->bytes_per_pixel();
        overlay_pixel += overlay->bytes_per_pixel();
        composite_pixel += bytes_per_pixel();
      }
    }
  }
};

void composite() {
    simple_shared_ptr<Tilestack> overlay(tilestackstack.pop());
  simple_shared_ptr<Tilestack> base(tilestackstack.pop());

  simple_shared_ptr<Tilestack> composite(new CompositeTilestack(base, overlay));
  tilestackstack.push(composite);
}

class BinopTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> a;
  simple_shared_ptr<Tilestack> b;
  double (*op)(double a, double b);

public:
  BinopTilestack(simple_shared_ptr<Tilestack> &a, simple_shared_ptr<Tilestack> &b, double (*op)(double, double)) :
    a(a), b(b), op(op) {
    (*(TilestackInfo*)this) = (TilestackInfo&)(*a);
    if (a->nframes != a->nframes) {
      throw_error("binop: nframes don't match: %d != %d",
                  a->nframes, a->nframes);
    }
    if (a->tile_width != a->tile_width ||
        a->tile_height != a->tile_height) {
      throw_error("binop: dimensions don't match: (%d,%d) != (%d,%d)",
                  a->tile_width, a->tile_height,
                  b->tile_width, b->tile_height);
    }
    if (a->bands_per_pixel != b->bands_per_pixel) {
      throw_error("binop: bands per pixel don't match: %d != %d",
                  a->bands_per_pixel, b->bands_per_pixel);
    }
    set_nframes(a->nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    unsigned char *a_pixel = a->frame_pixels(frame);
    unsigned char *b_pixel = b->frame_pixels(frame);
    unsigned char *result_pixel = pixels[frame];

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned x = 0; x < tile_width; x++) {
        for (unsigned band = 0; band < bands_per_pixel; band++) {
          set_pixel_band(result_pixel, band,
                         (*op)(a->get_pixel_band(a_pixel, band), b->get_pixel_band(b_pixel, band)));
        }
        a_pixel += a->bytes_per_pixel();
        b_pixel += b->bytes_per_pixel();
        result_pixel += bytes_per_pixel();
      }
    }
  }
};

double subtract_op(double a, double b) { return a-b; }
double add_op(double a, double b) { return a+b; }

void binop(double (*op)(double, double)) {
  simple_shared_ptr<Tilestack> a(tilestackstack.pop());
  simple_shared_ptr<Tilestack> b(tilestackstack.pop());
  simple_shared_ptr<Tilestack> result(new BinopTilestack(a, b, op));
  tilestackstack.push(result);
}

template <class T>
class UnopTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> a;
  T op;

public:
  UnopTilestack(simple_shared_ptr<Tilestack> &a, T op) : a(a), op(op) {
    (*(TilestackInfo*)this) = (TilestackInfo&)(*a);
    set_nframes(a->nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    unsigned char *a_pixel = a->frame_pixels(frame);
    unsigned char *result_pixel = pixels[frame];

    for (unsigned y = 0; y < tile_height; y++) {
      for (unsigned x = 0; x < tile_width; x++) {
        for (unsigned band = 0; band < bands_per_pixel; band++) {
          set_pixel_band(result_pixel, band, op(a->get_pixel_band(a_pixel, band)));
        }
        a_pixel += a->bytes_per_pixel();
        result_pixel += bytes_per_pixel();
      }
    }
  }
};

class scale_unop {
  double scale;
public:
  scale_unop(double scale) : scale(scale) {}
  double operator()(double a) const { return scale*a; }
};

template <class T>
void unop(T op) {
  simple_shared_ptr<Tilestack> a(tilestackstack.pop());
  simple_shared_ptr<Tilestack> result(new UnopTilestack<T>(a, op));
  tilestackstack.push(result);
}

void tilestack_info() {
  simple_shared_ptr<Tilestack> src(tilestackstack.top());
  fprintf(stderr, "Tilestack information: %s\n", src->info().c_str());
}

void write_png(std::string dest)
{
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  std::string dir = filename_sans_suffix(dest);
  make_directory(dir);

  for (unsigned i = 0; i < src->nframes; i++) {
    std::string image_filename = string_printf("%05d.png", i);
    std::string image_path = dir + "/" + image_filename;
    fprintf(stderr, "Writing %s\n", image_filename.c_str());
    write_png(image_path.c_str(), src->tile_width, src->tile_height,
              src->bands_per_pixel, src->bits_per_band, src->frame_pixels(i));
  }
  fprintf(stderr, "Created PNGs\n");
}

void write_html(std::string dest)
{
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  std::string dir = filename_sans_suffix(dest);

  make_directory(dir);

  std::string html_filename = filename_sans_suffix(dest) + ".html";
  FILE *html = fopen(html_filename.c_str(), "w");

  fprintf(html,
          "<html>\n"
          "<head>\n"
          "<style type=\"text/css\">\n"
          "div {display:inline-block; margin:5px}\n"
          "</style>\n"
          "</head>\n"
          "<body>\n");

  for (unsigned i = 0; i < src->nframes; i++) {
    std::string image_filename = string_printf("%04d.png", i);
    std::string image_path = dir + "/" + image_filename;
    std::string html_image_path = filename_sans_directory(dir) + "/" + image_filename;
    fprintf(stderr, "Writing %s\n", image_filename.c_str());
    write_png(image_path.c_str(), src->tile_width, src->tile_height,
              src->bands_per_pixel, src->bits_per_band, src->frame_pixels(i));
    fprintf(html, "<div><img src=\"%s\"><br>%04d</div>\n", html_image_path.c_str(), i);
  }
  fprintf(html,
          "</body>\n"
          "</html>\n");
  fclose(html);
  fprintf(stderr, "Created %s\n", html_filename.c_str());
}

void save(std::string dest)
{
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());

  if (create_parent_directories) make_directory_and_parents(filename_directory(dest));

  std::string temp_dest = temporary_path(dest);

  {
    simple_shared_ptr<FileWriter> out(FileWriter::open(temp_dest));
    src->write(out.get());
  }

  rename_file(temp_dest, dest);

  fprintf(stderr, "Created %s\n", dest.c_str());
}

class PrependLeaderTilestack : public LRUTilestack {
  simple_shared_ptr<Tilestack> source;
  unsigned leader_nframes;

public:
  PrependLeaderTilestack(simple_shared_ptr<Tilestack> source, int leader_nframes)
    : source(source), leader_nframes(leader_nframes) {
    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->source.get());
    set_nframes(leader_nframes + this->source->nframes);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    if (frame >= leader_nframes) {
      // Beyond leader -- use source frame
      memcpy(pixels[frame], source->frame_pixels(frame - leader_nframes), bytes_per_frame());
    } else if (frame >= leader_nframes - 2) {
      // Last two frames of leader -- use first frame of source
      memcpy(pixels[frame], source->frame_pixels(0), bytes_per_frame());
    } else if (frame == 0) {
      // First frame of leader -- use black
      memset(pixels[frame], 0, bytes_per_frame());
    } else {
      // Generate leader frame
      MWC random((frame + 1) * 4294967291, (frame + 1) * 3537812053);
      int frame_size = bytes_per_frame();
      for (int i = 0; i < frame_size;) {
        unsigned char c = random.get_byte() / 4 + 96;
        for (int j = 0; j < 8; j++) {
          if (i < frame_size) {
            for (unsigned k = 0; k < bytes_per_pixel(); k++) {
              pixels[frame][i++] = c;
            }
          }
        }
      }
    }
  }
};

void prepend_leader(int leader_nframes)
{
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  simple_shared_ptr<Tilestack> result(new PrependLeaderTilestack(src, leader_nframes));
  tilestackstack.push(result);
}

class BlackTilestack : public LRUTilestack {

public:
  BlackTilestack(int nframes, int width, int height, int bands_per_pixel, int bits_per_band) {
    set_nframes(nframes);
    tile_width = width;
    tile_height = height;
    this->bands_per_pixel = bands_per_pixel;
    this->bits_per_band = bits_per_band;
    pixel_format = 0;
    compression_format = 0;
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    memset(pixels[frame], 0, bytes_per_frame());
  }
};

// Video encoding uses 3 bands (more, e.g. alpha, will be ignored)
// and uses values 0-255 (values outside this range will be clamped)

void write_video(std::string dest, double fps, double compression, int max_size, std::string codec)
{
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  while (1) {
    std::string temp_dest = temporary_path(dest);
    if (!src->nframes) {
      throw_error("Tilestack has no frames in write_video");
    }

    if (create_parent_directories) make_directory_and_parents(filename_directory(dest));

    fprintf(stderr, "Encoding video to %s (temp %s)\n", dest.c_str(), temp_dest.c_str());

    VideoEncoder *encoder;
    if (codec == "h.264" || codec == "h264")
      encoder = new H264Encoder(temp_dest, src->tile_width, src->tile_height, fps, compression);
    else if (codec == "vp8")
      encoder = new VP8Encoder(temp_dest, src->tile_width, src->tile_height, fps, compression);
    else if (codec == "proreshq")
      encoder = new ProresHQEncoder(temp_dest, src->tile_width, src->tile_height, fps, compression);
    else
      throw_error("Codec '%s' not supported", codec.c_str());

    // Code should work with single-channel (duplicating 3x), or 3 or more channels (using first 3)
    assert(src->bands_per_pixel != 2);
    std::vector<unsigned char> destframe(src->tile_width * src->tile_height * 3);

    // Which channels for red, green, blue?
    int ch0 = 0, ch1 = 1, ch2 = 2;
    if (src->bands_per_pixel == 1) {
      // Greyscale -- duplicate single channel to r, g, b
      ch0 = ch1 = ch2 = 0;
    }

    for (unsigned frame = 0; frame < src->nframes; frame++) {
      unsigned char *srcptr = src->frame_pixels(frame);
      int src_bytes_per_pixel = src->bytes_per_pixel();
      unsigned char *destptr = &destframe[0];
      for (unsigned y = 0; y < src->tile_height; y++) {
        for (unsigned x = 0; x < src->tile_width; x++) {
          *destptr++ = (unsigned char) std::max(0, std::min(255, (int)src->get_pixel_band(srcptr, ch0)));
          *destptr++ = (unsigned char) std::max(0, std::min(255, (int)src->get_pixel_band(srcptr, ch1)));
          *destptr++ = (unsigned char) std::max(0, std::min(255, (int)src->get_pixel_band(srcptr, ch2)));
          srcptr += src_bytes_per_pixel;
        }
      }
      encoder->write_pixels(&destframe[0], destframe.size());
    }
    encoder->close();
    delete encoder;
    int filelen = (int) file_size(temp_dest);
    if (max_size > 0 && filelen > max_size) {
      compression += 2;
      fprintf(stderr, "Size is too large: %d > %d;  increasing compression to crf=%g and reencoding\n",
              filelen, max_size, compression);
      delete_file(temp_dest);
    } else {
      if (max_size > 0) fprintf(stderr, "Size %d <= max size %d\n", filelen, max_size);
      fprintf(stderr, "Renaming %s to %s\n", temp_dest.c_str(), dest.c_str());
      rename_file(temp_dest, dest);
      break;
    }
  }
}

int compute_tile_nlevels(int width, int height, int tile_width, int tile_height) {
  int max_level = 0;
  while (width > (tile_width << max_level) || height > (tile_height << max_level)) {
    max_level++;
  }
  return max_level + 1;
}

void image2tiles(std::string dest, std::string format, std::string src)
{
  if (filename_exists(dest)) {
    fprintf(stderr, "%s already exists, skipping\n", dest.c_str());
    return;
  }
  simple_shared_ptr<ImageReader> reader(ImageReader::open(src));
  //fprintf(stderr, "Opened %s: %d x %d pixels\n", src.c_str(), reader->width(), reader->height());

  std::vector<unsigned char> stripe(reader->bytes_per_row() * tilesize);
  std::vector<unsigned char> tile(reader->bytes_per_pixel() * tilesize * tilesize);

  int max_level = compute_tile_nlevels(reader->width(), reader->height(), tilesize, tilesize);

  make_directory_and_parents(dest);

  {
    Json::Value r;
    r["width"] = reader->width();
    r["height"] = reader->height();
    r["tile_width"] = r["tile_height"] = tilesize;
    std::string jsonfile = dest + "/r.json";
    std::ofstream jsonout(jsonfile.c_str());
    if (!jsonout.good()) throw_error("Error opening %s for writing", jsonfile.c_str());
    jsonout << r;
  }

  for (unsigned top = 0; top < reader->height(); top += tilesize) {
    unsigned nrows = std::min(reader->height() - top, tilesize);
    fill(stripe.begin(), stripe.end(), 0);
    reader->read_rows(&stripe[0], nrows);
    for (unsigned left = 0; left < reader->width(); left += tilesize) {
      unsigned ncols = std::min(reader->width() - left, tilesize);
      fill(tile.begin(), tile.end(), 0);
      for (unsigned y = 0; y < tilesize; y++) {
        memcpy(&tile[y * reader->bytes_per_pixel() * tilesize],
               &stripe[y * reader->bytes_per_row() + left * reader->bytes_per_pixel()],
               ncols * reader->bytes_per_pixel());
      }

      std::string path = dest + "/" + GPTileIdx(max_level - 1, left/tilesize, top/tilesize).path() + "." + format;
      std::string directory = filename_directory(path);
      make_directory_and_parents(directory);
      std::string temp_path = temporary_path(path);
      ImageWriter::write(temp_path, tilesize, tilesize, reader->bands_per_pixel(), reader->bits_per_band(),
                         &tile[0]);

      rename_file(temp_path, path);
    }
  }
}

class TilestackFromTiles : public LRUTilestack {
  std::vector<std::string> srcs;
public:
  TilestackFromTiles(const std::vector<std::string> &srcs_init) : srcs(srcs_init) {
    assert(srcs.size() > 0);
    if (delete_source_tiles) {
      source_tiles_to_delete.insert(source_tiles_to_delete.begin(), srcs.begin(), srcs.end());
    }

    simple_shared_ptr<ImageReader> tile0(ImageReader::open(srcs[0]));
    set_nframes(srcs.size());
    tile_width = tile0->width();
    tile_height = tile0->height();
    bands_per_pixel = tile0->bands_per_pixel();
    bits_per_band = tile0->bits_per_band();
    pixel_format = PixelInfo::PIXEL_FORMAT_INTEGER;
    compression_format = TilestackInfo::NO_COMPRESSION;
  }
private:
  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);

    toc[frame].timestamp = 0;
    simple_shared_ptr<ImageReader> tile(ImageReader::open(srcs[frame]));
    assert(tile->width() == tile_width);
    assert(tile->height() == tile_height);
    assert(tile->bands_per_pixel() == bands_per_pixel);
    assert(tile->bits_per_band() == bits_per_band);

    tile->read_rows(pixels[frame], tile->height());
  }
};

void load_tiles(const std::vector<std::string> &srcs)
{
  simple_shared_ptr<Tilestack> tilestack(new TilestackFromTiles(srcs));
  tilestackstack.push(tilestack);
}

struct Image {
  PixelInfo pixel_info;
  int width;
  int height;
  unsigned char *pixels;
  Image(const PixelInfo &pixel_info, int width, int height, unsigned char *pixels)
  : pixel_info(pixel_info), width(width), height(height), pixels(pixels) {}
  unsigned char *pixel(int x, int y) {
    return pixels + pixel_info.bytes_per_pixel() * (x + y * width);
  }
};

class Renderer : public TilestackInfo {
protected:
  std::string stackset_path;
  int width, height;
  int nlevels;

  static int fast_render_count;
  static int slow_render_count;

  static double interpolate(double val, double in_min, double in_max, double out_min, double out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  // x and y must vary from 0 to 1
  static double bilinearly_interpolate(double a00, double a01, double a10, double a11, double x, double y) {
    return
      a00 * (1-x) * (1-y) +
      a01 * (1-x) *   y   +
      a10 *   x   * (1-y) +
      a11 *   x   *   y;
  }

  virtual Tilestack *get_tilestack(int level, int x, int y) = 0;

public:

  // 47 vs .182:  250x more CPU than ffmpeg
  // 1.86 vs .182: 10x more CPU than ffmpeg
  // .81 vs .182: 4.5x more CPU than ffmpeg
  // .5 vs .182: 2.75x more CPU than ffmpeg
  // .41 vs .182: 2.25x more CPU than ffmpeg

  void get_pixel(unsigned char *dest, int frame, int level, int x, int y) {
    if (x >= 0 && y >= 0) {
      int tile_x = x / tile_width;
      int tile_y = y / tile_height;
      Tilestack *tilestack = get_tilestack(level, tile_x, tile_y);
      if (tilestack) {
        memcpy(dest, tilestack->frame_pixel(frame, x % tile_width, y % tile_height), bytes_per_pixel());
        return;
      }
    }
    memset(dest, 0, bytes_per_pixel());
  }

  // Pixels are centered at +.5
  void interpolate_pixel(unsigned char *dest, int frame, int level, double x, double y) {
    // Convert to pixels centered at +.0
    x -= 0.5;
    y -= 0.5;

    const unsigned int MAX_BYTES_PER_PIXEL = 128;
    unsigned char p00[MAX_BYTES_PER_PIXEL];
    unsigned char p01[MAX_BYTES_PER_PIXEL];
    unsigned char p10[MAX_BYTES_PER_PIXEL];
    unsigned char p11[MAX_BYTES_PER_PIXEL];

    assert(bytes_per_pixel() < MAX_BYTES_PER_PIXEL);

    int x0 = (int)floor(x);
    int y0 = (int)floor(y);

    get_pixel(p00, frame, level, x0+0, y0+0);
    get_pixel(p01, frame, level, x0+0, y0+1);
    get_pixel(p10, frame, level, x0+1, y0+0);
    get_pixel(p11, frame, level, x0+1, y0+1);

    for (unsigned band = 0; band < bands_per_pixel; band++) {
      set_pixel_band(dest, band, bilinearly_interpolate(get_pixel_band(p00, band),
                                                      get_pixel_band(p01, band),
                                                      get_pixel_band(p10, band),
                                                      get_pixel_band(p11, band),
                                                      x - x0, y - y0));
    }
  }

  // Render an image from tilestak and do appropriate projections
  // Its source projection is in Plate Carree and its final projection is in equidistant azimuthal
  // for a half sphere

  void render_projection(Image &dest, const Frame &frame, const double XR1, const double XR2, const double YR, const double X, const double Y, const double Pitch, const double Yaw, const double wf, const double wb, const double wt) {
    int frameno = (int) frame.frameno;
    if (frameno >= (int) nframes) {
      throw_error("Attempt to render frame number %d from tilestack (valid frames 0 to %d)",
                  frameno, nframes-1);
    }
    Bbox myb = frame.bounds;

    double x1, y1, theta1, psi1, x2, y2, z2, x3, y3, z3, psi2, theta2, x, y;

    // center of tour editor bounding box
    double yc = myb.width / 2.0 + myb.x;
    double xc = myb.height / 2.0 + myb.y;

    // center of horizon
    double xh = XR1 / (XR1 + XR2) * X;
    double yh = Y / 2.0;

    // zoom parameter
    double zoom = std::max(Y/myb.width, X/myb.height);

    // rotations needed to center picture based on the initial bounding box
    double yaw = Yaw + (yc - yh) / Y * YR;
    double pitch = Pitch + (xc - xh) / X * (XR1 + XR2);
    double r[3][3];

    // creating the rotation matrix
    r[0][0] = cos(pitch) * cos(yaw);
    r[0][1] = -sin(yaw);
    r[0][2] = cos(yaw) * sin(pitch);

    r[1][0] = cos(pitch) * sin(yaw);
    r[1][1] = cos(yaw);
    r[1][2] = sin(pitch) * sin(yaw);

    r[2][0] = -sin(pitch);
    r[2][1] = 0;
    r[2][2] = cos(pitch);

    for (int i = 0; i < dest.height; i++)
      for (int j = 0; j < dest.width; j++) {
        x1 = PI*((i+1.0)/dest.height-0.5);
        y1 = PI*((j+1.0)/dest.width-0.5);
        theta1 = atan2(y1,x1);
        psi1 = PI/2.0-sqrt(x1*x1+y1*y1);

        if (theta1 > wf || theta1 < -1.*wf || wb > psi1 || psi1 > wt) {
          memset(dest.pixel(j,i), 0, bytes_per_pixel());
          continue;
        }

        // rotations
        x2 = cos(psi1)*cos(theta1);
        y2 = cos(psi1)*sin(theta1);
        z2 = sin(psi1);

        x3 = r[0][0] * x2 + r[0][1] * y2 + r[0][2] * z2;
        y3 = r[1][0] * x2 + r[1][1] * y2 + r[1][2] * z2;
        z3 = r[2][0] * x2 + r[2][1] * y2 + r[2][2] * z2;

        // returning back to polar coordinates
        psi2 = atan2(z3,sqrt(x3*x3+y3*y3));
        theta2 = atan2(y3,x3);

        // finding the coordinates in the original picture
        x = xh - psi2 * X / (XR1 + XR2);
        y = theta2 * Y / YR + yh;

        // accounting for zoom
        x = (x - xc) / zoom + xc;
        y = (y - yc) / zoom + yc;

        // calculate the pixel
        interpolate_pixel(dest.pixel(j,i), frameno, nlevels-1, y, x);
      }
  }

  // Render an image from the tilestack
  //
  // Q: What level do we want to use for source pixels?
  //
  // A: We use this routine for building subsampled tiles.  In this case we want to grab pixels from the level
  // that has exactly twice the destination resolution, since the destination resolution doesn't exist yet
  // We also want to read from coordinates that are halfway in-between the pixels from the source level
  //
  // A: We use this routine for building video tiles.  In this case, we want to grab pixels from the level
  // that has exactly the right output resolution.  We also want to read from coordinates that are exactly in
  // line with pixels at this level, to not introduce blurring
  //
  // A: We use this routine for rendering tours.  In this case, we're grabbing pixels at a wide variety of fractional pixel
  // offsets.  Grabbing pixels from a level that has higher resolution is good.  Grabbing pixels from exactly the same
  // resolution isn't great, since we may be grabbing in-between pixels and making something more blurry than it needs to be.

  // frame coords are similar to OpenGL texture coords, but with positive Y going downwards
  // The center of the upper left pixel is 0.5, 0.5
  // The upper left corner of the upper left pixel is 0,0

  void render_image(Image &dest, const Frame &frame, bool downsize) {
    int frameno = (int) frame.frameno;
    if (frameno >= (int)nframes) {
      throw_error("Attempt to render frame number %d from tilestack (valid frames 0 to %d)",
                  frameno, nframes-1);
    }

    // scale between original pixels and desination frame.  If less than one, we subsample (sharp), or greater than
    // one means supersample (blurry)
    double scale = dest.width / frame.bounds.width;
    double cutoff = 1.0000001;
    if (downsize) cutoff *= .5;
    double subsample_nlevels = log(cutoff / scale)/log(2.0);
    double source_level_d = (nlevels - 1) - subsample_nlevels;
    int source_level = limit((int)ceil(source_level_d), 0, (int)(nlevels - 1));
    //fprintf(stderr, "dest.width = %d, frame.bounds.width = %g\n", dest.width, frame.bounds.width);
    //fprintf(stderr, "nlevels = %d, source_level_d = %g, source_level = %d\n", nlevels, source_level_d, source_level);
    Bbox bounds = frame.bounds;
    if (source_level != nlevels-1) {
      bounds = bounds / (1 << (nlevels-1-source_level));
    }
    if (dest.height == bounds.height &&
        dest.width == bounds.width &&
        bounds.x == (int)bounds.x &&
        bounds.y == (int)bounds.y &&
        dest.pixel_info.bands_per_pixel == bands_per_pixel &&
        dest.pixel_info.bits_per_band == bits_per_band) {
      fast_render_count++;
      int bounds_x = (int)bounds.x;
      int bounds_y = (int)bounds.y;
      for (int y = 0; y < dest.height; y++) {
        for (int x = 0; x < dest.width; x++) {
          get_pixel(dest.pixel(x, y), frameno, source_level, bounds_x + x, bounds_y + y);
        }
      }
    } else {
      //fprintf(stderr, "slowly rendering %d x %d from %s\n", dest.width, dest.height, bounds.to_string().c_str());
      slow_render_count++;
      for (int y = 0; y < dest.height; y++) {
        double source_y = interpolate(y + 0.5, 0, dest.height, bounds.y, bounds.y + bounds.height);
        for (int x = 0; x < dest.width; x++) {
          double source_x = interpolate(x + 0.5, 0, dest.width, bounds.x, bounds.x + bounds.width);
          interpolate_pixel(dest.pixel(x,y), frameno, source_level, source_x, source_y);
        }
      }
    }
  }

  virtual ~Renderer() {}

  static std::string stats() {
    int total_render_count = slow_render_count + fast_render_count;
    std::string ret = string_printf("%d images rendered", total_render_count);
    if (total_render_count) ret += string_printf(" (%.0f%% fast)", 100.0 * fast_render_count / total_render_count);
    return ret;
  }
};

int Renderer::slow_render_count;
int Renderer::fast_render_count;

class StacksetRenderer : public Renderer {
protected:
  std::string stackset_path;
  JSON info;
  std::map<unsigned long long, TilestackReader* > readers;

  std::string path(int level, int x, int y) {
    return stackset_path + "/" + GPTileIdx(level, x, y).path() + ".ts2";
  }

  TilestackReader *get_tilestack(int level, int x, int y) {
    static unsigned long long cached_idx = -1;
    static TilestackReader *cached_reader = NULL;
    unsigned long long idx = GPTileIdx::idx(level, x, y);
    if (idx == cached_idx) return cached_reader;
    if (readers.find(idx) == readers.end()) {
      // TODO(RS): If this starts running out of RAM, consider LRU on the readers
      try {
        //fprintf(stderr, "get_reader constructing TilestackReader %llx from %s\n", (unsigned long long) readers[idx], path(level, x, y).c_str());
	readers[idx] = new TilestackReader(simple_shared_ptr<Reader>(FileReader::open(path(level, x, y))));
      } catch (std::runtime_error &e) {
        //fprintf(stderr, "No tilestackreader for (%d, %d, %d)\n", level, x, y);
        readers[idx] = NULL;
      }
    }
    cached_idx = idx;
    cached_reader = readers[idx];
    return cached_reader;
  }

public:
  StacksetRenderer(const std::string &stackset_path) : stackset_path(stackset_path) {
    fprintf(stderr, "stackset_path is %s\n", stackset_path.c_str());
    std::string json_path = stackset_path + "/r.json";
    info = JSON::fromFile(json_path);
    width = info["width"].integer();
    height = info["height"].integer();
    tile_width = info["tile_width"].integer();
    tile_height = info["tile_height"].integer();

    nlevels = compute_tile_nlevels(width, height, tile_width, tile_height);
    Tilestack *tilestack = get_tilestack(nlevels-1, 0, 0);
    if (!tilestack) throw_error("Initializing stackset but couldn't find tilestack at path %s",
                                path(nlevels-1, 0, 0).c_str());
    (TilestackInfo&)(*this) = (TilestackInfo&)(*tilestack);
  }

  virtual ~StacksetRenderer() {
    for (std::map<unsigned long long, TilestackReader*>::iterator i = readers.begin(); i != readers.end(); ++i) {
      if (i->second) delete i->second;
      i->second = NULL;
    }
  }
};

class TilestackRenderer : public Renderer {
protected:
  simple_shared_ptr<Tilestack> tilestack;

  Tilestack *get_tilestack(int level, int x, int y) {
    if (level == 0 && x == 0 && y == 0) return tilestack.get();
    return 0;
  }

public:
  TilestackRenderer(simple_shared_ptr<Tilestack> &tilestack) : tilestack(tilestack) {
    width = tile_width = tilestack->tile_width;
    height = tile_height = tilestack->tile_height;
    nlevels = 1;

    (TilestackInfo&)(*this) = (TilestackInfo&)(*tilestack);
  }

  virtual ~TilestackRenderer() {}
};

class TilestackFromPathProjected : public LRUTilestack {
  simple_shared_ptr<Renderer> renderer;
  std::vector<Frame> frames;

  double pixelPerRadian, XR1, XR2, YR, pitch, yaw, X, Y;

  void init(Renderer *renderer_init, int stack_width_init, int stack_height_init, JSON path, JSON warp_settings) {
    renderer.reset(renderer_init);
    parse_warp(frames, path, warp_settings);
    set_nframes(frames.size());
    tile_width = stack_width_init;
    tile_height = stack_height_init;
    bands_per_pixel = renderer->bands_per_pixel;
    bits_per_band = renderer->bits_per_band;
    pixel_format = renderer->pixel_format;
    compression_format = 0;
  }

public:
  static double wt, wb, wf; // window top, window bottom, window field

  TilestackFromPathProjected(int stack_width, int stack_height, double source_pixelPerRadian, double source_XR1, double source_XR2, double source_YR, double dest_pitch, double dest_yaw, JSON path, const std::string &stackset_path, JSON warp_settings) {
    pixelPerRadian = source_pixelPerRadian;
    XR1 = source_XR1;
    XR2 = source_XR2;
    YR = source_YR;
    pitch = dest_pitch;
    yaw = dest_yaw;
    X = pixelPerRadian * (XR1 + XR2);
    Y = pixelPerRadian * YR;

    init(new StacksetRenderer(stackset_path), stack_width, stack_height, path, warp_settings);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);
    Image image(*this, tile_width, tile_height, pixels[frame]);
    renderer->render_projection(image, frames[frame], XR1, XR2, YR, X, Y, pitch, yaw, wf, wb, wt);
  }
};

double TilestackFromPathProjected::wt = PI/2; // window top
double TilestackFromPathProjected::wb = 0; // window bottom
double TilestackFromPathProjected::wf = PI; // window field

class TilestackFromPath : public LRUTilestack {
  simple_shared_ptr<Renderer> renderer;
  std::vector<Frame> frames;
  bool downsize;

  void init(Renderer *renderer_init, int stack_width_init, int stack_height_init, JSON path, bool downsize_init, JSON warp_settings) {
    renderer.reset(renderer_init);
    parse_warp(frames, path, warp_settings);
    set_nframes(frames.size());
    tile_width = stack_width_init;
    tile_height = stack_height_init;
    downsize = downsize_init;
    bands_per_pixel = renderer->bands_per_pixel;
    bits_per_band = renderer->bits_per_band;
    pixel_format = renderer->pixel_format;
    compression_format = 0;
  }

public:
  TilestackFromPath(int stack_width, int stack_height, JSON path, const std::string &stackset_path, bool downsize, JSON warp_settings) {
    init(new StacksetRenderer(stackset_path), stack_width, stack_height, path, downsize, warp_settings);
  }

  TilestackFromPath(int stack_width, int stack_height, JSON path, simple_shared_ptr<Tilestack> &tilestack, bool downsize, JSON warp_settings) {
    init(new TilestackRenderer(tilestack), stack_width, stack_height, path, downsize, warp_settings);
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);
    Image image(*this, tile_width, tile_height, pixels[frame]);
    renderer->render_image(image, frames[frame], downsize);
  }
};

void path2stack_projected(int stack_width, int stack_height, double pixelPerRadian, double XR1, double XR2, double YR, double pitch, double yaw, JSON path, const std::string &stackset_path, JSON warp_settings) {
  simple_shared_ptr<Tilestack> out(new TilestackFromPathProjected(stack_width, stack_height, pixelPerRadian, XR1, XR2, YR, pitch, yaw, path, stackset_path, warp_settings));
  tilestackstack.push(out);
}

void path2stack(int stack_width, int stack_height, JSON path, const std::string &stackset_path, bool downsize, JSON warp_settings) {
  simple_shared_ptr<Tilestack> out(new TilestackFromPath(stack_width, stack_height, path, stackset_path, downsize, warp_settings));
  tilestackstack.push(out);
}

void path2stack_from_stack(int stack_width, int stack_height, JSON path, JSON warp_settings) {
  simple_shared_ptr<Tilestack> src(tilestackstack.pop());
  simple_shared_ptr<Tilestack> out(new TilestackFromPath(stack_width, stack_height, path, src, false, warp_settings));
  tilestackstack.push(out);
}

class OverlayFromPath : public LRUTilestack {
  std::vector<Frame> frames;
  std::string overlay_html_path;

public:
  OverlayFromPath(int stack_width, int stack_height, JSON path, const std::string &overlay_html_path, JSON warp_settings) :
    overlay_html_path(overlay_html_path) {
    parse_warp(frames, path, warp_settings);
    set_nframes(frames.size());
    tile_width = stack_width;
    tile_height = stack_height;
    bands_per_pixel = 4;
    bits_per_band = 8;
    pixel_format = PixelInfo::PIXEL_FORMAT_INTEGER;
    compression_format = TilestackInfo::NO_COMPRESSION;
  }

  virtual void instantiate_pixels(unsigned frame) const {
    assert(!pixels[frame]);
    create(frame);
    //Image image(*this, tile_width, tile_height, pixels[frame]);
    //stackset.render_image(image, frames[frame], downsize);
    std::string overlay_image_path = temporary_path("overlay.png");

    std::string path_to_render_js = render_js_path_override != "" ? render_js_path_override :
      filename_directory(executable_path()) + "/render.js";

    // Truncate frameno;  if in the future we start interpolating, think how to do this --
    //  maybe pass floating point to html page to render an animation between labels?
    int source_frameno = (int) frames[frame].frameno;

    std::string cmd = string_printf("phantomjs %s %s %d %s %d %d",
                                    path_to_render_js.c_str(),
                                    overlay_html_path.c_str(),
                                    source_frameno,
                                    overlay_image_path.c_str(),
                                    tile_width,
                                    tile_height);
    if (system(cmd.c_str())) {
      throw_error("Error executing '%s'", cmd.c_str());
    }
    {
      PngReader overlay(overlay_image_path);
      if (overlay.width() != tile_width || overlay.height() != tile_height) {
        throw_error("Overlay dimensions %d,%d don't match stack dimensions %d,%d",
                    overlay.width(), overlay.height(), tile_width, tile_height);
      }
      overlay.read_rows(pixels[frame], tile_height);
    }
    delete_file(overlay_image_path);
  }
};

void path2overlay(int stack_width, int stack_height, JSON path, const std::string &overlay_html_path, JSON warp_settings) {
  simple_shared_ptr<Tilestack> out(new OverlayFromPath(stack_width, stack_height, path, overlay_html_path, warp_settings));
  tilestackstack.push(out);
}

void usage(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr,          "\nUsage:\n"
          "tilestacktool [args]\n"
          "--load src.ts2\n"
          "--save dest.ts2\n"
          "--viz min max gamma\n"
          "--writehtml dest.html\n"
          "--writevideo dest.type fps compression codec\n"
          "              h.264: 24=high quality, 28=typical, 30=low quality\n"
          "				 vp8: 10=high quality, 30=typical, 50=low quality\n"
          "				 proreshq: 5=high quality, 9=typical, 13=low quality\n"
          "--ffmpeg-path path_to_ffmpeg\n"
          /*"--vpxenc-path path_to_vpxenc\n"*/
          "--image2tiles dest_dir format src_image\n"
          "              Be sure to set tilesize earlier in the commandline\n"
          "--tilesize N\n"
          "--loadtiles src_image0 src_image1 ... src_imageN\n"
          "--loadtiles-from-json path.json\n"
          "--delete-source-tiles\n"
          "--create-parent-directories\n"
          "--path2stack width height path-or-warp-json stackset-path [warp-settings-json]\n"
          "        width, height:  size, in pixels, of output stack\n"
          "        path-or-warp-json:\n"
          "           Path, or warp, in JSON format.  Can be inline JSON, or a filename prefixed with @ (e.g. @filename.json)\n"
          "              Path format [frame1, frame2, ... frameN]\n"
          "              Frame format {\"frame\":N, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
          "              Multiframe with single bounds. from and to are both inclusive.  step defaults to 1:\n"
          "              {\"frames\":{\"from\":N, \"to\":N, \"step\":N}, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\n"
          "           Warp format is standard format output by tour editor\n"
          "\n"
          "        stackset-path:\n"
          "           Directory name for root of stackset;  stackset-path/r.json should exist and contain metainfo\n"
          "\n"
          "        warp-settings-json:\n"
          "           Only used when warp is specified.\n"
          "           Form: {\"sourceFPS\": N, \"destFPS\": N, \"smoothingDuration\": N}\n"
          "              sourceFPS: frames per second from Time Machine source.  Can be found in its r.json\n"
          "              destFPS: frames per second to render in output tilestack.  OK to be different from source FPS\n"
          "              smoothingDuration: amount of time, in seconds, to smooth motion.  1.0 is a good number\n"
          "\n"
          "--path2stack-from-stack width height [frame1, ... frameN] [warp-settings-json]\n"
          "        Frame format {\"frame\":N, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
          "        Multiframe with single bounds. from and to are both inclusive.  step defaults to 1:\n"
          "          {\"frames\":{\"from\":N, \"to\":N, \"step\":N}, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
          "--path2overlay width height [frame1, ... frameN] overlay.html\n"
          "        Create tilestack by rendering overlay.html#FRAMENO for each frame in path.  Leave background in overlay.html unset\n"
          "        to create an overlay with transparent background\n"
          "--path2stack-projected width height source_pixel_per_radian source_height_above_horizon_radians source_height_below_horizon_radians source_width_radians projection_pitch projection_yaw path-or-warp-json stackset-path [warp-settings-json]\n"
          "--path2stack-projected-xml width height path_to_stitcher_r.info projection_pitch projection_yaw path-or-warp-json stackset-path [warp-settings-json]\n"
          "--projection-window window_field_radian window_bottom_radian window_top_radian\n"
          "        crops visible window on dome. must be set before --path2stack-projected or --path2stack-projected-xml\n"
          "--cat\n"
          "        Temporally concatenate all tilestacks on stack into one\n"
          "--composite\n"
          "        Framewise overlay top of stack onto second from top.  Stacks must have same dimensions\n"
          "--createfile file   (like touch file)\n"
          "--loadraw file width height (uint8|uint16|uint32|float32|float64) channels\n"
          "--hblur sigma: gaussian blur horizontally\n"
          "--vblur sigma: gaussian blur vertically\n"
          "--tblur sigma: gaussian blur in time\n"
          );
  exit(1);
}

int n_commands = 0;
const int MAX_COMMANDS = 1000;
Command commands[MAX_COMMANDS];

bool register_command(Command cmd) {
  commands[n_commands++] = cmd;
  return true;
}

bool self_test() {
  bool success = true;
  fprintf(stderr, "tilestacktool self-test: ");
  {
    fprintf(stderr, "ffmpeg/vpxenc/qt-faststart:\n");
    bool encoder_ok = H264Encoder::test() && VP8Encoder::test();
    fprintf(stderr, "%s\n", encoder_ok ? "success" : "FAIL");
    success = success && encoder_ok;
  }
  fprintf(stderr, "Self-test %s\n", success ? "succeeded" : "FAILED");
  return success;
}

int main(int argc, char **argv)
{
  try {
    Arglist args(argv+1, argv+argc);
    while (!args.empty()) {
      std::string arg = args.shift();
      if (arg == "--load") {
        std::string src = args.shift();
        if (filename_suffix(src) != "ts2") {
          usage("Filename to load should end in '.ts2'");
        }
        load(src);
      }
      else if (arg == "--loadraw") {
        std::string src = args.shift();
        TilestackInfo ti;
        ti.tile_width = args.shift_int();
        ti.tile_height = args.shift_int();
        {
          // TODO: parse this correctly
          if (args.shift() != "uint8") {
            usage("Only supporting uint8 for raw input currently");
          }
          ti.bits_per_band = 8;
          ti.pixel_format = PixelInfo::PIXEL_FORMAT_INTEGER;
        }
        ti.bands_per_pixel = args.shift_int();

        loadraw(src, ti);
      }
      else if (arg == "--version") {
        printf("%s\n", version());
        return 0;
      }
      else if (arg == "--save") {
        std::string dest = args.shift();
        if (filename_suffix(dest) != "ts2") {
          usage("Filename to save should end in '.ts2'");
        }
        save(dest);
      }
      else if (arg == "--viz") {
        JSON params = args.shift_json();
        viz(params);
      }
      else if (arg == "--writehtml") {
        std::string dest = args.shift();
        write_html(dest);
      }
      else if (arg == "--writepng") {
        std::string dest = args.shift();
        write_png(dest);
      }
      else if (arg == "--prependleader") {
        int leader_nframes = args.shift_int();
        prepend_leader(leader_nframes);
      }
      else if (arg == "--blackstack") {
        int nframes = args.shift_int();
        int width = args.shift_int();
        int height = args.shift_int();
        int bands_per_pixel = args.shift_int();
        int bits_per_band = args.shift_int();
        simple_shared_ptr<Tilestack> blacktilestack(new BlackTilestack(nframes, width, height,
                                                                     bands_per_pixel,
                                                                     bits_per_band));

        tilestackstack.push(blacktilestack);
      }
      else if (arg == "--writevideo") {
        std::string dest = args.shift();
        double fps = args.shift_double();
        double compression = args.shift_double();
        int max_size = 0; // TODO(rsargent):  don't hardcode this
        // Default fallback codec if none specified
        std::string codec = "h.264";
        if (!args.empty())
          codec = args.shift();
        write_video(dest, fps, compression, max_size, codec);
      }
      else if (arg == "--tilesize") {
        tilesize = args.shift_int();
      }
      else if (arg == "--image2tiles") {
        std::string dest = args.shift();
        std::string format = args.shift();
        std::string src = args.shift();
        image2tiles(dest, format, src);
      }
      else if (arg == "--selftest") {
        exit(!self_test());
      }
      else if (arg == "--loadtiles-from-json") {
        std::vector<std::string> srcs;

        std::string json_path = args.shift();
        JSON tileList = JSON::fromFile(json_path);

        for (unsigned i = 0; i < tileList["tiles"].size(); i++) {
          srcs.push_back(tileList["tiles"][i].str());
        }

        if (srcs.empty()) {
          usage("--loadtiles-from-json must have at least one tile defined in the json");
        }

        //delete_file(json_path.c_str());
        load_tiles(srcs);
      } else if (arg == "--loadtiles") {
        std::vector<std::string> srcs;
        while (!args.empty() && args.front().substr(0,1) != "-") {
          srcs.push_back(args.shift());
        }
        if (srcs.empty()) {
          usage("--loadtiles must have at least one tile");
        }
        load_tiles(srcs);
        fprintf(stderr, "Loaded %d tiles\n", (int)srcs.size());
      }
      else if (arg == "--delete-source-tiles") {
        delete_source_tiles = true;
      }
      else if (arg == "--tilestackinfo") {
        tilestack_info();
      }
      else if (arg == "--cat") {
        cat();
      }
      else if (arg == "--composite") {
        composite();
      }
      else if (arg == "--subtract") {
        binop(subtract_op);
      }
      else if (arg == "--add") {
        binop(add_op);
      }
      else if (arg == "--scale") {
        double scale = args.shift_double();
        unop(scale_unop(scale));
      }
      else if (arg == "--hblur") {
        float sigma = args.shift_double();
        simple_shared_ptr<Tilestack> result(new HConvolve(tilestackstack.pop(), gaussian_kernel(sigma)));
        tilestackstack.push(result);
      }
      else if (arg == "--vblur") {
        float sigma = args.shift_double();
        simple_shared_ptr<Tilestack> result(new VConvolve(tilestackstack.pop(), gaussian_kernel(sigma)));
        tilestackstack.push(result);
      }
      else if (arg == "--tblur") {
        float sigma = args.shift_double();
        simple_shared_ptr<Tilestack> result = tconvolve(tilestackstack.pop(), gaussian_kernel(sigma));
        tilestackstack.push(result);
      }
      else if (arg == "--create-parent-directories") {
        create_parent_directories = true;
      }
      else if (arg == "--ffmpeg-path") {
        H264Encoder::ffmpeg_path_override = args.shift();
        VP8Encoder::ffmpeg_path_override = H264Encoder::ffmpeg_path_override;
      }
      /*else if (arg == "--vpxenc-path") {
        VP8Encoder::vpxenc_path_override = args.shift();
      }*/
      else if (arg == "--render-path") {
        render_js_path_override = args.shift();
      }
      else if (arg == "--projection-window") {
        double wf = args.shift_double();
        double wb = args.shift_double();
        double wt = args.shift_double();
        if (wb < 0 || wb > PI/2) {
          usage("--projection-window: window bottom should be between 0 and PI/2");
        }
        if (wt < 0 || wt > PI/2) {
          usage("--projection-window: window top should be between 0 and PI/2");
        }
        if (wf < 0 || wf > PI) {
          usage("--projection-window: window field should be between 0 and PI");
        }
        TilestackFromPathProjected::wb = wb;
        TilestackFromPathProjected::wt = wt;
        TilestackFromPathProjected::wf = wf;
      }
      else if (arg == "--path2stack-projected") {
        int stack_width = args.shift_int();
        int stack_height = args.shift_int();
        double pixelPerRadian = args.shift_double();
        double XR1 = args.shift_double();
        double XR2 = args.shift_double();
        double YR = args.shift_double();
        double pitch = args.shift_double();
        double yaw = args.shift_double();
        JSON path = args.shift_json();
        std::string stackset = args.shift();
        JSON warp_settings =
          (args.next_is_non_flag()) ? args.shift_json() : JSON("{}");
        if (stack_width <= 0 || stack_height <= 0) {
          usage("--path2stack-projected: width and height must be positive numbers");
        }
        path2stack_projected(stack_width, stack_height, pixelPerRadian, XR1, XR2, YR, pitch, yaw, path, stackset, warp_settings);
      }
      else if (arg == "--path2stack-projected-xml") {
        int stack_width = args.shift_int();
        int stack_height = args.shift_int();
        if (stack_width <= 0 || stack_height <= 0) {
          usage("--path2stack-projected-xml: width and height must be positive numbers");
        }

        std::string xmlPath = args.shift();
        Rinfo r = parse_xml(xmlPath.c_str());
        if (r.minx == -1 || r.miny == -1 || r.maxx == -1 || r.maxy == -1 || r.projx <= 0 || r.projy <= 0)
          usage("--path2stack-projected-xml: stitcher xml file is not correct");

        double pixelPerRadian = r.projy/PI;
        double XR1 = PI * (0.5 - r.miny / r.projy);
        double XR2 = PI * (r.maxy / r.projy - 0.5);
        double YR = (r.maxx - r.minx) / r.projx * 2 * PI;
        double pitch = args.shift_double();
        double yaw = args.shift_double();
        JSON path = args.shift_json();
        std::string stackset = args.shift();
        JSON warp_settings =
          (args.next_is_non_flag()) ? args.shift_json() : JSON("{}");

        path2stack_projected(stack_width, stack_height, pixelPerRadian, XR1, XR2, YR, pitch, yaw, path, stackset, warp_settings);
      }
      else if (arg == "--path2stack" || arg == "--path2stack-downsize" || arg == "--path2stack-from-stack") {
        int stack_width = args.shift_int();
        int stack_height = args.shift_int();
        JSON path = args.shift_json();
        std::string stackset = (arg != "--path2stack-from-stack") ? args.shift() : "";
        JSON warp_settings =
          (args.next_is_non_flag()) ? args.shift_json() : JSON("{}");
        if (stack_width <= 0 || stack_height <= 0) {
          usage("--path2stack: width and height must be positive numbers");
        }
        bool downsize = (arg == "--path2stack-downsize");
        if (arg == "--path2stack-from-stack") {
          path2stack_from_stack(stack_width, stack_height, path, warp_settings);
        } else {
          path2stack(stack_width, stack_height, path, stackset, downsize,
                     warp_settings);
        }
      }
      else if (arg == "--path2overlay") {
        int stack_width = args.shift_int();
        int stack_height = args.shift_int();
        JSON path = args.shift_json();
        std::string overlay_html_path = args.shift();
        JSON warp_settings =
          (args.next_is_non_flag()) ? args.shift_json() : JSON("{}");
        path2overlay(stack_width, stack_height, path,
                     overlay_html_path, warp_settings);
      }
      else if (arg == "--createfile") {
        simple_shared_ptr<FileWriter> out(FileWriter::open(args.shift()));
      }
      else {
        for (int i = 0; i < n_commands; i++) {
          if (commands[i](arg, args)) goto success;
        }
        usage("Unknown argument %s", arg.c_str());
     success:;
      }
    }
    if (source_tiles_to_delete.size()) {
      fprintf(stderr, "Deleting %ld source tiles\n", (long) source_tiles_to_delete.size());

      for (unsigned i = 0; i < source_tiles_to_delete.size(); i++) {
        unlink(source_tiles_to_delete[i].c_str());
      }
    }
    double user, system;
    get_cpu_usage(user, system);
    fprintf(stderr, "%s\n", TilestackReader::stats().c_str());
    fprintf(stderr, "%s\n", Renderer::stats().c_str());

    fprintf(stderr, "User time %g, System time %g\n", user, system);

    while (tilestackstack.size()) tilestackstack.pop();

    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error: %s\n", e.what());
    return 1;
  }
}
