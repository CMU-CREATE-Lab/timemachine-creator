#include "io.h"

std::vector<unsigned char> Reader::read(size_t offset, size_t length) {
  std::vector<unsigned char> ret(length);
  read(&ret[0], offset, length);
  return ret;
}

void Writer::write(const std::vector<unsigned char> &src) {
  write(&src[0], src.size());
}

FileReader* (*FileReader::opener)(std::string filename);

FileReader *FileReader::open(std::string filename) {
  if (!opener) throw_error("Nothing registered to handle FileReader::open");
  return (*opener)(filename);
}

bool FileReader::register_opener(FileReader* (*o)(std::string filename)) {
  if (opener) throw_error("Multiple handlers registered for FileReader::open");
  opener = o;
  return true;
}


FileWriter* (*FileWriter::opener)(std::string filename);

FileWriter *FileWriter::open(std::string filename) {
  if (!opener) throw_error("Nothing registered to handle FileWriter::open");
  return (*opener)(filename);
}

bool FileWriter::register_opener(FileWriter* (*o)(std::string filename)) {
  if (opener) throw_error("Multiple handlers registered for FileWriter::open");
  opener = o;
  return true;
}
