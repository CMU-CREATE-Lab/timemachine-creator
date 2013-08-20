#ifndef WARP_H
#define WARP_H

#include <vector>
// TODO - if path2stack-projected always uses bounding boxes, we have to remove
// the third parameter: isProjection.
void parse_warp(std::vector<Frame> &frames, Json::Value path, bool isProjection = false);

#endif

