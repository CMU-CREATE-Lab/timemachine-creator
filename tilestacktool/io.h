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

class Writer {
public:
  virtual void write(const unsigned char *src, size_t length) = 0;
  void write(const std::vector<unsigned char> &src);
  virtual ~Writer() {}
};

class FileReader : public Reader {
  static FileReader* (*opener)(std::string filename);
public:
  virtual void read(unsigned char *dest, size_t pos, size_t length) = 0;
  virtual size_t length() = 0;
  virtual ~FileReader() {}

  static FileReader *open(std::string filename);
  static bool register_opener(FileReader* (*o)(std::string filename));
};

class FileWriter : public Writer {
  static FileWriter* (*opener)(std::string filename);
public:
  virtual void write(const unsigned char *src, size_t length) = 0;
  virtual ~FileWriter() {}

  static FileWriter *open(std::string filename);
  static bool register_opener(FileWriter* (*o)(std::string filename));
};


#endif
