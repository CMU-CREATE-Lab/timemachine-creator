#ifndef XMLREADER_H
#define XMLREADER_H

#include "xml/rapidxml.hpp"
#include "tilestacktool.h"
#include <stdio.h>

struct Rinfo{
  double minx, miny, maxx, maxy, projx, projy;
  Rinfo():minx(-1),miny(-1),maxx(-1),maxy(-1),projx(-1),projy(-1) {}
};

Rinfo parse_xml(const char *filename);

#endif
