#ifndef MWC_H
#define MWC_H

#include <assert.h>

// Adapted from George Marsaglia's Multiply With Carry, http://www.cse.yorku.ca/~oz/marsaglia-rng.html

struct MWC {
  unsigned int z; // 32 bits
  unsigned int w; // 32 bits
  MWC(int z_seed, int w_seed) : z(z_seed), w(w_seed) {
    assert(z && w);
  }
  unsigned int get() {
    z = 36969 * (z & 65535) + (z >> 16);
    w = 18000 * (w & 65535) + (w >> 16);
    return (z << 16) + w;
  }
  unsigned char get_byte() {
    return (unsigned char) (get() >> 12);
  }
};

#endif
