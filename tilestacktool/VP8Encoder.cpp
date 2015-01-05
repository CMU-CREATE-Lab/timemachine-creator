#include "VP8Encoder.h"

VP8Encoder::VP8Encoder(std::string dest_filename, int width, int height, double fps, double compression) :
  total_written(0), dest_filename(dest_filename), width(width), height(height), fps(fps), compression(compression)  {
  tmp_filename = temporary_path(dest_filename);
  int nthreads = 8;
  std::string cmdline = string_printf("\"%s\" -threads %d -loglevel error -benchmark", path_to_ffmpeg().c_str(), nthreads);

  // Input
  cmdline += string_printf(" -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r %g -i pipe:0",
                           width, height, fps);
  // Output
  int frames_per_keyframe = 20; // TODO(pdille): don't hardcode this
  std::string max_bitrate = "5M"; // TODO(pdille): don't hardcode this
  cmdline += " -c:v libvpx";
  cmdline += string_printf(" -qmin 0 -qmax 34 -crf %g -b:v %s -g %d \"%s\"",
                           ceil(compression / 2), max_bitrate.c_str(), frames_per_keyframe, tmp_filename.c_str());

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

void VP8Encoder::write_pixels(unsigned char *pixels, size_t len) {
  //fprintf(stderr, "Writing %zd bytes to ffmpeg\n", len);
  if (1 != fwrite(pixels, len, 1, out)) {
    throw_error("Error writing to ffmpeg");
  }
  total_written += len;
}

void VP8Encoder::close() {
  if (out) pclose(out);
  fprintf(stderr, "Wrote %ld frames (%ld bytes) to ffmpeg\n",
          (long) (total_written / (width * height * 3)), (long) total_written);
  out = NULL;

  rename_file(tmp_filename, dest_filename);
}

bool VP8Encoder::test() {
  std::string cmdline = string_printf("\"%s\" -loglevel error -version", path_to_ffmpeg().c_str());
  FILE *ffmpeg = popen_utf8(cmdline.c_str(), "wb");
  if (!ffmpeg) {
    fprintf(stderr, "VP8Encoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
    return false;
  }
  while (fgetc(ffmpeg) != EOF) {}
  int status = pclose(ffmpeg);
  if (status) {
    fprintf(stderr, "VP8Encoder::test:  pclose '%s' returns status %d.  This is likely due to an installation problem.\n", cmdline.c_str(), status);
    return false;
  }

  fprintf(stderr, "VP8Encoder: success\n");

  return true;
}

std::string VP8Encoder::ffmpeg_path_override;

std::string VP8Encoder::path_to_ffmpeg() {
  if (ffmpeg_path_override != "") return ffmpeg_path_override;
  return path_to_executable("ffmpeg");
}

std::string VP8Encoder::path_to_executable(std::string executable) {
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
