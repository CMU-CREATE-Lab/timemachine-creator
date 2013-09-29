#include <assert.h>

#include "JSON.h"

int main() {
  try {
    JSON j("[1,2,3,\"a\",true,false,{\"a\":99}]");
    
    assert(j[0].doub() == 1);
    assert(j[0].integer() == 1);
    assert(j[1].doub() == 2);
    assert(j[2].doub() == 3);
    assert(j[3].str() == "a");
    assert(j[4].boolean() == true);
    assert(j[5].boolean() == false);
    assert(j[6]["a"].integer() == 99);
    
    j = JSON::fromFile("unit_tests/test_JSON.json");
    assert(j["a"].integer() == 1);
    assert(j["b"].doub() == 2);
    assert(j["c"].size() == 3);
    assert(j["c"][0].integer() == 1);
    assert(j["c"][1].integer() == 2);
    assert(j["c"][2].integer() == 3);
    
  } catch (std::exception &e) {
    fprintf(stderr, "Unexpected exception %s\n", e.what());
  }
  return 0;
}
