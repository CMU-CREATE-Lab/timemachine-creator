#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
#include "qt-faststart.h"

#include "io.h"

#include "Tilestack.h"
#include "tilestacktool.h"

#define TODO(x) do { fprintf(stderr, "%s:%d: error: TODO %s\n", __FILE__, __LINE__, x); abort(); } while (0)

unsigned int tilesize = 512;
bool create_parent_directories = false;
bool delete_source_tiles = false;
std::vector<std::string> source_tiles_to_delete;

void usage(const char *fmt, ...);

const char *version() { return "0.2.1"; }

class LRUTilestack : public Tilestack {
  unsigned int lru_size;
  std::list<int> lru;

public:
  LRUTilestack() : lru_size(5) {}
  
  // Not really deleting the LRU, but rather the least recently created
  // (to avoid the overhead of recording use)
  void delete_lru() {
    delete[] pixels[lru.back()];
    pixels[lru.back()] = 0;
    lru.pop_back();
  }

  virtual void create(unsigned frame) {
    while (lru.size() > lru_size) delete_lru();
    lru.push_front(frame);
    pixels[frame] = new unsigned char[bytes_per_frame()];
  }

  virtual ~LRUTilestack() {
    while (!lru.empty()) delete_lru();
  }
};

class TilestackReader : public LRUTilestack {
public:
  std::auto_ptr<Reader> reader;
  TilestackReader(std::auto_ptr<Reader> reader) : reader(reader) {
    read();
  }

  virtual void instantiate_pixels(unsigned frame) {
    assert(!pixels[frame]);
    create(frame);
    reader->read(pixels[frame], toc[frame].address, toc[frame].length);
  }

  virtual ~TilestackReader() {}

