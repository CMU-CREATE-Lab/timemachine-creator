#include "H264Encoder.h"

// OSX: Download ffmpeg binary from ffmpegmac.net

H264Encoder::H264Encoder(std::string dest_filename, int width, int height, double fps, double compression) :
  total_written(0), dest_filename(dest_filename), width(width), height(height), fps(fps), compression(compression)  {
  tmp_filename = temporary_path(filename_sans_directory(dest_filename));
  int nthreads = 8;
  std::string cmdline = string_printf("\"%s\" -threads %d -loglevel error -benchmark", path_to_ffmpeg().c_str(), nthreads);
  
  // Input
  cmdline += string_printf(" -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -r %g -i pipe:0",
                           width, height, fps);
  // Output
  int frames_per_keyframe = 10; // TODO(rsargent): don't hardcode this
  cmdline += " -vcodec libx264";
  //cmdline += string_printf(" -fpre libx264-hq.ffpreset");
  //cmdline += " -coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -keyint_min 25 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -refs 4 -directpred 3 -trellis 1 -flags2 +wpred+mixed_refs+dct8x8+fastpskip";
  cmdline += " -preset slow -pix_fmt yuv420p";

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

void H264Encoder::write_pixels(unsigned char *pixels, size_t len) {
  //fprintf(stderr, "Writing %zd bytes to ffmpeg\n", len);
  if (1 != fwrite(pixels, len, 1, out)) {
    throw_error("Error writing to ffmpeg");
  }
  total_written += len;
}

void H264Encoder::close() {
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

bool H264Encoder::test() {
  std::string cmdline = string_printf("\"%s\" -loglevel error -version", path_to_ffmpeg().c_str());
  FILE *ffmpeg = popen_utf8(cmdline.c_str(), "wb");
  if (!ffmpeg) {
    fprintf(stderr, "H264Encoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
    return false;
  }
  while (fgetc(ffmpeg) != EOF) {}
  int status = pclose(ffmpeg);
  if (status) {
    fprintf(stderr, "H264Encoder::test:  pclose '%s' returns status %d.  This is likely due to an installation problem.\n", cmdline.c_str(), status);
    return false;
  }
  
  fprintf(stderr, "H264Encoder: success\n");
  
  // No longer using qt-faststart executable;  instead, using internal qt_faststart function    
  // cmdline = string_printf("\"%s\"", path_to_qt_faststart().c_str());
  // status = system(cmdline.c_str());
  // if (status != 0) {
  //   fprintf(stderr, "H264Encoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
  //   return false;
  // }
  // fprintf(stderr, "qt-faststart: success\n");
  
  return true;
}

std::string H264Encoder::ffmpeg_path_override;

std::string H264Encoder::path_to_ffmpeg() {
  if (ffmpeg_path_override != "") return ffmpeg_path_override; 
  return path_to_executable("ffmpeg");
}

std::string H264Encoder::path_to_qt_faststart() {
  return path_to_executable("qt-faststart");
}

std::string H264Encoder::path_to_executable(std::string executable) {
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
