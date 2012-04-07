#ifndef FILENAME_UTILS_H
#define FILENAME_UTILS_H

#include <stdarg.h>

#include <string>

std::string filename_sans_directory(const std::string &filename);
std::string filename_directory(const std::string &filename);
std::string filename_sans_suffix(const std::string &filename);
std::string filename_suffix(const std::string &filename);
std::string string_vprintf(const char *fmt, va_list args);
std::string string_printf(const char *fmt, ...);

#endif
