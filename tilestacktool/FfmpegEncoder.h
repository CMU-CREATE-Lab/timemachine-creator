#ifndef INCLUDE_FFMPEG_ENCODER_H
#define INCLUDE_FFMPEG_ENCODER_H

#include <string>

class FfmpegEncoder {
  int width, height;
  FILE *out;
  size_t total_written;
  std::string tmp_filename;
  std::string dest_filename;

public:
  FfmpegEncoder(std::string dest_filename, int width, int height,
                double fps, double compression);
  void write_pixels(unsigned char *pixels, size_t len);
  void close();

  static bool test();
  static std::string path_to_ffmpeg();
  static std::string path_to_qt_faststart();
  static std::string path_to_executable(std::string executable);
  static std::string ffmpeg_path_override;
};

#endif