  protected:
  void read() {
    size_t footer_size = 48;
    size_t filelen = reader->length();
    std::vector<unsigned char> footer = reader->read(filelen - footer_size, footer_size);
    set_nframes(   (unsigned int) read_u64(&footer[ 0]));
    tile_width =   (unsigned int) read_u64(&footer[ 8]);
    tile_height =  (unsigned int) read_u64(&footer[16]);
    bands_per_pixel =             read_u32(&footer[24]);
    bits_per_band =               read_u32(&footer[28]);
    pixel_format =                read_u32(&footer[32]);
    compression_format =          read_u32(&footer[36]);
    unsigned long long magic =    read_u64(&footer[40]);
    if (magic != 0x646e65326b747374LL) {
      fprintf(stderr, "Incorrect magic (%08x:%08x)", (unsigned int)(magic >> 32), (unsigned int)magic);
      assert(0);
    }

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

AutoPtrStack<Tilestack> tilestackstack;

void load(std::string filename)
{
  tilestackstack.push(std::auto_ptr<Tilestack>(new TilestackReader(std::auto_ptr<Reader>(FileReader::open(filename)))));
}

// gamma > 1 brightens midtones; < 1 darkens midtones

class VizBand {
  double one_over_gamma;
  double maxval;
  double scale_over_maxval;

public:
  VizBand(Json::Value params, unsigned int band) {
    double gamma = get_param(params, "gamma", band, 1);
    one_over_gamma = 1.0 / gamma;
    double scale = get_param(params, "gain", band, 1);
    // To get full and correct dynamic range, maxval should be 256 for 8-bit values (not 255)
    maxval = get_param(params, "maxval", band, 256);
    scale_over_maxval = scale / maxval;
  }

  double get_param(Json::Value params, const char *name, unsigned int band, double default_val) {
    Json::Value param = params[name];
    if (param.isNull()) {
      return default_val;
    } else if (param.isArray()) {
      if (band >= param.size()) {
        throw_error("Too few values to viz:%s (must have at least the number of image bands)", name);
      }
      return param[band].asDouble();
    } else {
      return param.asDouble();
    }
  }

  double apply(double val) {
    return maxval * pow(std::max(0.0, val * scale_over_maxval), one_over_gamma);
  }
};

class VizTilestack : public LRUTilestack {
  std::auto_ptr<Tilestack> src;
  std::vector<VizBand> viz_bands;

public:
  VizTilestack(std::auto_ptr<Tilestack> src, Json::Value params) : src(src) {
    
    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->src.get());
    for (unsigned i = 0; i < bands_per_pixel; i++) {
      viz_bands.push_back(VizBand(params, i));
    }
    set_nframes(this->src->nframes);
  }
  
  virtual void instantiate_pixels(unsigned frame) {
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

void viz(Json::Value params) {
  std::auto_ptr<Tilestack> src(tilestackstack.pop());
  tilestackstack.push(std::auto_ptr<Tilestack>(new VizTilestack(src, params)));
}

void ensure_resident() {
  std::auto_ptr<Tilestack> src(tilestackstack.pop());
  if (dynamic_cast<ResidentTilestack*>(src.get())) {
    // Already resident
    tilestackstack.push(src);
  } else {
    std::auto_ptr<Tilestack> copy(
              new ResidentTilestack(src->nframes, src->tile_width, src->tile_height,
                                    src->bands_per_pixel, src->bits_per_band, src->pixel_format, 0));
    for (unsigned frame = 0; frame < src->nframes; frame++) {
      memcpy(copy->frame_pixels(frame), src->frame_pixels(frame), src->bytes_per_frame());
    }
    tilestackstack.push(copy);
  }
}

void write_html(std::string dest)
{
  std::auto_ptr<Tilestack> src(tilestackstack.pop());
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
  ensure_resident();
  std::auto_ptr<Tilestack> tmp(tilestackstack.pop());
  ResidentTilestack *src = dynamic_cast<ResidentTilestack*>(tmp.get());
  assert(src);

  if (create_parent_directories) make_directory_and_parents(filename_directory(dest));

  std::string temp_dest = temporary_path(dest);

  {
    std::auto_ptr<FileWriter> out(FileWriter::open(temp_dest));
    src->write(out.get());
  }

  rename_file(temp_dest, dest);

  fprintf(stderr, "Created %s\n", dest.c_str());
}

// OSX: Download ffmpeg binary from ffmpegmac.net
class FfmpegEncoder {
  int width, height;
  FILE *out;
  size_t total_written;
  std::string tmp_filename;
  std::string dest_filename;
public:
  
  static bool test() {
    std::string cmdline = string_printf("\"%s\" -loglevel error -version", path_to_ffmpeg().c_str());
    FILE *ffmpeg = popen_utf8(cmdline.c_str(), "wb");
    if (!ffmpeg) {
      fprintf(stderr, "FfmpegEncoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
      return false;
    }
    while (fgetc(ffmpeg) != EOF) {}
    int status = pclose(ffmpeg);
    if (status) {
      fprintf(stderr, "FfmpegEncoder::test:  pclose '%s' returns status %d.  This is likely due to an installation problem.\n", cmdline.c_str(), status);
      return false;
    }

    fprintf(stderr, "ffmpeg: success\n");

    // No longer using qt-faststart executable;  instead, using internal qt_faststart function    
    // cmdline = string_printf("\"%s\"", path_to_qt_faststart().c_str());
    // status = system(cmdline.c_str());
    // if (status != 0) {
    //   fprintf(stderr, "FfmpegEncoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
    //   return false;
    // }
    // fprintf(stderr, "qt-faststart: success\n");

    return true;
  }
  
  FfmpegEncoder(std::string dest_filename, int width, int height,
                double fps, double compression) :
    width(width), height(height), total_written(0), dest_filename(dest_filename) {
    tmp_filename = temporary_path("tmp.mp4");
    int nthreads = 8;
    std::string cmdline = string_printf("\"%s\" -threads %d -loglevel error -benchmark", path_to_ffmpeg().c_str(), nthreads);
    // Input
    cmdline += string_printf(" -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r %g -i pipe:0",
                             width, height, fps);
    // Output
    int frames_per_keyframe = 10;
    cmdline += " -vcodec libx264";
    //cmdline += string_printf(" -fpre libx264-hq.ffpreset");
    cmdline += " -coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -keyint_min 25 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -refs 4 -directpred 3 -trellis 1 -flags2 +wpred+mixed_refs+dct8x8+fastpskip";

    cmdline += string_printf(" -crf %g -g %d -bf 0 \"%s\"",
                             compression, frames_per_keyframe, tmp_filename.c_str());
    fprintf(stderr, "Cmdline: %s\n", cmdline.c_str());

    #ifdef _WIN32
      putenv("AV_LOG_FORCE_NOCOLOR=1");
    #else
      setenv("AV_LOG_FORCE_NOCOLOR", "1", 1);
    #endif

    unlink(dest_filename.c_str());
    out = popen_utf8(cmdline, "wb");
    if (!out) {
      throw_error("Error trying to run ffmpeg.  Make sure it's installed and in your path\n"
                  "Tried with this commandline:\n"
                  "%s\n", cmdline.c_str());
    }
  }

  void write_pixels(unsigned char *pixels, size_t len) {
    //fprintf(stderr, "Writing %zd bytes to ffmpeg\n", len);
    if (1 != fwrite(pixels, len, 1, out)) {
      throw_error("Error writing to ffmpeg");
    }
    total_written += len;
  }

  void close() {
    if (out) pclose(out);
    fprintf(stderr, "Wrote %ld frames (%ld bytes) to ffmpeg\n", 
            (long) (total_written / (width * height * 3)), (long) total_written);
    out = NULL;
    //std::string cmd = string_printf("\"%s\" \"%s\" \"%s\"", path_to_qt_faststart().c_str(), tmp_filename.c_str(), dest_filename.c_str());
    //if (system_utf8(cmd)) {
    //  throw_error("Error running qtfaststart: '%s'", cmd.c_str());
    //}
    qt_faststart(tmp_filename, dest_filename);
    delete_file(tmp_filename);
  }

  
  static std::string path_to_ffmpeg() {
    return path_to_executable("ffmpeg");
  }

  static std::string path_to_qt_faststart() {
    return path_to_executable("qt-faststart");
  }
    
  static std::string path_to_executable(std::string executable) {
    executable += executable_suffix();
    std::vector<std::string> search_path;
    search_path.push_back(string_printf("%s/ffmpeg/%s/%s",
					filename_directory(executable_path()).c_str(),
					os().c_str(),
					executable.c_str()));
    search_path.push_back(string_printf("%s/%s",
					filename_directory(executable_path()).c_str(),
					executable.c_str()));
    
    for (unsigned i = 0; i < search_path.size(); i++) {
      if (filename_exists(search_path[i])) {
	fprintf(stderr, "Found %s at path %s\n", executable.c_str(), search_path[i].c_str());
	return search_path[i];
      }
    }
    
    fprintf(stderr, "Could not find %s in search path:\n", executable.c_str());
    for (unsigned i = 0; i < search_path.size(); i++) {
      fprintf(stderr, "%s\n", search_path[i].c_str());
    }
    fprintf(stderr, "Using version in PATH, if available\n");
    return executable;
  }
};

// Adapted from George Marsaglia's Multiply With Carry, http://www.cse.yorku.ca/~oz/marsaglia-rng.html

struct MWC {
  unsigned int z; // 32 bits
  unsigned int w; // 32 bits
  MWC(int z_seed, int w_seed) : z(z_seed), w(w_seed) {
    assert(z && w);
  }
  unsigned int get() {
    z = 36969 * (z & 65535) + (z >> 16);
    w = 18000 * (w & 65535) + (w >> 16);
    return (z << 16) + w;
  }
  unsigned char get_byte() {
    return (unsigned char) (get() >> 12);
  }
};

class PrependLeaderTilestack : public LRUTilestack {
  std::auto_ptr<Tilestack> source;
  unsigned leader_nframes;

public:
  PrependLeaderTilestack(std::auto_ptr<Tilestack> source, int leader_nframes)  
    : source(source), leader_nframes(leader_nframes) {
    (*(TilestackInfo*)this) = (*(TilestackInfo*)this->source.get());
    set_nframes(leader_nframes + this->source->nframes);
  }
  
  virtual void instantiate_pixels(unsigned frame) {
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
      MWC random((frame + 1) * 4294967291,
                 (frame + 1) * 3537812053);
      int frame_size = bytes_per_frame();
      for (int i = 0; i < frame_size;) {
        unsigned char c = random.get_byte() / 4 + 96;
        for (int j = 0; j < 8; j++) {
          if (i < frame_size) {
            for (int k = 0; k < bytes_per_pixel(); k++) {
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
  std::auto_ptr<Tilestack> src(tilestackstack.pop());
  std::auto_ptr<Tilestack> result(new PrependLeaderTilestack(src, leader_nframes));
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
  
  virtual void instantiate_pixels(unsigned frame) {
    assert(!pixels[frame]);
    create(frame);
    
    memset(pixels[frame], 0, bytes_per_frame());
  }
};

// Video encoding uses 3 bands (more, e.g. alpha, will be ignored)
// and uses values 0-255 (values outside this range will be clamped)

void write_video(std::string dest, double fps, double compression)
{
  std::auto_ptr<Tilestack> src(tilestackstack.pop());
  std::string temp_dest = temporary_path(dest);
  if (!src->nframes) {
    throw_error("Tilestack has no frames in write_video");
  }

  if (create_parent_directories) make_directory_and_parents(filename_directory(dest));

  fprintf(stderr, "Encoding video to %s\n", temp_dest.c_str());
  FfmpegEncoder encoder(temp_dest, src->tile_width, src->tile_height, fps, compression);
  assert(src->bands_per_pixel >= 3);
  std::vector<unsigned char> destframe(src->tile_width * src->tile_height * 3);

  for (unsigned frame = 0; frame < src->nframes; frame++) {
    unsigned char *srcptr = src->frame_pixels(frame);
    int src_bytes_per_pixel = src->bytes_per_pixel();
    unsigned char *destptr = &destframe[0];
    for (unsigned y = 0; y < src->tile_height; y++) {
      for (unsigned x = 0; x < src->tile_width; x++) {
        *destptr++ = (unsigned char) std::min(255u, (unsigned int)src->get_pixel_band(srcptr, 0));
        *destptr++ = (unsigned char) std::min(255u, (unsigned int)src->get_pixel_band(srcptr, 1));
        *destptr++ = (unsigned char) std::min(255u, (unsigned int)src->get_pixel_band(srcptr, 2));
        srcptr += src_bytes_per_pixel;
      }
    }
    encoder.write_pixels(&destframe[0], destframe.size());
  }
  encoder.close();
  fprintf(stderr, "Renaming %s to %s\n", temp_dest.c_str(), dest.c_str());
  rename_file(temp_dest, dest);
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
  std::auto_ptr<ImageReader> reader(ImageReader::open(src));
  fprintf(stderr, "Opened %s: %d x %d pixels\n", src.c_str(), reader->width(), reader->height());

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

void load_tiles(const std::vector<std::string> &srcs)
{
  assert(srcs.size() > 0);
  std::auto_ptr<ImageReader> tile0(ImageReader::open(srcs[0]));

  std::auto_ptr<Tilestack> dest(new ResidentTilestack(srcs.size(), tile0->width(), tile0->height(),
                                                 tile0->bands_per_pixel(), tile0->bits_per_band(), 0, 0));

  for (unsigned frame = 0; frame < dest->nframes; frame++) {
    dest->toc[frame].timestamp = 0;
    std::auto_ptr<ImageReader> tile(ImageReader::open(srcs[frame]));
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
  Bbox &operator*=(double scale) {
    x *= scale;
    y *= scale;
    width *= scale;
    height *= scale;
    return *this;
  }
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
  std::string stackset_path;
  Json::Value info;
  int width, height;
  int nlevels;
  std::map<unsigned long long, TilestackReader* > readers;

public:
  Stackset(const std::string &stackset_path) : stackset_path(stackset_path) {
    std::string json_path = stackset_path + "/r.json";
    std::string json = read_file(json_path);
    if (json == "") {
      throw_error("Can't open %s for reading, or is empty", json_path.c_str());
    }
    Json::Reader json_reader;
    if (!json_reader.parse(json, info)) {
      throw_error("Can't parse %s as JSON", json_path.c_str());
    }
    width = info["width"].asInt();
    height = info["height"].asInt();
    tile_width = info["tile_width"].asInt();
    tile_height = info["tile_height"].asInt();

    nlevels = compute_tile_nlevels(width, height, tile_width, tile_height);
    TilestackReader *reader = get_reader(nlevels-1, 0, 0);
    if (!reader) throw_error("Initializing stackset but couldn't find tilestack at path %s", 
			     path(nlevels-1, 0, 0).c_str());
    (TilestackInfo&)(*this) = (TilestackInfo&)(*reader);
  }
  
  std::string path(int level, int x, int y) {
    return stackset_path + "/" + GPTileIdx(level, x, y).path() + ".ts2";
  }

  TilestackReader *get_reader(int level, int x, int y) {
    static unsigned long long cached_idx = -1;
    static TilestackReader *cached_reader = NULL;
    unsigned long long idx = GPTileIdx::idx(level, x, y);
    if (idx == cached_idx) return cached_reader;
    if (readers.find(idx) == readers.end()) {
      // TODO(RS): If this starts running out of RAM, consider LRU on the readers
      try {
	readers[idx] = new TilestackReader(std::auto_ptr<Reader>(FileReader::open(path(level, x, y))));
      } catch (std::runtime_error &e) {
        readers[idx] = NULL;
      }
    }
    cached_idx = idx;
    cached_reader = readers[idx];
    return cached_reader;
  }

  double interpolate(double val, double in_min, double in_max, double out_min, double out_max) {
    return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  // 47 vs .182:  250x more CPU than ffmpeg
  // 1.86 vs .182: 10x more CPU than ffmpeg
  // .81 vs .182: 4.5x more CPU than ffmpeg
  // .5 vs .182: 2.75x more CPU than ffmpeg
  // .41 vs .182: 2.25x more CPU than ffmpeg
  
  void get_pixel(unsigned char *dest, int frame, int level, int x, int y) {
    if (x >= 0 && y >= 0) {
      int tile_x = x / tile_width;
      int tile_y = y / tile_height;
      TilestackReader *reader = get_reader(level, tile_x, tile_y);
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

  // Pixels are centered at 0.5
  void interpolate_pixel(unsigned char *dest, int frame, int level, double x, double y) {
    x -= 0.5;
    y -= 0.5;

    const int MAX_BYTES_PER_PIXEL = 128;
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

  // frame coords:  x and y are the center of the upper-left pixel
  void render_image(Image &dest, const Frame &frame) {
    // scale between original pixels and desination frame.  If less than one, we subsample (sharp), or greater than
    // one means supersample (blurry)
    double scale = dest.width / frame.bounds.width;
    double cutoff = 0.999;
    double subsample_nlevels = log(cutoff / scale)/log(2.0);
    double source_level_d = (nlevels - 1) - subsample_nlevels;
    int source_level = limit((int)ceil(source_level_d), 0, (int)(nlevels - 1));
    //fprintf(stderr, "dest.width = %d, frame.bounds.width = %g\n", dest.width, frame.bounds.width);
    //fprintf(stderr, "nlevels = %d, source_level_d = %g, source_level = %d\n", nlevels, source_level_d, source_level);
    Bbox bounds = frame.bounds;
    if (source_level != nlevels-1) {
      bounds *= (1.0 / (1 << (nlevels-1-source_level)));
    }
    for (int y = 0; y < dest.height; y++) {
      double source_y = interpolate(y + 0.5, 0, dest.height, bounds.y, bounds.y + bounds.height);
      for (int x = 0; x < dest.width; x++) {
        double source_x = interpolate(x + 0.5, 0, dest.width, bounds.x, bounds.x + bounds.width);
        interpolate_pixel(dest.pixel(x,y), frame.frameno, source_level, source_x, source_y);
      }
    }
  }

  ~Stackset() {
    for (std::map<unsigned long long, TilestackReader*>::iterator i = readers.begin(); i != readers.end(); ++i) {
      if (i->second) delete i->second;
      i->second = NULL;
    }
  }
};

void parse_path(std::vector<Frame> &frames, Json::Value path) {
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

//void path2stack(int stack_width, int stack_height, Json::Value path, const std::string &stackset_path) {
//  Stackset stackset(stackset_path);
//  std::vector<Frame> frames;
//  parse_path(frames, path);
//  std::auto_ptr<Tilestack> out(new ResidentTilestack(frames.size(), stack_width, stack_height,
//                                                stackset.bands_per_pixel, stackset.bits_per_band,
//                                                stackset.pixel_format, 0));
//  for (unsigned i = 0; i < frames.size(); i++) {
//    Image image(stackset, stack_width, stack_height, out->frame_pixels(i));
//    stackset.render_image(image, frames[i]);
//  }
//  tilestackstack.push(out);
//}

class TilestackFromPath : public LRUTilestack {
  Stackset stackset;
  std::vector<Frame> frames;

public:
  TilestackFromPath(int stack_width, int stack_height, Json::Value path, const std::string &stackset_path) :
    stackset(stackset_path) {
    parse_path(frames, path);
    set_nframes(frames.size());
    tile_width = stack_width;
    tile_height = stack_height;
    bands_per_pixel = stackset.bands_per_pixel;
    bits_per_band = stackset.bits_per_band;
    pixel_format = stackset.pixel_format;
    compression_format = 0;
  }

  virtual void instantiate_pixels(unsigned frame) {
    assert(!pixels[frame]);
    create(frame);
    Image image(*this, tile_width, tile_height, pixels[frame]);
    stackset.render_image(image, frames[frame]);
  }
};

void path2stack(int stack_width, int stack_height, Json::Value path, const std::string &stackset_path) {
  std::auto_ptr<Tilestack> out(new TilestackFromPath(stack_width, stack_height, path, stackset_path));
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
          "--loadtiles-from-json path.json\n"
          "--delete-source-tiles\n"
          "--create-parent-directories\n"
          "--path2stack width height [frame1, ... frameN] stackset\n"
          "        Frame format {\"frameno\":N, \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
          "        Multiframe with single bounds. from and to are both inclusive.  step defaults to 1:\n"
          "          {\"frames\":{\"from\":N, \"to\":N, \"step\":N], \"bounds\": {\"xmin\":N, \"ymin\":N, \"xmax\":N, \"ymax\":N}\n"
          "--createfile file   (like touch file)\n"
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
    fprintf(stderr, "ffmpeg/qt-faststart:\n");
    bool ffmpeg_ok = FfmpegEncoder::test();
    fprintf(stderr, "%s\n", ffmpeg_ok ? "success" : "FAIL");
    success = success && ffmpeg_ok;
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
        Json::Value params = args.shift_json();
        viz(params);
      }
      else if (arg == "--writehtml") {
        std::string dest = args.shift();
        write_html(dest);
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
        tilestackstack.push(std::auto_ptr<Tilestack>(new BlackTilestack(nframes, width, height, 
                                                                        bands_per_pixel, bits_per_band)));
      }
      else if (arg == "--writevideo") {
        std::string dest = args.shift();
        double fps = args.shift_double();
        double compression = args.shift_double();
        write_video(dest, fps, compression);
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
        std::string json = read_file(json_path);
        if (json == "") {
          throw_error("Can't open %s for reading, or is empty", json_path.c_str());
        }

        Json::Value tileList;
        Json::Reader json_reader;
        if (!json_reader.parse(json, tileList)) {
          throw_error("Can't parse %s as JSON", json_path.c_str());
        }

        for (int i = 0; i < tileList["tiles"].size(); i++) {
          srcs.push_back(tileList["tiles"][i].asString());
        }

        if (srcs.empty()) {
          usage("--loadtiles-from-json must have at least one tile defined in the json");
        }

        delete_file(json_path.c_str());
        load_tiles(srcs);
      }else if (arg == "--loadtiles") {
        std::vector<std::string> srcs;
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
        std::string stackset = args.shift();
        if (stack_width <= 0 || stack_height <= 0) {
          usage("--path2stack: width and height must be positive numbers");
        }
        path2stack(stack_width, stack_height, path, stackset);
      }
      else if (arg == "--createfile") {
        std::auto_ptr<FileWriter> out(FileWriter::open(args.shift()));
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
    fprintf(stderr, "User time %g, System time %g\n", user, system);
    return 0;
  } catch (const std::exception &e) {
    fprintf(stderr, "Error: %s\n", e.what());
    return 1;
  }
}
