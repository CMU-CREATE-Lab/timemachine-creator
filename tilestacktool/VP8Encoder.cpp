#include "VP8Encoder.h"

VP8Encoder::VP8Encoder(std::string dest_filename, int width, int height, double fps, double targetBitsPerPixel) :
  total_written(0), dest_filename(dest_filename), width(width), height(height), fps(fps), targetBitsPerPixel(targetBitsPerPixel)  {
    
  tmp_filename = temporary_path(filename_sans_directory(dest_filename));
  int nthreads = 8;
  std::string cmdline = string_printf("\"%s\" -threads %d -loglevel error -benchmark", path_to_ffmpeg().c_str(), nthreads);
  cmdline += string_printf(" -s %dx%d -vcodec rawvideo -f rawvideo -pix_fmt rgb24 -i - -pix_fmt yuv420p %s.yuv",width, height, tmp_filename.c_str());
  
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
  
  return;
}

void VP8Encoder::write_pixels(unsigned char *pixels, size_t len) {
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
  
  std::string cmdcommon = string_printf("\"%s\" %s.yuv -o %s -w %d -h %d --fps=%ld/1 -p 2 --codec=vp8 --fpf=%s.fpf --cpu-used=0 --target-bitrate=%ld --auto-alt-ref=1 -v --min-q=4 --max-q=52 --drop-frame=0 --lag-in-frames=25 --kf-min-dist=0 --kf-max-dist=120 --static-thresh=0 --tune=psnr", path_to_vpxenc().c_str(), tmp_filename.c_str(), dest_filename.c_str(), width, height, lrint(fps), tmp_filename.c_str(), lrint(width * height * fps * targetBitsPerPixel / 8000.0));
  
  std::string pass1 = cmdcommon + string_printf(" --pass=1 --minsection-pct=0 --maxsection-pct=800 -t 0");
  std::string pass2 = cmdcommon + string_printf(" --pass=2 --minsection-pct=5 --maxsection-pct=1000 --bias-pct=50 -t 6 --end-usage=vbr --good --profile=0 --arnr-maxframes=7 --arnr-strength=5 --arnr-type=3 --psnr");
  
  fprintf(stderr, "Cmndline: %s\n", pass1.c_str());
  if (system(pass1.c_str()) != 0)
    throw_error("Problem in running vpxenc pass 1");
  fprintf(stderr, "Cmndline: %s\n", pass2.c_str());
  if (system(pass2.c_str()) != 0)
    throw_error("Problem in running vpxenc pass 2");
  
  delete_file(tmp_filename+".yuv");
  delete_file(tmp_filename+".fpf");
  return;
}

bool VP8Encoder::test() {
  std::string cmdline = string_printf("\"%s\"", path_to_vpxenc().c_str());
  FILE *vpxenc = popen_utf8(cmdline.c_str(), "wb");
  if (!vpxenc) {
    fprintf(stderr, "VP8Encoder::test: Can't run '%s'.  This is likely due to an installation problem.\n", cmdline.c_str());
    return false;
  }
  while (fgetc(vpxenc) != EOF) {}
  int status = pclose(vpxenc);
  if (status == -1) {
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

std::string VP8Encoder::vpxenc_path_override;

std::string VP8Encoder::path_to_vpxenc() {
  if (ffmpeg_path_override != "") return ffmpeg_path_override; 
  return path_to_executable("vpxenc");
}

std::string VP8Encoder::path_to_executable(std::string executable) {
  executable += executable_suffix();
  std::vector<std::string> search_path;
  search_path.push_back(string_printf("%s/vpxenc/%s/%s",
                                      filename_directory(executable_path()).c_str(),
                                      os().c_str(),
                                      executable.c_str()));
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
