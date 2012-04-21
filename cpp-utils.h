#ifndef FILENAME_UTILS_H
#define FILENAME_UTILS_H

#include <stdarg.h>

#include <string>

std::string filename_sans_directory(const std::string &filename);
std::string filename_directory(const std::string &filename);
std::string filename_sans_suffix(const std::string &filename);
std::string filename_suffix(const std::string &filename);
std::string string_vprintf(const char *fmt, va_list args);
std::string string_printf(const char *fmt, ...)  __attribute__((format(printf,1,2)));
void mkdir_parents(const std::string &dirname);
bool filename_exists(const std::string &filename);
bool iequals(const std::string &a, const std::string &b);
std::string temporary_path(const std::string &path);
std::string hostname();
void throw_error(const char *fmt, ...) __attribute__((format(printf,1,2))) __attribute__((noreturn));
std::string executable_path();
#endif
