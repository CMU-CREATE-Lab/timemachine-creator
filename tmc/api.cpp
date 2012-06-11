#include <stdio.h>
#include <string>

#include <QDir>
#include <QFileDialog>
#include <QFileInfoList>
#include <QList>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>
#include <QVariant>

#include "api.h"
#include "apiprocess.h"
#include "cpp-utils.h"
#include "Exif.h"

using namespace std;

API::API(const std::string &rootdir) : rootdir(rootdir) {
}

void API::setFrame(QWebFrame *frame) {
  this->frame = frame;
}

void API::setWindow(MainWindow *mainwindow) {
	this->mainwindow = mainwindow;
}

void API::evaluateJavaScript(const QString & scriptSource) {
  frame->evaluateJavaScript(scriptSource);
}

void API::setUndoMenu(bool state) {
	mainwindow->setUndoMenu(state);
}

void API::setRedoMenu(bool state) {
	mainwindow->setRedoMenu(state);
}

void API::setOpenProjectMenu(bool state) {
	mainwindow->setOpenProjectMenu(state);
}

void API::setSaveMenu(bool state) {
	mainwindow->setSaveMenu(state);
}

void API::setSaveAsMenu(bool state) {
	mainwindow->setSaveAsMenu(state);
}

void API::setAddImagesMenu(bool state) {
	mainwindow->setAddImagesMenu(state);
}

void API::setAddFoldersMenu(bool state) {
	mainwindow->setAddFoldersMenu(state);
}

int API::log() {
  fprintf(stderr, "log!\n");
  QVariantList args;
  args << "hello";
  args << 33;
  requestCallback(44, args);
  return 33;
}

void API::addJSObject() {
  frame->addToJavaScriptWindowObject(QString("_api"), this);
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

    FILE *in = fopen_utf8(path_utf8, "rb"); //was rw
    EXIF_ASSERT(in, ExifErr() << "Can't read " << path_utf8);
    ExifView exif(path_utf8);
    size_t offset = exif.get_thumbnail_location();
    int32 length;
    exif.query_by_tag(EXIF_ThumbnailLength, length);
    EXIF_ASSERT(length > 0, ExifErr() << "Illegal thumbnail length");
    vector<unsigned char> buf(length);
    EXIF_ASSERT(fseek(in, offset, SEEK_SET) == 0, ExifErr() << "Error reading thumbnail");
    //EXIF_ASSERT(1 == (int32)fread(&buf[0], length, 1, in), ExifErr() << "Error reading thumbnail");
    fread(&buf[0], length, 1, in);
    fclose(in);

    QPixmap ret;
    ret.loadFromData(&buf[0], length);

    return ret;
  } catch (ExifErr &e) {
    fprintf(stderr, "ExifErr: %s\n", e.what());
    return QPixmap();
  }
}

double API::exifTime(QString path) {
  try {
    string path_utf8 = path.toUtf8().constData();
    ExifView exif(path_utf8);
    ExifDateTime capture_time = exif.get_capture_time();
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = capture_time.m_year - 1900;
    tm.tm_mon = capture_time.m_month - 1;
    tm.tm_mday = capture_time.m_day;
    tm.tm_hour = capture_time.m_hour;
    tm.tm_min = capture_time.m_minute;
    tm.tm_sec = capture_time.m_second;
    double seconds = (double) mktime(&tm);

    int32 hundredths = 0;
    try {
      exif.query_by_tag(EXIF_SubSecTimeDigitized, hundredths);
    } catch (ExifErr &) {}

    return seconds + hundredths / 100.0;
  } catch (ExifErr &e) {
    fprintf(stderr, "ExifErr: %s\n", e.what());
    // Would be nicer to return null here, but don't know how to do that
    return -1.0;
  }
}

QStringList listDirectoryRecursively(QString path) {
  QStringList ret;
  QFileInfoList dir = QDir(path).entryInfoList();
  for (int i = 0; i < dir.length(); i++) {
    if (dir[i].fileName() == "." || dir[i].fileName() == "..") {
      // ignore
    } else if (dir[i].isDir()) {
      ret << listDirectoryRecursively(dir[i].absoluteFilePath());
    } else {
      ret << dir[i].absoluteFilePath();
    }
  }
  return ret;
}

void API::dropPaths(QStringList paths) {
  droppedPaths = paths;
}

void API::requestCallback(int id, QVariantList args) {
  emit callback(id, args);
}

QStringList API::droppedFilesRecursive() {

  QStringList ret;
  for (int i = 0; i < droppedPaths.length(); i++) {
    //fprintf(stderr, "dfr: >%s<\n", droppedPaths[i].toUtf8().constData());
    QFileInfo info(droppedPaths[i]);
    if (info.isDir()) {
      ret << listDirectoryRecursively(droppedPaths[i]);
    } else {
      ret << info.absoluteFilePath();
    }
  }
  ret.removeDuplicates();
  ret.sort();
  return ret;
}

// filter, e.g. "*.timemachinedefinition"
QString API::saveAsDialog(QString caption, QString startingDirectory, QString filter) {
  return QFileDialog::getSaveFileName(NULL, caption, startingDirectory, filter);
}

bool API::writeFile(QString path, QString data)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream(&file) << data;
  return true;
}

QString API::readFile(QString caption, QString startingDirectory, QString filter)
{
  QString path = QFileDialog::getOpenFileName(NULL, caption, startingDirectory, filter);
  if(path != "") {
	  QFile file(path);
	  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return ""; // null would be better
	  openedProject = path;
	  return QTextStream(&file).readAll();
  }
  return NULL;
}

QString API::getOpenedProjectPath()
{
	return openedProject;
}

bool API::makeDirectory(QString path)
{
  return QDir().mkdir(path);
}

bool API::invokeRubySubprocess(QStringList args, int callback_id)
{
  fprintf(stderr, "invokeRubySubprocess:");
  for (int i = 0; i < args.length(); i++) {
    fprintf(stderr, " %s", args[i].toUtf8().constData());
  }
  fprintf(stderr, "\n");
  APIProcess *process = new APIProcess(this, callback_id);

  // Ruby path
  std::string ruby_path;
  
  if (os() == "windows") {
    ruby_path = rootdir + "/ruby/windows/bin/ruby.exe";
  } else {
    ruby_path = "/usr/bin/ruby";
  }

  fprintf(stderr, "Invoking ruby with path '%s'\n", ruby_path.c_str());
  process->process.start(ruby_path.c_str(), args, QIODevice::ReadOnly);
  return true;
}
