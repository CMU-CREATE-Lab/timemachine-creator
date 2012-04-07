#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "utils.h"
#include "png_util.h"

using namespace std;
auto_ptr<int> foo;

void usage(const char *fmt, ...);

void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\nAborting\n");
  exit(1);
}

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
    return atof(arg.c_str());
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
  IfstreamReader(string filename) : f(filename.c_str()), filename(filename) {
    if (f.bad()) die("Error opening %s for reading\n", filename.c_str());
  }    
  virtual void read(unsigned char *dest, size_t pos, size_t length) {
    f.seekg(pos, ios_base::beg);
    f.read((char*)dest, length);
    if (f.bad()) {
      die("Error reading %lld bytes from file %s at position %lld\n",
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
  OfstreamWriter(string filename) : f(filename.c_str()), filename(filename) {
    if (f.bad()) die("Error opening %s for writing\n", filename.c_str());
  }    
  virtual void write(const unsigned char *src, size_t length) {
    f.write((char*)src, length);
    if (f.bad()) {
      die("Error writing %lld bytes to file %s\n", length, filename.c_str());
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

void write_u32(unsigned char *dest, unsigned int src) {
  dest[0] = src >>  0;
  dest[1] = src >>  8;
  dest[2] = src >> 16;
  dest[3] = src >> 24;
}

void write_u64(unsigned char *dest, unsigned long long src) {
  write_u32(dest + 0, (unsigned int) src);
  write_u32(dest + 4, (unsigned int) (src >> 32));
}

void write_double64(unsigned char *dest, double src) {
  assert(sizeof(src) == 8);
  assert(sizeof(unsigned long long) == 8);
  write_u64(dest, *(unsigned long long*)&src);
}

unsigned int read_u32(unsigned char *src) {
  return src[0] + (src[1]<<8) + (src[2]<<16) + (src[3]<<24);
}

unsigned long long read_u64(unsigned char *src) {
  return read_u32(src) + (((unsigned long long)read_u32(src + 4)) << 32);
}

double read_double_64(unsigned char *src) {
  unsigned long long d = read_u64(src);
  assert(sizeof(d) == 8);
  assert(sizeof(double) == 8);
  return *(double*)&d;
}

class Tilestack {
public:
  struct TOCEntry {
    double timestamp;
    unsigned long long address, length;
  };
  vector<TOCEntry> toc;
  unsigned int nframes;
  unsigned int tile_width;
  unsigned int tile_height;
  unsigned int bands_per_pixel;
  unsigned int bits_per_band;
  unsigned int pixel_format;
  unsigned int compression_format;
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
  virtual void instantiate_pixels(unsigned frame) = 0;
  virtual ~Tilestack() {}
};

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
    string image_filename = string_printf("%s/%04d.png", dir.c_str(), i);
    fprintf(stderr, "Writing %s\n", image_filename.c_str());
    write_png(image_filename.c_str(), src->tile_width, src->tile_height,
              src->bands_per_pixel, src->bits_per_band, src->frame_pixels(i));
    fprintf(html, "<div><img src=\"%s\"><br>%04d</div>\n", image_filename.c_str(), i);
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
  if (!src) die("Can only save type ResidentTilestack (do an operation after reading before writing)");

  OfstreamWriter out(dest);
  
  src->write(out);
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
    string cmdline = string_printf("./ffmpeg -threads %d", nthreads);
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
      die("Error trying to run ffmpeg.  Make sure it's installed and in your path\n"
          "Tried with this commandline:\n"
          "%s\n", cmdline.c_str());
    }
  }

  void write_pixels(unsigned char *pixels, size_t len) {
    fprintf(stderr, "Writing %zd bytes to ffmpeg\n", len);
    if (1 != fwrite(pixels, len, 1, out)) {
      die("Error writing to ffmpeg");
    }
  }
  
  void close() {
    if (out) pclose(out);
    out = NULL;
  }
};
  
void write_video(string dest, double fps, double compression)
{
  auto_ptr<Tilestack> src(tilestackstack.pop());
  FfmpegEncoder encoder(dest, src->tile_width, src->tile_height, fps, compression);
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
}

void usage(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\nUsage:\n");
  fprintf(stderr, "tilestacktool [args]\n");
  fprintf(stderr, "--load src.ts2\n");
  fprintf(stderr, "--save dest.ts2\n");
  fprintf(stderr, "--viz min max gamma\n");
  fprintf(stderr, "--writehtml dest.html\n");
  fprintf(stderr, "--writevideo dest.mp4 fps compression\n");
  fprintf(stderr, "              28=typical, 24=high quality, 32=low quality\n");
  exit(1);
}
  
int main(int argc, char **argv)
{
  Arglist args(argv+1, argv+argc);
  while (!args.empty()) {
    std::string arg = args.shift();
    if (arg == "--load") {
      string src = args.shift();
      load(src);
    }
    else if (arg == "--save") {
      string dest = args.shift();
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
    else usage("Unknown argument %s", arg.c_str());
  }
  return 0;
}
