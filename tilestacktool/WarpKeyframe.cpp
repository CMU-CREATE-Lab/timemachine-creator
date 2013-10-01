#include "WarpKeyframe.h"

Bbox parseBounds(JSON bounds) {
  return Bbox(bounds["xmin"].doub(), 
              bounds["ymin"].doub(),
              bounds["xmax"].doub() - bounds["xmin"].doub(),
              bounds["ymax"].doub() - bounds["ymin"].doub());
}

WarpKeyframe WarpKeyframe::fromJson(JSON json) {
  WarpKeyframe k;
  k.time = json["time"].doub();
  k.bounds = parseBounds(json["bounds"]);
  k.dur = json.hasKey("duration") && json["duration"].isNumeric() ? json["duration"].doub() : NAN;
  k.speed = json.get("speed", NAN);
  if (!isnan(k.speed)) k.speed /= 100.0;
  k.looped = json.get("is-loop", false);
  k.loopCount = json.get("loopTimes", 0);
  k.loopDwellStart = json.get("waitStart", 0.0);
  k.loopDwellEnd = json.get("waitEnd", 0.0);
  
  return k;
}

std::string WarpKeyframe::to_string() const {
  return string_printf("[WarpKeyframe speed=%g looped=%d loopCount=%d loopDwellStart=%g loopDwellEnd=%g dur=%g]",
                       speed, looped, loopCount, loopDwellStart, loopDwellEnd, dur);
}

double WarpKeyframe::computeSourceTime(double t, const WarpKeyframe &next) const {
  if (looped) {
    double playback = fmod((t + playbackOffset()), playbackPeriod());
    playback -= loopDwellStart;
    playback = std::max(0.0, playback);
    double ret = std::min(sourcePeriod(), playback * speed);
    return ret;
  } else {
    return interpolate(t, 0, time, duration(next), next.time);
  }
}

// TODO(rsargent): fix me
double WarpKeyframe::sourcePeriod() const {
  return looped ? 2.9 : 1e100;
}
 
double WarpKeyframe::playbackPeriod() const {
  return loopDwellStart + sourcePeriod() / speed + loopDwellEnd;
}
 
double WarpKeyframe::playbackOffset() const {
  return playbackOffset(time);
}
    
double WarpKeyframe::playbackOffset(double sourceTime) const {
  double ret = sourceTime / speed;
  if (time > 0) {
    ret += loopDwellStart; 
  }
  return ret;
}
    
double WarpKeyframe::duration(const WarpKeyframe &next) const {
  if (looped) {
    double ret = playbackOffset(next.time) - playbackOffset();
    if (ret < 0) ret += playbackPeriod();
    ret += loopCount * playbackPeriod();
    return ret;
  } else {
    return dur;
  }
}
