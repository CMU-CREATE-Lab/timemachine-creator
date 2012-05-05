#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

#include "marshal.h"
#include "cpp-utils.h"
#include "png_util.h"
#include "GPTileIdx.h"
#include "ImageReader.h"
#include "ImageWriter.h"

#include "json/json.h"

using namespace std;

#define TODO(x) do { fprintf(stderr, "%s:%d: error: TODO %s\n", __FILE__, __LINE__, x); abort(); } while (0)

unsigned int tilesize = 512;
bool create_parent_directories = false;
bool delete_source_tiles = false;
vector<string> source_tiles_to_delete;

void usage(const char *fmt, ...);

class Arglist : public list<string> {
public:
  Arglist(char **begin, char **end) {
    for (char **arg = begin; arg < end; arg++) push_back(string(*arg));
  }
  string shift() {
    if (empty()) usage("Missing argument");
    string ret = front();
    pop_front();
    return ret;
  }
  double shift_double() {
    string arg = shift();
    char *end;
    double ret = strtod(arg.c_str(), &end);
    if (end != arg.c_str() + arg.length()) {
      usage("Can't parse '%s' as floating point value", arg.c_str());
    }
    return ret;
  }
  double shift_int() {
    string arg = shift();
    char *end;
    double ret = strtol(arg.c_str(), &end, 0);
    if (end != arg.c_str() + arg.length()) {
      usage("Can't parse '%s' as integer", arg.c_str());
    }
    return ret;
  }
  Json::Value shift_json() {
    string arg = shift();
    Json::Reader reader;
    Json::Value ret;
    if (!reader.parse(arg, ret)) {
      usage("Can't parse '%s' as json: %s", arg.c_str());
    }
    return ret;
  }
};

class Reader {
public:
  virtual void read(unsigned char *dest, size_t offset, size_t length) = 0;
  virtual size_t length() = 0;
  vector<unsigned char> read(size_t offset, size_t length) {
    vector<unsigned char> ret(length);
    read(&ret[0], offset, length);
    return ret;
  }
};

class IfstreamReader : public Reader {
  ifstream f;
  string filename;
public:
  IfstreamReader(string filename) : f(filename.c_str(), ios::in | ios::binary), filename(filename) {
    if (!f.good()) throw_error("Error opening %s for reading\n", filename.c_str());
  }    
  virtual void read(unsigned char *dest, size_t pos, size_t length) {
    f.seekg(pos, ios_base::beg);
    f.read((char*)dest, length);
    if (f.fail()) {
      throw_error("Error reading %zd bytes from file %s at position %zd\n",
          length, filename.c_str(), pos);
    }
  }
  size_t length() {
    f.seekg(0, ios::end);
    return f.tellg();
  }
};

class Writer {
public:
  virtual void write(const unsigned char *src, size_t length) = 0;
  void write(const vector<unsigned char> &src) {
    write(&src[0], src.size());
  }
};

class OfstreamWriter : public Writer {
  ofstream f;
  string filename;
public:
  OfstreamWriter(string filename) : f(filename.c_str(), ios::out | ios::binary), filename(filename) {
    if (!f.good()) throw_error("Error opening %s for writing\n", filename.c_str());
  }    
  virtual void write(const unsigned char *src, size_t length) {
    f.write((char*)src, length);
    if (f.fail()) {
      throw_error("Error writing %zd bytes to file %s\n", length, filename.c_str());
    }
  }
};

/*
Options:

File-at-once:
 Read file into memory
 Parse header
 Decompress, or point to, frames only when requested, e.g. frameptr(frame) returns uncompressed

 Adv: large read (e.g. 250MB)
 Disadv: large read: higher latency, wasted effort when reading only a few frames, e.g. tour

Frames-on-demand:
 Read header into memory
 Parse header
 Read, and optionally decompress, a frame only when requested

 Adv: lower latency, no wasted effort
 Disadv: small read (e.g. 400K for uncompressed, 20K for JPEG-compressed)
 Consider reading multiple frames at once
*/

