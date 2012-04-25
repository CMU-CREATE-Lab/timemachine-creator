#ifndef FILENAME_UTILS_H
#define FILENAME_UTILS_H

#include <stdarg.h>
#ifdef _WIN32
#include <wchar.h>
#endif

#include <string>

std::string filename_sans_directory(const std::string &filename);
std::string filename_directory(const std::string &filename);
std::string filename_sans_suffix(const std::string &filename);
std::string filename_suffix(const std::string &filename);
std::string string_vprintf(const char *fmt, va_list args);
std::string string_printf(const char *fmt, ...)  __attribute__((format(printf,1,2)));
void make_directory(const std::string &dirname);
void make_directory_and_parents(const std::string &dirname);
bool filename_exists(const std::string &filename);
bool iequals(const std::string &a, const std::string &b);
std::string temporary_path(const std::string &path);
std::string hostname();
void throw_error(const char *fmt, ...) __attribute__((format(printf,1,2))) __attribute__((noreturn));
std::string executable_path();
std::string application_user_state_directory(const std::string &application_name);

class Unicode {
  std::string utf8;
#ifdef _WIN32  
  vector<unsigned short> utf16;
#endif
public:
  Unicode(const std::string &utf8);
  Unicode(const char *utf8);
#ifdef _WIN32
  const wchar_t *path();
#else
  const char *path();
#endif
};

#endif
