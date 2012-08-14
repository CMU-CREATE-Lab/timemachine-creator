#ifndef TILESTACKTOOL_H
#define TILESTACKTOOL_H

#include <cstdlib>
#include <list>
#include <string>
#include <memory>

#include "json/json.h"

#include "simple_shared_ptr.h"
#include "Tilestack.h"

void usage(const char *fmt, ...);

void ensure_resident();

class Arglist : public std::list<std::string> {
public:
  Arglist(char **begin, char **end) {
    for (char **arg = begin; arg < end; arg++) push_back(std::string(*arg));
  }
  std::string shift() {
    if (empty()) usage("Missing argument");
    std::string ret = front();
    pop_front();
    return ret;
  }
  double shift_double() {
    std::string arg = shift();
    char *end;
    double ret = strtod(arg.c_str(), &end);
    if (end != arg.c_str() + arg.length()) {
      usage("Can't parse '%s' as floating point value", arg.c_str());
    }
    return ret;
  }
  int shift_int() {
    std::string arg = shift();
    char *end;
    int ret = strtol(arg.c_str(), &end, 0);
    if (end != arg.c_str() + arg.length()) {
      usage("Can't parse '%s' as integer", arg.c_str());
    }
    return ret;
  }
  Json::Value shift_json() {
    std::string arg = shift();
    Json::Reader reader;
    Json::Value ret;
    if (!reader.parse(arg, ret)) {
      usage("Can't parse '%s' as json", arg.c_str());
    }
    return ret;
  }
  bool next_is_non_flag() const {
    if (empty()) return false;
    if (front().length() > 0 && front()[0] == '-') return false;
    return true;
  }
};

template <typename T>
class AutoPtrStack {
  std::list<simple_shared_ptr<T> > stack;
public:
  void push(simple_shared_ptr<T> &t) {
    stack.push_back(t);
  }
  simple_shared_ptr<T> pop() {
    assert(!stack.empty());
    simple_shared_ptr<T> ret = stack.back();
    stack.pop_back();
    return ret;
  }
  simple_shared_ptr<T> top() {
    assert(!stack.empty());
    return stack.back();
  }
  int size() { 
    return stack.size(); 
  }
};

struct Bbox {
  double x, y;
  double width, height;
  Bbox(double x, double y, double width, double height) : x(x), y(y), width(width), height(height) {}
  Bbox &operator*=(double scale) {
    x *= scale;
    y *= scale;
    width *= scale;
    height *= scale;
    return *this;
  }
  std::string to_string() {
    return string_printf("[bbox x=%g y=%g width=%g height=%g]",
                         x, y, width, height);
  }
};

struct Frame {
  int frameno;
  Bbox bounds;
  Frame(int frameno, const Bbox &bounds) : frameno(frameno), bounds(bounds) {}
};

extern AutoPtrStack<Tilestack> tilestackstack;

typedef bool (*Command)(const std::string &flag, Arglist &arglist);
bool register_command(Command cmd);

#endif