struct PixelInfo {
  unsigned int bands_per_pixel;
  unsigned int bits_per_band;
  unsigned int pixel_format; // 0=unsigned integer, 1=floating point,
  int bytes_per_pixel() { return bands_per_pixel * bits_per_band / 8; }

  double get_pixel_ch(const unsigned char *pixel, unsigned ch) const {
    switch ((bits_per_band << 1) | pixel_format) {
    case ((8 << 1) | 0):
      return ((unsigned char *)pixel)[ch];
    case ((16 << 1) | 0):
      return ((unsigned short *)pixel)[ch];
    case ((32 << 1) | 0):
      return ((unsigned int *)pixel)[ch];
    case ((32 << 1) | 1):
      return ((float *)pixel)[ch];
    case ((64 << 1) | 1):
      return ((double *)pixel)[ch];
    default:
      throw_error("Can't read pixel type %d:%d", bits_per_band, pixel_format);
    }
  }

  static double limit(double x, double minval, double maxval) {
    return max(min(x, maxval), minval);
  }

  void set_pixel_ch(unsigned char *pixel, unsigned ch, double val) const {
    switch ((bits_per_band << 1) | pixel_format) {
    case ((8 << 1) | 0):
      ((unsigned char *)pixel)[ch] = (unsigned char)round(limit(val, 0x00, 0xff));
      break;
    case ((16 << 1) | 0):
      ((unsigned short *)pixel)[ch] = (unsigned short)round(limit(val, 0x0000, 0xffff));
      break;
    case ((32 << 1) | 0):
      ((unsigned int *)pixel)[ch] = (unsigned int)round(limit(val, 0x00000000, 0xffffffff));
      break;
    case ((32 << 1) | 1):
      ((float *)pixel)[ch] = (float)val;
      break;
    case ((64 << 1) | 1):
      ((double *)pixel)[ch] = val;
      break;
    default:
      throw_error("Can't write pixel type %d:%d", bits_per_band, pixel_format);
      break;
    }
  }
};

struct TilestackInfo : public PixelInfo {
  unsigned int nframes;
  unsigned int tile_width;
  unsigned int tile_height;
  unsigned int compression_format;
  
};

class Tilestack : public TilestackInfo {
public:
  struct TOCEntry {
    double timestamp;
    unsigned long long address, length;
  };
  vector<TOCEntry> toc;
  std::vector<unsigned char*> pixels;

public:
  double frame_timestamp(unsigned frame) {
    assert(frame < nframes);
    toc[frame].timestamp;
  }
  unsigned char *frame_pixels(unsigned frame) {
    assert(frame < nframes);
    if (!pixels[frame]) instantiate_pixels(frame);
    return pixels[frame];
  }
  unsigned char *frame_pixel(unsigned frame, unsigned x, unsigned y) {
    return frame_pixels(frame) + bytes_per_pixel() * (x + y * tile_width);
  }
  virtual void instantiate_pixels(unsigned frame) = 0;
  virtual ~Tilestack() {}
};

// TODO: support compressed formats by splitting all_pixels into multiple frames
class ResidentTilestack : public Tilestack {
  vector<unsigned char> all_pixels;
  size_t tile_size;
public:
  ResidentTilestack(unsigned int nframes, unsigned int tile_width,
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

  void write(Writer &w) {
    vector<unsigned char> header(8);
    write_u64(&header[0], 0x326b7473656c6974); // ASCII 'tilestk2'
    w.write(header);
    w.write(all_pixels);
    size_t tocentry_size = 24;
    vector<unsigned char> tocdata(tocentry_size * nframes);
    size_t address = header.size();
    for (unsigned i = 0; i < nframes; i++) {
      write_double64(&tocdata[i*tocentry_size +  0], toc[i].timestamp);
      write_u64(&tocdata[i*tocentry_size +  8], address);
      write_u64(&tocdata[i*tocentry_size + 16], tile_size);
      address += tile_size;
    }
    w.write(tocdata);
    size_t footer_size = 48;
    vector<unsigned char> footer(footer_size);
    
    write_u64(&footer[ 0], nframes);
    write_u64(&footer[ 8], tile_width);
    write_u64(&footer[16], tile_height);
    write_u32(&footer[24], bands_per_pixel);
    write_u32(&footer[28], bits_per_band);
    write_u32(&footer[32], pixel_format);
    write_u32(&footer[36], compression_format);
    write_u64(&footer[40], 0x646e65326b747374); // ASCII: 'tstk2end'
    w.write(footer);
  }
  
  virtual void instantiate_pixels(unsigned frame) {
    assert(0); // we instantiate all the pixels in our constructor
  }
};

class TilestackReader : public Tilestack {
public:
  auto_ptr<Reader> reader;
  TilestackReader(auto_ptr<Reader> reader) : reader(reader) {
    read();
    pixels.resize(nframes);
  }
  
