#ifndef IO_STREAMFILE_H
#define IO_STREAMFILE_H

#include "io.h"

class StreamFileReader : public FileReader {
  std::ifstream f;
  std::string filename;
public:
  StreamFileReader(std::string filename);
  virtual void read(unsigned char *dest, size_t pos, size_t length);
  size_t length();
  virtual ~StreamFileReader() {}

  static FileReader *open(std::string filename);
};

class StreamFileWriter : public FileWriter {
  std::ofstream f;
  std::string filename;
public:
  StreamFileWriter(std::string filename);
  virtual void write(const unsigned char *src, size_t length);
  virtual ~StreamFileWriter() {}

  static FileWriter *open(std::string filename);
};

#endif
