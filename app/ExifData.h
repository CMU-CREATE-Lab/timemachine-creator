// Adapted from Vision Workbench, which is licensed under the NASA Open Source Agreement, version 1.3

// __BEGIN_LICENSE__
// Copyright (C) 2006-2011 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


// Note: The following code for parsing EXIF information in the Vision
// Workbench camera module was adapted from JHead : the EXIF Jpeg
// header manipulation tool written by Matthias Wandel
// (http://www.sentex.net/~mwandel/jhead/).  Here is the JHead
// copyright notice:
//
//    Jhead is public domain software - that is, you can do whatever
//    you want with it, and include it in software that is licensed under
//    the GNU or the BSD license, or whatever other licence you chose,
//    including proprietary closed source licenses.  Although not part
//    of the license, I do expect common courtesy, please.
//
//    -Matthias Wandel
//

#ifndef __EXIF_DATA_H__
#define __EXIF_DATA_H__

#include <map>
#include <cstdio>
#include <stdexcept>
#include <sstream>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef int int32;

struct ExifErr : public std::exception {
  /// The default constructor generates exceptions with empty error
  /// message text.  This is the cleanest approach if you intend to
  /// use streaming (via operator <<) to generate your message.
  ExifErr() {}
  
  virtual ~ExifErr() throw() {}
  
  /// Copy Constructor
  ExifErr( ExifErr const& e ) : std::exception(e) {
    m_desc << e.m_desc.str();
  }
  
  /// Assignment operator copies the error string.
  ExifErr& operator=( ExifErr const& e ) {
    m_desc.str( e.m_desc.str() );
    return *this;
  }
  
  /// Returns a the error message text for display to the user.  The
  /// returned pointer must be used immediately; other operations on
  /// the exception may invalidate it.  If you need the data for
    /// later, you must save it to a local buffer of your own.
  virtual const char* what() const throw() {
    m_what_buf = m_desc.str();
    return m_what_buf.c_str();
  }
  
  /// Returns the error message text as a std::string.
  std::string desc() const { return m_desc.str(); }
  
  /// Returns a string version of this exception's type.
  virtual std::string name() const { return "ExifErr"; }
  
  void set( std::string const& s ) { m_desc.str(s); }
  void reset() { m_desc.str(""); }
  
  virtual void default_throw() const { throw *this; }
    
  template <class T>
  ExifErr& operator<<( T const& t ) { stream() << t; return *this; }

 protected:
  virtual std::ostringstream& stream() {return m_desc;}
  
 private:
  // The error message text.
  std::ostringstream m_desc;
  
  // A buffer for storing the full exception description returned by
  // the what() method, which must generate its return value from
  // the current value of m_desc.  The what() method provides no
  // mechanism for freeing the returned string, and so we handle
  // allocation of that memory here, internally to the exception.
  mutable std::string m_what_buf;
};

#define EXIF_ASSERT(cond,excep) do { if(!(cond)) throw( excep ); } while(0)

typedef enum { IntType, DoubleType, StringType } ExifDataType;

typedef struct {
  ExifDataType type;
  union {
    int i;
    double d;
    char* s;
  } value;
} ExifTagData;

class ExifData {
private:
  static const int NUM_FORMATS = 12;

  std::map<unsigned int, ExifTagData> tags;
  bool MotorolaOrder;
  unsigned int ExifLocation;

  int process_tiff_header(const unsigned char * buffer);
  bool read_jpeg_sections(FILE* infile);
  bool read_tiff_ifd(const std::string &filename);
  void process_exif(unsigned char * ExifSection, unsigned int length);
  void process_exif_dir(const unsigned char * DirStart, const unsigned char * OffsetBase,
                        unsigned ExifLength, int NestingLevel);
  const unsigned char * dir_entry_addr(const unsigned char * start, int entry);
  void Put16u(void * Short, unsigned short PutValue);
  int Get16u(const void * Short);
  int Get32s(const void * Long);
  void Put32u(void * Value, unsigned PutValue);
  unsigned Get32u(const void * Long);
  double convert_any_format(const void * ValuePtr, int Format);

public:
  ExifData() {}
  ~ExifData();

  bool import_data(std::string const &filename);

  bool get_tag_value(const uint16 tag, int &value) const;
  bool get_tag_value(const uint16 tag, double &value) const;
  bool get_tag_value(const uint16 tag, std::string &value) const;
  unsigned int get_exif_location() const;

  void print_debug();
};

#endif
