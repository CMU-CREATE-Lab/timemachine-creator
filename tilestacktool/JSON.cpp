#include <assert.h>
#include <stdexcept>
#include <fstream>


#include "JSON.h"

JSON::JSON(const std::string &str) {
  type = ROOT;
  Json::Reader reader;
  if (!reader.parse(str, json)) {
    error("Can't parse JSON string");
  }
  root = str;
}

JSON::JSON() {
  type = UNINITIALIZED;
}

JSON JSON::fromFile(const std::string &filename) {
  std::ifstream in(filename.c_str());
  if (!in.is_open()) {
    assert(0);
    throw std::runtime_error(string_printf("Can't open %s for input", filename.c_str()));
  }
  return JSON(std::string((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>()));
}

JSON JSON::operator[](const std::string &key) const {
  if (!isObject()) {
    error("Tried to get field '%s' from non-object", key.c_str());
  }
  if (!hasKey(key)) {
    error("Tried to get missing field '%s'", key.c_str());
  }
  JSON ret(KEYREF);
  ret.json = json[key];
  ret.parent.reset(new JSON(*this));
  ret.key = key;
  return ret;
}

bool JSON::hasKey(const std::string &key) const {
  if (!isObject()) {
    error("Tried to check for field '%s' from non-object", key.c_str());
  }
  return json.isMember(key);
}

JSON JSON::operator[](int index) const {
  if (!isArray()) {
    error("Tried to get array element %d from a non-array", index);
  }
  if (index < 0 || index >= (int)size()) {
    error("Tried to access array element %d beyond bounds of array", index);
  }
  JSON ret(AREF);
  ret.json = json[index];
  ret.parent.reset(new JSON(*this));
  ret.index = index;
  return ret;
}

unsigned int JSON::size() const {
  if (!isArray()) {
    error("Tried to get size of non-array", index);
  }
  return json.size();
}

double JSON::doub() const {
  if (!isNumeric()) {
    error("Value is not a number");
  }
  return json.asDouble();
}

int JSON::integer() const {
  double val = doub();
  int ret = (int) val;
  if (ret != val) {
    error("Number is not an integer");
  }
  return ret;
}

std::string JSON::str() const {
  if (!isStr()) {
    error("Value is not a string");
  }
  return json.asString();
}

bool JSON::boolean() const {
  if (!isBoolean()) {
    error("Value is not true/false");
  }
  return json.asBool();
}

double JSON::get(const std::string &key, double deflt) const {
  return hasKey(key) ? (*this)[key].doub() : deflt;
}

int JSON::get(const std::string &key, int deflt) const {
  return hasKey(key) ? (*this)[key].integer() : deflt;
}

std::string JSON::get(const std::string &key, std::string deflt) const {
  return hasKey(key) ? (*this)[key].str() : deflt;
}

bool JSON::get(const std::string &key, bool deflt) const {
  return hasKey(key) ? (*this)[key].boolean() : deflt;
}

bool JSON::isArray() const {
  return json.isArray();
}

bool JSON::isObject() const {
  return json.isObject();
}

bool JSON::isNumeric() const {
  return json.isNumeric();
}

bool JSON::isStr() const {
  return json.isString();
}

bool JSON::isBoolean() const {
  return json.isBool();
}

bool JSON::isNull() const {
  return json.isNull();
}

// private

JSON::JSON(Type type) : type(type) {
}

void JSON::error(const char *fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  std::string msg = string_vprintf(fmt, args);
  throw std::runtime_error(msg + " " + describe());
}

std::string JSON::describe() const {
  switch (type) {
  case ROOT:
    return string_printf("[JSON string of length %ld]", (long)root.length());
  case KEYREF:
    return string_printf("%s.%s", parent->describe().c_str(), key.c_str());
  case AREF:
    return string_printf("%s[%d]", parent->describe().c_str(), index);
  default:
    throw std::runtime_error("internal error: illegal type in JSON::describe");
  }
}
