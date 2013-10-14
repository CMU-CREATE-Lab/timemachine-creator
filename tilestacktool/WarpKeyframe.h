#ifndef WARP_KEYFRAME_H
#define WARP_KEYFRAME_H

#include "Frame.h"
#include "JSON.h"

#ifdef _WIN32
  #define INFINITY (DBL_MAX+DBL_MAX)
  #define NAN (INFINITY-INFINITY)
  #include <float.h>
#endif

class WarpKeyframe {
 public:
  // Position
  double time;
  Bbox bounds;

  // Transition
 private:
  double dur;  // seconds

 public:
  double speed;        // speed of source video
  bool   looped;       // is source video looped?
  int    loopCount;    // number of loops
  double loopDwellStart;
  double loopDwellEnd;

  static WarpKeyframe fromJson(JSON json);

  double computeSourceTime(double t, const WarpKeyframe &next) const;

  double sourcePeriod() const;

  double duration(const WarpKeyframe &next) const;

  std::string to_string() const;

 private:
  double playbackPeriod() const;
  double playbackOffset() const;
  double playbackOffset(double sourceTime) const;
};

#endif
