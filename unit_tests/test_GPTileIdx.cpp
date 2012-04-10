#include <assert.h>

#include "GPTileIdx.h"

int main(int argc, char **argv) {
  assert(GPTileIdx(0,0,0).basename() == "r");
  assert(GPTileIdx(0,0,0).path() == "r");
  assert(GPTileIdx(1,0,0).basename() == "r0");
  assert(GPTileIdx(1,0,0).path() == "r0");
  assert(GPTileIdx(2,0,0).basename() == "r00");
  assert(GPTileIdx(2,0,0).path() == "r00");

  assert(GPTileIdx(3,0,0).basename() == "r000");
  assert(GPTileIdx(3,0,0).path() == "r00/r000");
  assert(GPTileIdx(4,0,0).basename() == "r0000");
  assert(GPTileIdx(4,0,0).path() == "r00/r0000");
  assert(GPTileIdx(5,0,0).basename() == "r00000");
  assert(GPTileIdx(5,0,0).path() == "r00/r00000");
  
  assert(GPTileIdx(6,0,0).basename() == "r000000");
  assert(GPTileIdx(6,0,0).path() == "r00/000/r000000");
  
  assert(GPTileIdx(6,42,12).basename() == "r103210");
  assert(GPTileIdx(6,42,12).path() == "r10/321/r103210");

  return 0;
}
