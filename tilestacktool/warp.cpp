#include "tilestacktool.h"

#include "warp.h"

void parse_simple_warp(std::vector<Frame> &frames, Json::Value path, bool isProjection) {
  // Iterate through an array of frames.  If we don't receive an array, assume we have a single frame path
  int len = path.isArray() ? path.size() : 1;
  for (int i = 0; i < len; i++) {
    Json::Value frame_desc = path.isArray() ? path[i] : path;
    if (!frame_desc.isObject()) {
      throw_error("Syntax error translating frame %d (not a JSON object)", i);
    }
    
    double x=0,y=0,width=0,height=0;
    if (!isProjection) {
      if (!frame_desc.isMember("bounds")) {
        throw_error("Frame must include bounds");
      }
      // TODO(RS): move validating and parsing json to bbox static member fn
      if (!frame_desc["bounds"].isMember("xmin")) {
        throw_error("bounds must include xmin");
      }
      if (!frame_desc["bounds"].isMember("ymin")) {
        throw_error("bounds must include ymin");
      }
      if (!frame_desc["bounds"].isMember("width")) {
        throw_error("bounds must include width");
      }
      if (!frame_desc["bounds"].isMember("height")) {
        throw_error("bounds must include height");
      }
      x = frame_desc["bounds"]["xmin"].asDouble();
      y = frame_desc["bounds"]["ymin"].asDouble();
      width = frame_desc["bounds"]["width"].asDouble();
      height = frame_desc["bounds"]["height"].asDouble();
    }
    Bbox bounds(x,y,width,height);
    if (frame_desc.isMember("frames")) {
      int step = frame_desc["frames"]["step"].isNull() ? 1 : frame_desc["frames"]["step"].asInt();
      for (int j = frame_desc["frames"]["start"].asInt(); true; j += step) {
        frames.push_back(Frame(j, bounds));
        if (j == frame_desc["frames"]["end"].asInt()) break;
      }
    } else if (frame_desc.isMember("frame")) {
      // Future: consider floating point frame #s to enable fading between frames
      frames.push_back(Frame((int) frame_desc["frame"].asDouble(), bounds));
    } else {
      throw_error("Don't yet know how to parse a path segment without a 'frame' or 'frames' field, sorry");
    }
  }
}

void parse_warp(std::vector<Frame> &frames, Json::Value path, bool isProjection) {
  if (path.isObject() && (path.isMember("snaplapse") || path.isMember("timewarp"))) {
    throw_error("timewarp not supported yet");
    //double fps = 25.0;
    //parse_timewarp(frames, path, fps);
  } else {
    parse_simple_warp(frames, path, isProjection);
  }
}

//class Path {
//  virtual bool get_bounds(Bbox &ret, double time) = NULL;
//};
//
//class Timeline {
//  virtual bool get_frame(double &ret, double time) = NULL;
//};
//
//
//
//
//
//
//ScaleCorrectedLinearMotion(start end)
//
//void parse_timewarp(std::vector<Frame> &frames, Json::Value json, double fps) {
//  Json::Value timewarp;
//  if (json.isMember("snaplapse")) timewarp = json["snaplapse"];
//  else if (json.isMember("timewarp")) timewarp = json["timewarp"];
//  else throw_error("Timewarp must have 'timewarp' field");
//  Json::Value keyframes = timewarp["keyframes"];
//  if (keyframes.isNull()) throw_error("Timewarp must have 'keyframes' field");
//  // Fill in durations
  
                                    
  
