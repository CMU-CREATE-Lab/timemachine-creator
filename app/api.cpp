#include <stdio.h>
#include <string>

#include "api.h"
#include "cpp-utils.h"
#include "Exif.h"

using namespace std;

void API::setFrame(QWebFrame *frame) {
  this->frame = frame;
}

int API::log() {
  fprintf(stderr, "log!\n");
  return 33;
}

void API::addJSObject() {
  frame->addToJavaScriptWindowObject(QString("api"), this);
}

// readThumbnail:  
// Example
// <img id="thumbnail" style="width:160px; height:120px">
// <script>
// window.api.readThumbnail("../patp10/00IMG_9946.JPG").assignToHTMLImageElement(document.getElementById('thumbnail'));
// </script>

QPixmap API::readThumbnail(QString path) {
  try {
    string path_utf8 = path.toUtf8().constData();
    FILE *in = fopen_utf8(path_utf8, "rw");
    EXIF_ASSERT(in, ExifErr() << "Can't read " << path_utf8);
    ExifView exif(path_utf8);
    size_t offset = exif.get_thumbnail_location();
    int32 length;
    exif.query_by_tag(EXIF_ThumbnailLength, length);
    EXIF_ASSERT(length > 0, ExifErr() << "Illegal thumbnail length");
    vector<unsigned char> buf(length);
    EXIF_ASSERT(fseek(in, offset, SEEK_SET) == 0, ExifErr() << "Error reading thumbnail");
    EXIF_ASSERT(1 == (int32)fread(&buf[0], length, 1, in), ExifErr() << "Error reading thumbnail");
    fclose(in);
    QPixmap ret;
    ret.loadFromData(&buf[0], length);
    return ret;
  } catch (ExifErr &e) {
    fprintf(stderr, "ExifErr: %s\n", e.what());
    return QPixmap();
  }
}
