#ifndef FILENAME_UTILS_H
#define FILENAME_UTILS_H

#include <ctime>
#include <stdarg.h>
#include <stdio.h>
#ifdef _WIN32
#include <wchar.h>
#endif

#include <string>
#include <vector>
#ifdef _WIN32
#define __attribute__(x)
#endif

std::string filename_sans_directory(const std::string &filename);
std::string filename_directory(const std::string &filename);
std::string filename_sans_suffix(const std::string &filename);
std::string filename_suffix(const std::string &filename);
std::string string_vprintf(const char *fmt, va_list args);
std::string string_printf(const char *fmt, ...)  __attribute__((format(printf,1,2)));
void make_directory(const std::string &dirname);
void make_directory_and_parents(const std::string &dirname);
bool filename_exists(const std::string &filename);
void rename_file(const std::string &src, const std::string &dest);
bool delete_file(const std::string &src);
FILE *fopen_utf8(const std::string &path, const char *mode);
std::string read_file(const std::string &path);
int system_utf8(const std::string &cmdline);
FILE *popen_utf8(const std::string &path, const char *mode);
#ifdef _WIN32
int pclose(FILE *p);
#endif

bool iequals(const std::string &a, const std::string &b);
std::string temporary_path(const std::string &path);
std::string hostname();
#ifdef _WIN32
__declspec(noreturn)
#endif
void throw_error(const char *fmt, ...) __attribute__((format(printf,1,2))) __attribute__((noreturn));
std::string executable_suffix();
std::string executable_path();
std::string application_user_state_directory(const std::string &application_name);
std::string os(); // osx, windows, linux

void get_cpu_usage(double &user, double &system);

class Unicode {
  std::string m_utf8;
#ifdef _WIN32
  std::vector<wchar_t> m_utf16;
#endif
public:
  Unicode(const std::string &utf8);
  Unicode(const char *utf8);
#ifdef _WIN32
  Unicode(const wchar_t *utf16);
  const wchar_t *path();
  const wchar_t *utf16();
#else
  const char *path();
#endif
  const char *utf8();
private:
  void init_from_utf8();
};

#endif
