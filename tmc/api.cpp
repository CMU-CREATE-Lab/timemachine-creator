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
#include "cpp_utils.h"
#include "Exif.h"

using namespace std;

APIProcess *process;

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

bool API::closeApp() {
	QVariant v = frame->evaluateJavaScript("isSafeToClose();");
	return v.toBool();
}

void API::doCloseApp() {
        qApp->quit();
}

void API::openBrowser(QString url) {
#if defined Q_WS_WIN
        url = "file:///"+url;
        // search whether user has chrome installed
        QSettings brwCH("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe",QSettings::NativeFormat);
        QString brwPath = brwCH.value( "Default", "0" ).toString();
        if(brwPath!="0") {
                QProcess::startDetached(brwPath, QStringList() << "--new-window" << url);
                return;
        }

        // probably the preferred way to find Chrome, but the above has worked on multiple systems tested
        QSettings brwCH2("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome",QSettings::NativeFormat);
        brwPath = brwCH2.value( "InstallLocation", "0" ).toString();
        if(brwPath!="0") {
                QProcess::startDetached(brwPath+"\\chrome.exe", QStringList() << "--new-window" << url);
                return;
        }
        
        // search whether user has safari installed
        QSettings brwSA("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Safari.exe",QSettings::NativeFormat);
        brwPath = brwSA.value( "Default", "0" ).toString();
        if(brwPath!="0") {
                QProcess::startDetached(brwPath, QStringList() << url);
                return;
        }
        
        // tell user that they don't have any compatible browser and exit
        QMessageBox::critical(mainwindow,tr("No Browser"),tr("There is no compatible browser installed on this computer.\nYou need either Chrome or Safari in order to view your time machine."));
        return;
        
#elif defined Q_WS_MAC
        url = "file://"+url;
        if(QFileInfo("/Applications/Google Chrome.app").exists())
                //QProcess::startDetached("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", QStringList() << "--new-window" << url);
                system(string_printf("open -a 'Google Chrome' '%s'", QUrl(url).toEncoded().replace("'", "%27").constData()).c_str());
        else if(QFileInfo("/Applications/Safari.app").exists())
                QProcess::startDetached("/Applications/Safari.app/Contents/MacOS/Safari", QStringList() << url);
        else
                QDesktopServices::openUrl(url);

#else //Linux or other
        /*if (QProcess::startDetached("chromium", QStringList() << url))
	        return;

        if (QProcess::startDetached("chromium-browser", QStringList() << "--new-window" << url))
	        return;*/

        if (QProcess::startDetached("google-chrome", QStringList() << "--new-window" << url))
            return;

        // go with the default browser
        //QDesktopServices::openUrl(url);

        QMessageBox::critical(mainwindow,tr("No Browser"),tr("There is no compatible browser installed on this computer.\nYou need Google Chrome in order to view your time machine."));
#endif
}

void API::setDeleteMenu(bool state) {
        mainwindow->setDeleteMenu(state);
}

void API::setUndoMenu(bool state) {
	mainwindow->setUndoMenu(state);
}

void API::setRedoMenu(bool state) {
	mainwindow->setRedoMenu(state);
}