  virtual void instantiate_pixels(unsigned frame) {
    // TODO: if this runs out of RAM, consider LRU implementation
    assert(!pixels[frame]);
    pixels[frame] = new unsigned char[toc[frame].length];
    reader->read(pixels[frame], toc[frame].address, toc[frame].length);
  }
  virtual ~TilestackReader() {
    for (unsigned i = 0; i < nframes; i++) {
      if (pixels[i]) {
        delete[] pixels[i];
        pixels[i] = 0;
      }
    }
  }

  protected:
  void read() {
    size_t footer_size = 48;
    size_t filelen = reader->length();
    vector<unsigned char> footer = reader->read(filelen - footer_size, footer_size);
    nframes =      (unsigned int) read_u64(&footer[ 0]);
    tile_width =   (unsigned int) read_u64(&footer[ 8]);
    tile_height =  (unsigned int) read_u64(&footer[16]);
    bands_per_pixel =             read_u32(&footer[24]);
    bits_per_band =               read_u32(&footer[28]);
    pixel_format =                read_u32(&footer[32]);
    compression_format =          read_u32(&footer[36]);
    unsigned long long magic =    read_u64(&footer[40]);
    assert(magic == 0x646e65326b747374);
    
    size_t tocentry_size = 24;
    size_t toclen = tocentry_size * nframes;
    vector<unsigned char> tocdata = reader->read(filelen - footer_size - toclen, toclen);
    toc.resize(nframes);
    for (unsigned i = 0; i < nframes; i++) {
      toc[i].timestamp = read_double_64(&tocdata[i*tocentry_size +  0]);
      toc[i].address =   read_u64      (&tocdata[i*tocentry_size +  8]);
      toc[i].length =    read_u64      (&tocdata[i*tocentry_size + 16]);
    }
  }

};
  
template <typename T>
class AutoPtrStack {
  list<T*> stack;
public:
  void push(auto_ptr<T> t) {
    stack.push_back(t.release());
  }
  auto_ptr<T> pop() {
    T* ret = stack.back();
    stack.pop_back();
    return auto_ptr<T>(ret);
  }
};

AutoPtrStack<Tilestack> tilestackstack;

void load(string filename)
{
  tilestackstack.push(auto_ptr<Tilestack>(new TilestackReader(auto_ptr<Reader>(new IfstreamReader(filename)))));
}

typedef unsigned char u8;
typedef unsigned short u16;

template <typename T> struct RGBA { T r, g, b, a; };
template <typename T> struct RGB { T r, g, b; };

u8 viz_channel(u16 in, double min, double max, double gamma) {
  double ret = (in - min) * 255 / (max - min);
  if (ret < 0) ret = 0;
  if (ret > 255) ret = 255;
  return (u8)lround(ret);
}

void viz(double min, double max, double gamma) {
  auto_ptr<Tilestack> src(tilestackstack.pop());
  auto_ptr<Tilestack> dest(
    new ResidentTilestack(src->nframes, src->tile_width, src->tile_height,
                          src->bands_per_pixel, 8, 0, 0));
  assert(src->bands_per_pixel == 4);
  assert(src->bits_per_band == 16);
  for (unsigned frame = 0; frame < src->nframes; frame++) {
    dest->toc[frame].timestamp = src->toc[frame].timestamp;
    RGBA<u16> *srcptr = (RGBA<u16>*) src->frame_pixels(frame);
    RGBA<u8> *destptr = (RGBA<u8>*) dest->frame_pixels(frame);
    for (unsigned y = 0; y < src->tile_height; y++) {
      for (unsigned x = 0; x < src->tile_width; x++, srcptr++, destptr++) {
        destptr->r = viz_channel(srcptr->r, min, max, gamma);
        destptr->g = viz_channel(srcptr->g, min, max, gamma);
        destptr->b = viz_channel(srcptr->b, min, max, gamma);
        destptr->a = viz_channel(srcptr->a, 0, 65535, 1);
      }
    }
  }
  tilestackstack.push(dest);
}

void write_html(string dest)
{
  auto_ptr<Tilestack> src(tilestackstack.pop());
  string dir = filename_sans_suffix(dest);
  mkdir(dir.c_str(), 0777);

  string html_filename = filename_sans_suffix(dest) + ".html";
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
    string image_filename = string_printf("%04d.png", i);
    string image_path = dir + "/" + image_filename;
    string html_image_path = filename_sans_directory(dir) + "/" + image_filename;
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

void save(string dest)
{
  auto_ptr<Tilestack> tmp(tilestackstack.pop());
  ResidentTilestack *src = dynamic_cast<ResidentTilestack*>(tmp.get());
  if (!src) throw_error("Can only save type ResidentTilestack (do an operation after reading before writing)");

  if (create_parent_directories) make_directory_and_parents(filename_directory(dest));

  string temp_dest = temporary_path(dest);

  {
    OfstreamWriter out(temp_dest);
    src->write(out);
  }
  
  if (rename(temp_dest.c_str(), dest.c_str())) {
    throw_error("Can't rename %s to %s", temp_dest.c_str(), dest.c_str());
  }
} 

// OSX: Download ffmpeg binary from ffmpegmac.net
class FfmpegEncoder {
  int width, height;
  FILE *out;
public:
  FfmpegEncoder(string dest_filename, int width, int height,
                double fps, double compression) :
    width(width), height(height) {
    int nthreads = 8;
    string cmdline = string_printf("%s -threads %d", path_to_ffmpeg().c_str(), nthreads);
    // Input
    cmdline += string_printf(" -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r %g -i pipe:0",
                             width, height, fps);
    // Output
    int frames_per_keyframe = 10;
    cmdline += " -vcodec libx264";
    //cmdline += string_printf(" -fpre libx264-hq.ffpreset");
    cmdline += " -coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -keyint_min 25 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -refs 4 -directpred 3 -trellis 1 -flags2 +wpred+mixed_refs+dct8x8+fastpskip";
      
    cmdline += string_printf(" -crf %g -g %d -bf 0 %s",
                             compression, frames_per_keyframe, dest_filename.c_str());
    fprintf(stderr, "Cmdline: %s\n", cmdline.c_str());
    setenv("AV_LOG_FORCE_NOCOLOR", "1", 1);
    unlink(dest_filename.c_str());
    out = popen(cmdline.c_str(), "w");
    if (!out) {
      throw_error("Error trying to run ffmpeg.  Make sure it's installed and in your path\n"
		  "Tried with this commandline:\n"
		  "%s\n", cmdline.c_str());
    }
  }

  void write_pixels(unsigned char *pixels, size_t len) {
    fprintf(stderr, "Writing %zd bytes to ffmpeg\n", len);
    if (1 != fwrite(pixels, len, 1, out)) {
      throw_error("Error writing to ffmpeg");
    }
  }
  
  void close() {
    if (out) pclose(out);
    out = NULL;
  }

  static string path_to_ffmpeg() {
    string colocated = string_printf("%s/ffmpeg/%s/ffmpeg", 
				     filename_directory(executable_path()).c_str(),
				     os().c_str());
    if (filename_exists(colocated)) return colocated;
    return "ffmpeg";
  }
};
  
void write_video(string dest, double fps, double compression)
{
  auto_ptr<Tilestack> src(tilestackstack.pop());
  string temp_dest = temporary_path(dest);
  
  fprintf(stderr, "Encoding video to %s\n", temp_dest.c_str());
  FfmpegEncoder encoder(temp_dest, src->tile_width, src->tile_height, fps, compression);
  assert(src->bands_per_pixel >= 3 && src->bits_per_band == 8);
  vector<unsigned char> destframe(src->tile_width * src->tile_height * 3);
  for (unsigned i = 0; i < src->nframes; i++) {
    unsigned char *srcptr = src->frame_pixels(i);
    unsigned char *destptr = &destframe[0];
    for (unsigned y = 0; y < src->tile_height; y++) {
      for (unsigned x = 0; x < src->tile_width; x++) {
        *destptr++ = srcptr[0];
        *destptr++ = srcptr[1];
        *destptr++ = srcptr[2];
        srcptr += src->bands_per_pixel;
      }
    }
    encoder.write_pixels(&destframe[0], destframe.size());
  }
  encoder.close();
  fprintf(stderr, "Renaming %s to %s\n", temp_dest.c_str(), dest.c_str());
  if (rename(temp_dest.c_str(), dest.c_str())) {
    throw_error("Can't rename %s to %s", temp_dest.c_str(), dest.c_str());
  }
}

int compute_tile_nlevels(int width, int height, int tile_width, int tile_height) {
  int max_level = 0;
  while (width > (tile_width << max_level) || height > (tile_height << max_level)) {
    max_level++;
  }
  return max_level + 1;
}

void image2tiles(string dest, string format, string src)
{
  if (filename_exists(dest)) {
    fprintf(stderr, "%s already exists, skipping\n", dest.c_str());
    return;
  }
  auto_ptr<ImageReader> reader(ImageReader::open(src));
  fprintf(stderr, "Opened %s: %d x %d pixels\n", src.c_str(), reader->width(), reader->height());

  vector<unsigned char> stripe(reader->bytes_per_row() * tilesize);
  vector<unsigned char> tile(reader->bytes_per_pixel() * tilesize * tilesize);

  int max_level = compute_tile_nlevels(reader->width(), reader->height(), tilesize, tilesize);
  string temp_dest = temporary_path(dest);
  
  make_directory_and_parents(temp_dest);

  {
    Json::Value r;
    r["width"] = reader->width();
    r["height"] = reader->height();
    r["tile_width"] = r["tile_height"] = tilesize;
    string jsonfile = temp_dest + "/r.json";
    ofstream jsonout(jsonfile.c_str());
    if (!jsonout.good()) throw_error("Error opening %s for writing", jsonfile.c_str());
    jsonout << r;
  }
  
  for (unsigned top = 0; top < reader->height(); top += tilesize) {
    unsigned nrows = min(reader->height() - top, tilesize);
    fill(stripe.begin(), stripe.end(), 0);
    reader->read_rows(&stripe[0], nrows);
    for (unsigned left = 0; left < reader->width(); left += tilesize) {
      unsigned ncols = min(reader->width() - left, tilesize);
      for (unsigned y = 0; y < tilesize; y++) {
      	memcpy(&tile[y * reader->bytes_per_pixel() * tilesize], 
      	       &stripe[y * reader->bytes_per_row() + left * reader->bytes_per_pixel()],
      	       ncols * reader->bytes_per_pixel());
      }
      
      string path = temp_dest + "/" + GPTileIdx(max_level - 1, left/tilesize, top/tilesize).path() + "." + format;
      string directory = filename_directory(path);
      make_directory_and_parents(directory);
      
      ImageWriter::write(path, tilesize, tilesize, reader->bands_per_pixel(), reader->bits_per_band(),
			 &tile[0]);
    }
  }
  
  if (rename(temp_dest.c_str(), dest.c_str())) {
    fprintf(stderr, "Error renaming %s to %s\n", temp_dest.c_str(), dest.c_str());
    // TODO(RS): Delete temporary directory
  }
}

void load_tiles(const vector<string> &srcs)
{
  assert(srcs.size() > 0);
  auto_ptr<ImageReader> tile0(ImageReader::open(srcs[0]));

  auto_ptr<Tilestack> dest(new ResidentTilestack(srcs.size(), tile0->width(), tile0->height(),
						 tile0->bands_per_pixel(), tile0->bits_per_band(), 0, 0));

  for (unsigned frame = 0; frame < dest->nframes; frame++) {
    dest->toc[frame].timestamp = 0;
    auto_ptr<ImageReader> tile(ImageReader::open(srcs[frame]));
    assert(tile->width() == dest->tile_width);
    assert(tile->height() == dest->tile_height);
    assert(tile->bands_per_pixel() == dest->bands_per_pixel);
    assert(tile->bits_per_band() == dest->bits_per_band);
    
    tile->read_rows(dest->frame_pixels(frame), tile->height());
  }
  tilestackstack.push(dest);
}

struct Bbox {
  double x, y;
  double width, height;
  Bbox(double x, double y, double width, double height) : x(x), y(y), width(width), height(height) {}
};

struct Frame {
  int frameno;
  Bbox bounds;
  Frame(int frameno, const Bbox &bounds) : frameno(frameno), bounds(bounds) {}
};
	       
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

class Stackset : public TilestackInfo {
protected:  
  string stackset_path;
  Json::Value info;
  int width, height;
  int nlevels;
  map<GPTileIdx, TilestackReader* > readers;
  
public:
  Stackset(const string &stackset_path) : stackset_path(stackset_path) {
    string json_path = stackset_path + "/r.json";
    Json::Reader reader;
    ifstream json_in(json_path.c_str());
    if (!json_in.good()) {
      throw_error("Can't open %s for reading", json_path.c_str());
    }
    if (!reader.parse(json_in, info)) {
      throw_error("Can't parse %s as JSON", json_path.c_str());
    }
    width = info["width"].asInt();
    height = info["height"].asInt();
    tile_width = info["tile_width"].asInt();
    tile_height = info["tile_height"].asInt();

    nlevels = compute_tile_nlevels(width, height, tile_width, tile_height);
    (TilestackInfo&)(*this) = (TilestackInfo&)(*get_reader(GPTileIdx(nlevels-1, 0, 0)));
  }
  
  TilestackReader *get_reader(const GPTileIdx &idx) {
    if (readers.find(idx) == readers.end()) {
      // TODO(RS): If this starts running out of RAM, consider LRU on the readers
      string path = stackset_path + "/" + idx.path() + ".ts2";
      
      try {
	readers[idx] = new TilestackReader(auto_ptr<Reader>(new IfstreamReader(path)));
      } catch (runtime_error) {
	readers[idx] = NULL;
      }
    }
    return readers[idx];
  }

  double interpolate(double val, double in_min, double in_max, double out_min, double out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  void get_pixel(unsigned char *dest, int frame, int level, int x, int y) {
    if (x >= 0 && y >= 0) {
      int tile_x = x / tile_width;
      int tile_y = y / tile_height;
      TilestackReader *reader = get_reader(GPTileIdx(level, tile_x, tile_y));
      if (reader) {
	memcpy(dest, reader->frame_pixel(frame, x % tile_width, y % tile_height), bytes_per_pixel());
	return;
      }
    }
    memset(dest, 0, bytes_per_pixel());
  }

  // x and y must vary from 0 to 1
  double bilinearly_interpolate(double a00, double a01, double a10, double a11, double x, double y) {
    return 
      a00 * (1-x) * (1-y) + 
      a01 * (1-x) *   y   +
      a10 *   x   * (1-y) +
      a11 *   x   *   y;
  }

  void interpolate_pixel(unsigned char *dest, int frame, int level, double x, double y) {
    vector<unsigned char> p00_store(bytes_per_pixel());
    vector<unsigned char> p01_store(bytes_per_pixel());
    vector<unsigned char> p10_store(bytes_per_pixel());
    vector<unsigned char> p11_store(bytes_per_pixel());
    
    unsigned char *p00 = &p00_store[0];
    unsigned char *p01 = &p01_store[0];
    unsigned char *p10 = &p10_store[0];
    unsigned char *p11 = &p11_store[0];
    
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);
    
    get_pixel(p00, frame, level, x0+0, y0+0);
    get_pixel(p01, frame, level, x0+0, y0+1);
    get_pixel(p10, frame, level, x0+1, y0+0);
    get_pixel(p11, frame, level, x0+1, y0+1);
    
    for (unsigned ch = 0; ch < bands_per_pixel; ch++) {
      set_pixel_ch(dest, ch, bilinearly_interpolate(get_pixel_ch(p00, ch), 
						    get_pixel_ch(p01, ch), 
						    get_pixel_ch(p10, ch), 
						    get_pixel_ch(p11, ch), 
						    x - x0, y - y0));
    }
  }
  
  // frame coords:  x and y are the center of the upper-left pixel
  void render_image(Image &dest, const Frame &frame) {
    // scale between original pixels and desination frame.  If less than one, we subsample (sharp), or greater than
    // one means supersample (blurry)
    fprintf(stderr, "dest.width = %d, frame.bounds.width = %g\n", dest.width, frame.bounds.width);
    double scale = dest.width / frame.bounds.width;
    double cutoff = 0.999;
    double subsample_nlevels = log(cutoff / scale)/log(2.0);
    double source_level_d = (nlevels - 1) - subsample_nlevels;
    int source_level = max((int)ceil(source_level_d), nlevels - 1);
    if (source_level != nlevels-1) TODO("need to subsample frame here");
    fprintf(stderr, "source_level_d = %g, source_level = %d\n", source_level_d, source_level);
    for (int y = 0; y < dest.height; y++) {
      double source_y = interpolate(y, 0, dest.height-1, frame.bounds.y, frame.bounds.y+frame.bounds.height-1);
      for (int x = 0; x < dest.width; x++) {
	double source_x = interpolate(x, 0, dest.width-1, frame.bounds.x, frame.bounds.x+frame.bounds.width-1);
	interpolate_pixel(dest.pixel(x,y), frame.frameno, source_level, source_x, source_y);
      }
    }
  }
  
  ~Stackset() {
    for (map<GPTileIdx, TilestackReader*>::iterator i = readers.begin(); i != readers.end(); ++i) {
      if (i->second) delete i->second;
      i->second = NULL;
    }
  }
};

void parse_path(vector<Frame> &frames, Json::Value path) {
  // Iterate through an array of frames.  If we don't receive an array, assume we have a single frame path
  int len = path.isArray() ? path.size() : 1;
  for (int i = 0; i < len; i++) {
    Json::Value frame_desc = path.isArray() ? path[i] : path;
    if (!frame_desc.isObject()) {
      throw_error("Syntax error translating frame %d (not a JSON object)", i);
    }
    Bbox bounds(frame_desc["bounds"]["xmin"].asDouble(),
		frame_desc["bounds"]["ymin"].asDouble(),
		frame_desc["bounds"]["width"].asDouble(),
		frame_desc["bounds"]["height"].asDouble());
    if (!frame_desc["frames"].isNull()) {
      int step = frame_desc["frames"]["step"].isNull() ? 1 : frame_desc["frames"]["step"].asInt();
      for (int j = frame_desc["frames"]["start"].asInt(); true; j += step) {
	frames.push_back(Frame(j, bounds));
	if (j == frame_desc["frames"]["end"].asInt()) break;
      }
    } else {
      throw_error("Don't yet know how to parse a path segment without a 'frames' field, sorry");
    }
  }
}

void path2stack(int stack_width, int stack_height, Json::Value path, const string &stackset_path) {
  Stackset stackset(stackset_path);
  vector<Frame> frames;
  parse_path(frames, path);
  fprintf(stderr, "got %zd frames\n", frames.size());
  auto_ptr<Tilestack> out(new ResidentTilestack(frames.size(), stack_width, stack_height, 
						stackset.bands_per_pixel, stackset.bits_per_band,
						stackset.pixel_format, 0));
  for (unsigned i = 0; i < frames.size(); i++) {
    Image image(stackset, stack_width, stack_height, out->frame_pixels(i));
    stackset.render_image(image, frames[i]);
  }
  tilestackstack.push(out);
}

void usage(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, 
	  "\nUsage:\n"
	  "tilestacktool [args]\n"
	  "--load src.ts2\n"
	  "--save dest.ts2\n"
	  "--viz min max gamma\n"
	  "--writehtml dest.html\n"
	  "--writevideo dest.mp4 fps compression\n"
	  "              28=typical, 24=high quality, 32=low quality\n"
	  "--image2tiles dest_dir format src_image\n"
	  "              Be sure to set tilesize earlier in the commandline\n"
	  "--tilesize N\n"
	  "--loadtiles src_image0 src_image1 ... src_imageN\n"
	  "--delete-source-tiles\n"
	  "--path2stack width height [frame1, ... frameN] stackset\n"
	  "        Frame format {\"frameno\":N, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
	  "        Multiframe with single bounds. from and to are both inclusive.  step defaults to 1:\n"
          "          {\"frames\":{\"from\":N, \"to\":N, \"step\":N], \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
	  );
  exit(1);
}

int main(int argc, char **argv)
{
  try {
    Arglist args(argv+1, argv+argc);
    while (!args.empty()) {
      std::string arg = args.shift();
      if (arg == "--load") {
	string src = args.shift();
	if (filename_suffix(src) != "ts2") {
	  usage("Filename to load should end in '.ts2'");
	}
	load(src);
      }
      else if (arg == "--save") {
	string dest = args.shift();
	if (filename_suffix(dest) != "ts2") {
	  usage("Filename to save should end in '.ts2'");
	}
	save(dest);
      }
      else if (arg == "--viz") {
	double min = args.shift_double();
	double max = args.shift_double();
	double gamma = args.shift_double();
	viz(min, max, gamma);
      }
      else if (arg == "--writehtml") {
	string dest = args.shift();
	write_html(dest);
      }
      else if (arg == "--writevideo") {
	string dest = args.shift();
	double fps = args.shift_double();
	double compression = args.shift_double();
	write_video(dest, fps, compression);
      }
      else if (arg == "--tilesize") {
	tilesize = args.shift_int();
      }
      else if (arg == "--image2tiles") {
	string dest = args.shift();
	string format = args.shift();
	string src = args.shift();
	image2tiles(dest, format, src);
      }
      else if (arg == "--loadtiles") {
	vector<string> srcs;
	while (!args.empty() && args.front().substr(0,1) != "-") {
	  srcs.push_back(args.shift());
	}
	if (srcs.empty()) {
	  usage("--loadtiles must have at least one tile");
	}
	load_tiles(srcs);
      }
      else if (arg == "--delete-source-tiles") {
	delete_source_tiles = true;
      }
      else if (arg == "--create-parent-directories") {
	create_parent_directories = true;
      }
      else if (arg == "--path2stack") {
	int stack_width = args.shift_int();
	int stack_height = args.shift_int();
	Json::Value path = args.shift_json();
	string stackset = args.shift();
	if (stack_width <= 0 || stack_height <= 0) {
	  usage("--path2stack: width and height must be positive numbers");
	}
	path2stack(stack_width, stack_height, path, stackset);
      }
      else usage("Unknown argument %s", arg.c_str());
    }
    if (source_tiles_to_delete.size()) {
      fprintf(stderr, "Deleting %zd source tiles\n", source_tiles_to_delete.size());
      for (unsigned i = 0; i < source_tiles_to_delete.size(); i++) {
	unlink(source_tiles_to_delete[i].c_str());
      }
    }
    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error: %s\n", e.what());
    return 1;
  }
}
