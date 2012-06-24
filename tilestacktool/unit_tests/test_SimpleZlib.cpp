#include "SimpleZlib.h"
#include "mwc.h"

#include <vector>

void test(const char *msg, const std::vector<unsigned char> &orig) {
  std::vector<unsigned char> compressed;
  Zlib::compress(compressed, &orig[0], orig.size());
  fprintf(stderr, "Testing Zlib on %s.  %d bytes uncompressed.  %d bytes compressed (%.2f%%)\n",
          msg, (int) orig.size(), (int) compressed.size(), 100.0 * compressed.size() / orig.size());
  std::vector<unsigned char> uncompressed;
  Zlib::uncompress(uncompressed, &compressed[0], compressed.size());
  assert(orig == uncompressed);
}

int main(int argc, char **argv)
{
  {
    MWC rand(0x12345678, 0x87654321);
    int len = 1024*1024*20+1234567;
    std::vector<unsigned char> data(len);
    for (int i = 0; i < len; i++) data[i] = rand.get_byte();
    test("Random bytes", data);
  }
  {
    std::vector<unsigned char> data(1000000, 0);
    test("Zero bytes", data);
  }
  {
    std::vector<unsigned char> data;
    test("Empty", data);
  }
  return 0;
}
