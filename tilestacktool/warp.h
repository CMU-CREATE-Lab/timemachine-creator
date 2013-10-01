#ifndef WARP_H
#define WARP_H

#include <vector>

#include "JSON.h"
#include "Frame.h"

void parse_warp(std::vector<Frame> &frames, JSON path, bool isProjection, JSON settings);

#endif