void API::setNewProjectMenu(bool state) {
	mainwindow->setNewProjectMenu(state);
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

void API::setRecentlyAddedMenu(bool state) {
        mainwindow->setRecentlyAddedMenu(state);
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

// filter, e.g. "*.tmc"
QString API::saveAsDialog(QString caption, QString startingDirectory, QString filter) {
  return QFileDialog::getSaveFileName(NULL, caption, startingDirectory, filter);
}

// TODO: make this a more generic method
bool API::writeFile(QString path, QString data)
{
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream(&file) << data;
  mainwindow->setCurrentFile(path);
  QString projectExtension = ".tmc";
  QString viewerPath = path.left(path.lastIndexOf("/")-(projectExtension.length())).append(".timemachine/");
  qApp->setProperty("PROJECT_VIEWER_PATH", viewerPath);
  file.close();
  return true;
}

QString API::readFileDialog(QString caption, QString startingDirectory, QString filter)
{
  // We do not use native dialog boxes because on Mac OS our data diretory ends in .tmc and Mac thinks this is the item we wanted
  QString path = QFileDialog::getOpenFileName(NULL, caption, startingDirectory, filter, NULL, QFileDialog::DontUseNativeDialog);

  if (path != "") {
    // first check if this project has an outdated viewer
    QString projectExtension = ".tmc";
    QString viewerPath = path.left(path.lastIndexOf("/")-(projectExtension.length())).append(".timemachine/");
    qApp->setProperty("PROJECT_VIEWER_PATH", viewerPath);
    QString versionFile = viewerPath.append("VERSION");
    checkViewerVersion(versionFile);
    openedProject = path;
  }
  return readFile(path);
}

QString API::openProjectFile(QString path)
{
  if (path != "") {
    // first check if this project has an outdated viewer
    QString projectExtension = ".tmc";
    QString viewerPath = path.left(path.lastIndexOf("/")-(projectExtension.length())).append(".timemachine/");
    qApp->setProperty("PROJECT_VIEWER_PATH", viewerPath);
    QString versionFile = viewerPath.append("VERSION");
    checkViewerVersion(versionFile);
    openedProject = path;
    mainwindow->setCurrentFile(path);
    return readFile(path);
  }
  return NULL;
}

QString API::readFile(QString path)
{
  if (path != "") {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return ""; // null would be better
    QString input = QTextStream(&file).readAll();
    file.close();
    return input;
  }
  return NULL;
}

bool copyDir(const QString source, const QString destination, const bool override, const QStringList exclude) {
  qApp->processEvents();

  QDir directory(source);
  bool error = false;

  if (!directory.exists()) {
    return false;
  }

  QStringList dirs =  directory.entryList(QDir::AllDirs | QDir::Hidden | QDir::NoDotAndDotDot);
  QStringList files = directory.entryList(QDir::Files | QDir::Hidden);
  bool doSkipDir, doSkipFile;

  QList<QString>::iterator d,f;

  for (d = dirs.begin(); d != dirs.end(); ++d) {

    if (!QFileInfo(directory.path() + "/" + (*d)).isDir()) {
      continue;
    }

    doSkipDir = false;
    for (int i = 0; i < exclude.length(); i++) {
      if (exclude[i] == *d) {
        doSkipDir = true;
        break;
      }
    }
    if (doSkipDir) continue;

    QDir temp(destination + "/" + (*d));
    temp.mkpath(temp.path());

    if (!copyDir(directory.path() + "/" + (*d), destination + "/" + (*d), override, exclude)) {
      error = true;
    }
  }

  for (f = files.begin(); f != files.end(); ++f) {
    QFile tempFile(directory.path() + "/" + (*f));

    if (QFileInfo(directory.path() + "/" + (*f)).isDir()) {
      continue;
    }

    doSkipFile = false;
    for (int i = 0; i < exclude.length(); i++) {
      if (exclude[i] == *f) {
        doSkipFile = true;
        break;
      }
    }
    if (doSkipFile) continue;

    QFile destFile(destination + "/" + directory.relativeFilePath(tempFile.fileName()));

    if (destFile.exists() && override) {
      destFile.remove();
    }

    if (!tempFile.copy(destination + "/" + directory.relativeFilePath(tempFile.fileName()))) {
      error = true;
    }
  }

  return !error;
}

bool API::checkViewerVersion(QString path)
{
  QString viewerVersion = readFile(path);

  if (QString::compare(viewerVersion,qApp->property("APP_VERSION").toString()) < 0) {
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString msg = "Old viewer detected: [" + viewerVersion + "] vs [" + qApp->property("APP_VERSION").toString() + "] Updating...";
    qDebug() << msg;

    QProgressDialog *dialog = new QProgressDialog();
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setLabelText("Updating viewer files...");
    dialog->setCancelButton(0);
    dialog->show();
    qApp->processEvents();

    QString srcPath = getRootAppPath().append("/time-machine-explorer/");
    QString dstPath = path.left(path.lastIndexOf("VERSION"));

    QStringList exclude;
    exclude << "archive" << "cgi-bin" << "htmlets" << "utils" << "tests"
            << ".git" << ".gitignore" << "favicon.ico" << "iframe.html"
            << "integrated-viewer.html" << "save_time_warp.php" << "standalone.html"
            << "TimeMachineExplorer.iml" << "TimeMachineExplorer.ipr";

    copyDir(srcPath,dstPath,true,exclude);

    dialog->close();
    delete dialog;
    QApplication::restoreOverrideCursor();
  }
  return true;
}

QString API::getOpenedProjectPath()
{
	return openedProject;
}

bool API::makeDirectory(QString path)
{
  return QDir().mkdir(path);
}

bool API::makeFullDirectoryPath(QString path)
{
  return QDir().mkpath(path);
}

bool API::fileExists(QString path)
{
  return QDir().exists(path);
}

bool API::invokeRubySubprocess(QStringList args, int callback_id)
{
  //fprintf(stderr, "invokeRubySubprocess:");
  qDebug() << "invokeRubySubprocess:";

  for (int i = 0; i < args.length(); i++) {
    //fprintf(stderr, " %s", args[i].toUtf8().constData());
    qDebug() << args[i].toUtf8().constData();
  }
  //fprintf(stderr, "\n");
  process = new APIProcess(this, callback_id);

  // Ruby path
  std::string ruby_path;
  
  if (os() == "windows") {
    ruby_path = rootdir + "/ruby/windows/bin/ruby.exe";
  } else {
    ruby_path = "/usr/bin/ruby";
  }

  //fprintf(stderr, "Invoking ruby with path '%s'\n", ruby_path.c_str());
  qDebug() << QString("Invoking ruby with path: ").append(ruby_path.c_str());
  process->process.start(ruby_path.c_str(), args, QIODevice::ReadOnly);
  return true;
}

bool API::killSubprocess() {
  process->process.kill();
  delete process;
  return true;
}

QString API::getRootAppPath()
{
  return QString::fromStdString(rootdir);
}
