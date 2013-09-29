#include "tilestacktool.h"

#include "warp.h"

void parse_framelist(std::vector<Frame> &frames, JSON path, bool isProjection) {
  // Iterate through an array of frames.  If we don't receive an array, assume we have a single frame path
  int len = path.isArray() ? path.size() : 1;
  for (int i = 0; i < len; i++) {
    JSON frame_desc = path.isArray() ? path[i] : path;
    
    double x=0,y=0,width=0,height=0;
    if (!isProjection) {
      x = frame_desc["bounds"]["xmin"].doub();
      y = frame_desc["bounds"]["ymin"].doub();
      width = frame_desc["bounds"]["width"].doub();
      height = frame_desc["bounds"]["height"].doub();
    }
    Bbox bounds(x,y,width,height);
    if (frame_desc.hasKey("frames")) {
      int step = frame_desc["frames"].hasKey("step") ? frame_desc["frames"]["step"].integer() : 1;
      for (int j = frame_desc["frames"]["start"].integer(); true; j += step) {
        frames.push_back(Frame(j, bounds));
        if (j == frame_desc["frames"]["end"].integer()) break;
      }
    } else if (frame_desc.hasKey("frame")) {
      // Future: consider floating point frame #s to enable fading between frames
      frames.push_back(Frame((int) frame_desc["frame"].doub(), bounds));
    } else {
      throw_error("Don't yet know how to parse a path segment without a 'frame' or 'frames' field, sorry");
    }
  }
}

void parse_timewarp(std::vector<Frame> &frames, JSON path, double fps) {
  JSON timewarp;
  //  if (!json.isMember("snaplapse")) throw_error("Timewarp missing 'snaplapse' field");
}

void parse_warp(std::vector<Frame> &frames, JSON path, bool isProjection, double fps) {
  if (path.isObject() && (path.hasKey("snaplapse") || path.hasKey("timewarp"))) {
    if (fps == 0) {
      throw_error("warp JSON given, but fps not given in command");
    }
    throw_error("timewarp not supported yet");
    parse_timewarp(frames, path, fps);
  } else {
    if (fps != 0) {
      fprintf(stderr, "Warning: ignoring fps specified for path2warp as explicit frames were provided");
    }
    parse_framelist(frames, path, isProjection);
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
//void parse_timewarp(std::vector<Frame> &frames, JSON json, double fps) {
//  JSON timewarp;
//  if (json.isMember("snaplapse")) timewarp = json["snaplapse"];
//  else if (json.isMember("timewarp")) timewarp = json["timewarp"];
//  else throw_error("Timewarp must have 'timewarp' field");
//  JSON keyframes = timewarp["keyframes"];
//  if (keyframes.isNull()) throw_error("Timewarp must have 'keyframes' field");
//  // Fill in durations
  
                                    
  
