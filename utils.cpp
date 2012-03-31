#include "filename_utils.h"

// From Randy Sargent's public domain library, 2001-2012

using namespace std;

#ifdef _WIN32
// Windows
#define ALLOWABLE_DIRECTORY_DELIMITERS "/\\"
#define DIRECTORY_DELIMITER '\\'
#define DIRECTORY_DELIMITER_STRING "\\"
#else
// UNIX
#define ALLOWABLE_DIRECTORY_DELIMITERS "/"
#define DIRECTORY_DELIMITER '/'
#define DIRECTORY_DELIMITER_STRING "/"
#endif

string filename_sans_directory(const string &filename)
{
  size_t lastDirDelim = filename.find_last_of(ALLOWABLE_DIRECTORY_DELIMITERS);
  // No directory delimiter, so return filename
  if (lastDirDelim == string::npos) return filename;
  // Return everything after the delimiter
  return filename.substr(lastDirDelim+1);
}

string filename_directory(const string &filename)
{
  size_t lastDirDelim = filename.find_last_of(ALLOWABLE_DIRECTORY_DELIMITERS);
  // No directory delimiter, so return nothing
  if (lastDirDelim == string::npos) return "";
  // Return everything up to just before the last delimiter
  return filename.substr(0, lastDirDelim);
}

string filename_sans_suffix(const string &filename)
{
  // Find the last '.'
  size_t lastDot = filename.find_last_of(".");
  if (lastDot == string::npos) return filename;

  // Find the last directory delimiter
  size_t lastDirDelim = filename.find_last_of(ALLOWABLE_DIRECTORY_DELIMITERS);

  if (lastDirDelim != string::npos &&
      lastDot < lastDirDelim) {
    // The last dot was inside the directory name, so return as is
    return filename;
  }

  // Return everything up to the last dot
  return filename.substr(0, lastDot);
}

string filename_suffix(const string &filename)
{
  // Find the last '.'
  size_t lastDot = filename.find_last_of(".");
  if (lastDot == string::npos) return "";

  // Find the last directory delimiter
  size_t lastDirDelim = filename.find_last_of(ALLOWABLE_DIRECTORY_DELIMITERS);

  if (lastDirDelim != string::npos &&
      lastDot < lastDirDelim) {
    // The last dot was inside the directory name, so no suffix
    return "";
  }

  // Return everything after the last dot
  return filename.substr(lastDot+1);
}

string string_vprintf(const char *fmt, va_list args) {
  size_t size = 500;
  char *buf = (char *)malloc(size);
  // grow the buffer size until the output is no longer truncated
  while (1) {
    va_list args_copy;
    va_copy(args_copy, args);
#if defined(_WIN32)
    size_t nwritten = _vsnprintf(buf, size-1, fmt, args_copy);
#else
    size_t nwritten = vsnprintf(buf, size-1, fmt, args_copy);
#endif
    // Some c libraries return -1 for overflow, some return a number larger than size-1
    if (nwritten < size-2) {
      buf[nwritten+1] = 0;
      string ret(buf);
      free(buf);
      return ret;
    }
    size *= 2;
    buf = (char *)realloc(buf, size);
  }
}

string string_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  string ret = string_vprintf(fmt, args);
  va_end(args);
  return ret;
}

