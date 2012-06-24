#include <assert.h>

#include "zlib.h"
#include "SimpleZlib.h"
#include "cpp_utils.h"

bool Zlib::compress(std::vector<unsigned char> &dest, const unsigned char *src, size_t src_len) {
  z_stream strm;
  dest.resize(src_len * 11/10 + 65536);
  
  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) return false;
  strm.avail_in = src_len;
  strm.next_in = (unsigned char*) src;

  strm.avail_out = dest.size();
  strm.next_out = &dest[0];

  ret = deflate(&strm, Z_FINISH);
  assert(ret != Z_STREAM_ERROR);
  assert(strm.avail_out > 0);
  assert(strm.avail_in == 0);
  dest.resize(dest.size() - strm.avail_out);
  (void)deflateEnd(&strm);
  return true;
}

bool Zlib::uncompress(std::vector<unsigned char> &dest, const unsigned char *src, size_t src_len) {
  z_stream strm;
  
  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = src_len;
  strm.next_in = (unsigned char*) src;
  int ret = inflateInit(&strm);
  if (ret != Z_OK) return false;

  std::vector<unsigned char> buf(1024*1024);

  dest.resize(0);

  do {
    strm.avail_out = buf.size();
    strm.next_out = &buf[0];

    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret < 0 || ret == Z_NEED_DICT) {
      (void)inflateEnd(&strm);
      throw_error("Error %d in zlib uncompress", ret);
    }
    dest.insert(dest.end(), &buf[0], &buf[buf.size() - strm.avail_out]);
  } while (ret != Z_STREAM_END);
  
  (void)inflateEnd(&strm);
  return true;
}
