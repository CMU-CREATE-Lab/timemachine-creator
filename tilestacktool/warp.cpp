#include "tilestacktool.h"
#include "WarpKeyframe.h"
#include "Frame.h"

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
      int step = frame_desc["frames"].get("step", 1);
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

// Returns null if beyond end of warp
Frame computeFrame(std::vector<WarpKeyframe> &keyframes, double time, double sourceFPS, double smoothing) {
  // Find keyframe range and compute relative time to start of 
  // the transition
  
  if (smoothing == 0) {
    for (unsigned i = 0; i < keyframes.size() - 1; i++) {
      WarpKeyframe start = keyframes[i];
      WarpKeyframe end = keyframes[i + 1];
      if (time <= start.duration(end)) {
        Bbox bounds = Bbox::scaled_interpolate(time, 0, start.bounds, start.duration(end), end.bounds);
        double sourceTime = start.computeSourceTime(time, end);
        return Frame(sourceTime * sourceFPS, bounds);
      }
      time -= start.duration(end);
    }
    // time is beyond end of last keyframe
    return Frame(-1, Bbox());
  } else {
    Frame ret = computeFrame(keyframes, time, sourceFPS, 0);
    if (ret.frameno >= 0) {
      Bbox sum = ret.bounds;
      int n = 1;
      for (int i = 1; i <= 50; i++) {
        Frame a = computeFrame(keyframes, time - smoothing * i / 50.0, sourceFPS, 0);
        Frame b = computeFrame(keyframes, time + smoothing * i / 50.0, sourceFPS, 0);
        // Add surrounding samples symmetrically;  if one side goes 
        // "off the end", discount the other side too, so that at the limit,
        // the edge of the tour will exactly match the input with no filtering.
        if (a.frameno >= 0 && b.frameno >= 0) {
          sum = sum + a.bounds + b.bounds;
          n += 2;
        }
      }
      ret.bounds = sum / n;
    }
    return ret;
  }
}

void parse_timewarp(std::vector<Frame> &frames, JSON path, JSON settings) {
  JSON jsonKeyFrames = path["snaplapse"]["keyframes"];

  double sourceFPS = settings["sourceFPS"].doub();
  double destFPS = settings["destFPS"].doub();
  double smoothingDuration = settings.get("smoothingDuration", 0.0);
  fprintf(stderr, "smoothingDuration = %g\n", smoothingDuration);
  
  std::vector<WarpKeyframe> keyframes;
  for (unsigned i = 0; i < jsonKeyFrames.size(); i++) {
    keyframes.push_back(WarpKeyframe::fromJson(jsonKeyFrames[i]));
  }
  
  for (double time = 0; 1; time += 1.0 / destFPS) {
    Frame f = computeFrame(keyframes, time, sourceFPS, smoothingDuration);
    if (f.frameno < 0) break;
    frames.push_back(f);
  }
}

void parse_warp(std::vector<Frame> &frames, JSON path, bool isProjection, JSON settings) {
  if (path.isObject() && (path.hasKey("snaplapse") || path.hasKey("timewarp"))) {
    parse_timewarp(frames, path, settings);
  } else {
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
  
                                    
  
