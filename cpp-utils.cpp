#ifdef _WIN32
  #define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
  #include <process.h>
  #include <Windows.h>
  #include <Userenv.h>
  #pragma comment(lib, "userenv.lib")
  #pragma comment(lib, "Advapi32.lib")
#else
  #include <strings.h>
  #include <sys/utsname.h>
  #include <sys/time.h>
  #include <sys/resource.h>
#endif

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
#if defined(_WIN32)
    args_copy = args;
    size_t nwritten = _vsnprintf(buf, size-1, fmt, args_copy);
#else
    va_copy(args_copy, args);
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
  _wmkdir(Unicode(dirname).path());
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
#ifdef _WIN32
  struct _stat64i32 s;
  return (0 == _wstat(Unicode(filename).path(), &s));
#else
  struct stat s;
  return (0 == stat(Unicode(filename).path(), &s));
#endif
}

FILE *fopen_utf8(const std::string &filename, const char *mode) {
#ifdef _WIN32
  return _wfopen(Unicode(filename).path(), Unicode(mode).path());
#else
  return fopen(filename.c_str(), mode);
#endif

}


bool iequals(const string &a, const string &b)
{
#ifdef _WIN32
  return !_stricmp(a.c_str(), b.c_str());
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
#ifdef _WIN32
	int pid = _getpid();
#else
	int pid = getpid();
#endif
  return string_printf("%s_%s_%d_%d_%d%s",
		       filename_sans_suffix(path).c_str(),
		       cached_hostname.c_str(),
		       (int) time(0),
		       pid,
		       counter++,
		       filename_suffix_with_dot(path).c_str());
}

#ifdef _WIN32
string hostname()
{
	char buf[1000];
	DWORD bufsize = sizeof(buf);
	GetComputerNameExA(ComputerNameDnsHostname, buf, &bufsize);
	return string(buf, bufsize);
}
#else
string hostname()
{
  struct utsname u;
  if (uname(&u)) {
    perror("uname");
    exit(1);
  }
  return u.nodename;
}
#endif

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

#if defined(_WIN32)
string executable_path() {
	wchar_t buf[10000];
	DWORD bufsize = sizeof(buf);
	GetModuleFileNameW(NULL, buf, bufsize);
	return Unicode(buf).utf8();
}

#elif defined(__APPLE__)
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

#ifdef _WIN32
string home_directory() {
	TCHAR buf[10000]={0};
	DWORD bufsize = sizeof(buf);
	HANDLE token = 0;
	OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
	GetUserProfileDirectory(token, buf, &bufsize);
	CloseHandle(token);
	return Unicode(buf).utf8();
}

#else

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
#endif

///// application_user_state_directory

#if defined(_WIN32)
string application_user_state_directory(const string &application_name) {
  return home_directory() + "/Application Data/" + application_name;
}

#elif defined(__APPLE__)
string application_user_state_directory(const string &application_name) {
  return home_directory() + "/Library/Application Support/" + application_name;
}
#endif

///// Unicode paths

Unicode::Unicode(const std::string &utf8) : m_utf8(utf8) {
	init_from_utf8();
}
Unicode::Unicode(const char *utf8) : m_utf8(utf8) {
	init_from_utf8();
}
const char *Unicode::utf8() { return m_utf8.c_str(); }
#ifdef _WIN32
Unicode::Unicode(const wchar_t *utf16) : m_utf16(utf16, utf16+wcslen(utf16)+1) {
	vector<char> tmp(m_utf16.size() * 4);
	wcstombs(&tmp[0], utf16, tmp.size());
	m_utf8 = string(&tmp[0]);
}
const wchar_t *Unicode::utf16() { return &m_utf16[0]; }
void Unicode::init_from_utf8() {
  // calculate size of output buffer (this includes terminating NULL)
  int nchars = MultiByteToWideChar(CP_UTF8, 0, m_utf8.c_str(), -1, NULL, 0);
  assert(nchars>0);
  m_utf16.resize(nchars);
  // see http://msdn.microsoft.com/en-us/library/windows/desktop/dd319072%28v=vs.85%29.aspx
  // this time, do the conversion and write to m_utf16
  int nchars_written = MultiByteToWideChar(CP_UTF8, 0, m_utf8.c_str(), -1, &m_utf16[0], nchars);
  assert(nchars_written == nchars);
}
const wchar_t *Unicode::path() { return utf16(); }
#else
void Unicode::init_from_utf8() {}
const char *Unicode::path() { return utf8(); }
#endif

#if defined(__APPLE__)
string os() { return "osx"; }
#elif defined(_WIN32)
string os() { return "windows"; }
#else
string os() { return "linux"; }
#endif

#if defined(_WIN32)

double filetime_to_double(struct _FILETIME &ft) {
  unsigned long long t = ft.dwLowDateTime + ((unsigned long long)ft.dwHighDateTime << 32);
  return t / 1e7;
}

void get_cpu_usage(double &user, double &system) {
  struct _FILETIME creation_time, exit_time, kernel_time, user_time;
  GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time);
  user = filetime_to_double(user_time);
  system = filetime_to_double(kernel_time);
}

#else

double tv_to_double(struct timeval &tv) {
  return tv.tv_sec + tv.tv_usec / 1e6;
}
void get_cpu_usage(double &user, double &system) {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  user = tv_to_double(usage.ru_utime);
  system = tv_to_double(usage.ru_stime);
}

#endif
