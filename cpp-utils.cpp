#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include <stdexcept>
#include <vector>

#include <sys/stat.h>
#include <sys/utsname.h>

#include "cpp-utils.h"

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

string filename_suffix_with_dot(const string &filename)
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

  // Return everything starting with last dot
  return filename.substr(lastDot);
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

void make_directory(const string &dirname) {
#ifdef _WIN32
  _wmkdir(Unicode(dirname).path(), 0777);
#else
  mkdir(Unicode(dirname).path(), 0777);
#endif
}

void make_directory_and_parents(const string &dirname) {
  if (dirname == "") return;
  make_directory_and_parents(filename_directory(dirname));
  make_directory(dirname);
}

bool filename_exists(const string &filename) {
  struct stat s;
#ifdef _WIN32
  return (0 == _wstat(Unicode(filename).path(), &s));
#else  
  return (0 == stat(Unicode(filename).path(), &s));
#endif
}

FILE *fopen_utf8(const std::string &filename, const char *mode) {
#ifdef _WIN32
  return _wfopen(Unicode(filename).path(), mode);
#else  
  return fopen(filename.c_str(), mode);
#endif
  
}


bool iequals(const string &a, const string &b)
{
#ifdef _WIN32
  return !stricmp(a.c_str(), b.c_str());
#else
  return !strcasecmp(a.c_str(), b.c_str());
#endif
}

string temporary_path(const std::string &path)
{
  // TODO(RS): make this thread-safe if we someday use threads
  static unsigned int counter = 0;
  static string cached_hostname;
  if (!counter) cached_hostname = hostname();
  return string_printf("%s_%s_%d_%d_%d%s",
		       filename_sans_suffix(path).c_str(),
		       cached_hostname.c_str(),
		       (int) time(0),
		       getpid(),
		       counter++,
		       filename_suffix_with_dot(path).c_str());
}

string hostname()
{
  struct utsname u;
  if (uname(&u)) {
    perror("uname");
    exit(1);
  }
  return u.nodename;
}

void throw_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  string msg = string_vprintf(fmt, args);
  va_end(args);
  throw runtime_error(msg);
}

///// executable_path

// From http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
// Mac OS X: _NSGetExecutablePath() (man 3 dyld)
// Linux: readlink /proc/self/exe
// Solaris: getexecname()
// FreeBSD: sysctl CTL_KERN KERN_PROC KERN_PROC_PATHNAME -1
// BSD with procfs: readlink /proc/curproc/file
// Windows: GetModuleFileName() with hModule = NULL

#if defined(__APPLE__)
#include <mach-o/dyld.h>
string executable_path() {
  uint32_t len = 0;
  int ret = _NSGetExecutablePath(NULL, &len);
  assert(ret == -1);
  vector<char> buf(len);
  ret = _NSGetExecutablePath(&buf[0], &len);
  return string(&buf[0]);
}
#else
// Linux
string executable_path() {
  vector<char> buf(1000);
  while (1) {
    int ret = readlink("/proc/self/exe", &buf[0], buf.size());
    assert(ret > 0);
    if (ret < (int)buf.size()) return string(&buf[0], ret);
    buf.resize(buf.size()*2);
  }
}
#endif

///// home_directory

// Linux or OS X
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

string home_directory() {
  struct passwd pwd, *pwdptr;
  char buf[10000];
  int ret = getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &pwdptr);
  assert(ret == 0 && pwdptr);
  return pwdptr->pw_dir;
}

///// application_user_state_directory

#if defined(__APPLE__)
string application_user_state_directory(const string &application_name) {
  return home_directory() + "/Library/Application Support/" + application_name;  
}
#endif

///// Unicode paths

Unicode::Unicode(const std::string &utf8) : utf8(utf8) {}
Unicode::Unicode(const char *utf8) : utf8(utf8) {}
#ifdef _WIN32
#error Implement Me!
const wchar_t *Unicode::path() { assert(0); }
#else
const char *Unicode::path() { return utf8.c_str(); }
#endif

#if defined(__APPLE__)
string os() { return "osx"; }
#elif defined(_WIN32)
string os() { return "windows"; }
#else
string os() { return "linux"; }
#endif






