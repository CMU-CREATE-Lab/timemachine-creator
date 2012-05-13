#ifndef TILESTACKTOOL_H
#define TILESTACKTOOL_H

#include <cstdlib>
#include <list>
#include <string>

#include "json/json.h"

void usage(const char *fmt, ...);

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
  double shift_int() {
    std::string arg = shift();
    char *end;
    double ret = strtol(arg.c_str(), &end, 0);
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
      usage("Can't parse '%s' as json: %s", arg.c_str());
    }
    return ret;
  }
};

template <typename T>
class AutoPtrStack {
  std::list<T*> stack;
public:
  void push(std::auto_ptr<T> t) {
    stack.push_back(t.release());
  }
  std::auto_ptr<T> pop() {
    assert(!stack.empty());
    T* ret = stack.back();
    stack.pop_back();
    return std::auto_ptr<T>(ret);
  }
};

extern AutoPtrStack<Tilestack> tilestackstack;

typedef bool (*Command)(const std::string &flag, Arglist &arglist);
bool register_command(Command cmd);

#endif
