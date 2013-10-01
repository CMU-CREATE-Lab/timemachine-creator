#ifndef JSON_H
#define JSON_H

#include "json/json.h"
#include "simple_shared_ptr.h"
#include "cpp_utils.h"

class JSON {
 private:
  enum Type {
    UNINITIALIZED = 0,
    ROOT,
    KEYREF,
    AREF
  } type;
  Json::Value json;               // always valid
  std::string root;               // valid if type == ROOT
  simple_shared_ptr<JSON> parent; // valid if type != ROOT
  std::string key;                // valid if type == KEYREF
  int index;                      // valid if type == AREF
  
 public:
  JSON(const std::string &str);
  JSON();
  
  static JSON fromFile(const std::string &filename);
  
  JSON operator[](const std::string &key) const;
  bool hasKey(const std::string &key) const;
  JSON operator[](int index) const;
  unsigned int size() const; // array length

  double doub() const;
  int integer() const;
  std::string str() const;
  bool boolean() const;

  double get(const std::string &key, double deflt) const;
  int get(const std::string &key, int deflt) const;
  std::string get(const std::string &key, std::string deflt) const;
  bool get(const std::string &key, bool deflt) const;

  bool isArray() const;
  bool isObject() const;
  bool isNumeric() const;
  bool isStr() const;
  bool isBoolean() const;
  bool isNull() const;

 private:
  JSON(Type type);
  void error(const char *fmt, ...) const;
  std::string describe() const;
};

#endif
