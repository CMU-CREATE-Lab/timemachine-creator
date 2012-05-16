#include "io.h"

std::vector<unsigned char> Reader::read(size_t offset, size_t length) {
  std::vector<unsigned char> ret(length);
  read(&ret[0], offset, length);
  return ret;
}

IfstreamReader::IfstreamReader(std::string filename) : f(filename.c_str(), std::ios::in | std::ios::binary), filename(filename) {
  if (!f.good()) throw_error("Error opening %s for reading\n", filename.c_str());
}

void IfstreamReader::read(unsigned char *dest, size_t pos, size_t length) {
  f.seekg(pos, std::ios_base::beg);
  f.read((char*)dest, length);
  if (f.fail()) {
    throw_error("Error reading %zd bytes from file %s at position %zd\n",
                length, filename.c_str(), pos);
  }
}

size_t IfstreamReader::length() {
  f.seekg(0, std::ios::end);
  return f.tellg();
}

void Writer::write(const std::vector<unsigned char> &src) {
  write(&src[0], src.size());
}

OfstreamWriter::OfstreamWriter(std::string filename) : f(filename.c_str(), std::ios::out | std::ios::binary), filename(filename) {
  if (!f.good()) throw_error("Error opening %s for writing\n", filename.c_str());
}

void OfstreamWriter::write(const unsigned char *src, size_t length) {
  f.write((char*)src, length);
  if (f.fail()) {
    throw_error("Error writing %zd bytes to file %s\n", length, filename.c_str());
  }
}
