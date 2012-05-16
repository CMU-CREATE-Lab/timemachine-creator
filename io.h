#ifndef IO_H
#define IO_H

#include <fstream>
#include <vector>

#include "cpp-utils.h"

class Reader {
public:
  virtual void read(unsigned char *dest, size_t offset, size_t length) = 0;
  virtual size_t length() = 0;
  std::vector<unsigned char> read(size_t offset, size_t length);
  virtual ~Reader() {}
};

class IfstreamReader : public Reader {
  std::ifstream f;
  std::string filename;
public:
  IfstreamReader(std::string filename);
  virtual void read(unsigned char *dest, size_t pos, size_t length);
  size_t length();
  virtual ~IfstreamReader() {}
};

class Writer {
public:
  virtual void write(const unsigned char *src, size_t length) = 0;
  void write(const std::vector<unsigned char> &src);
  virtual ~Writer() {}
};

class OfstreamWriter : public Writer {
  std::ofstream f;
  std::string filename;
public:
  OfstreamWriter(std::string filename);
  virtual void write(const unsigned char *src, size_t length);
  virtual ~OfstreamWriter() {}
};

#endif
