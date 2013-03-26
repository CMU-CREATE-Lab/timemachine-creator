#ifdef _WIN32
  #define _CRT_SECURE_NO_WARNINGS
  #include <io.h>
  #include <Windows.h>
#endif

#include <iostream>

#include <QApplication>
#include <QWebSettings>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QUrl>
#include <QMenu>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSizePolicy>

#include "WebViewExt.h"
#include "cpp_utils.h"
#include "api.h"
#include "mainwindow.h"

// Directory structure is in ../README.txt

#define TIMESTAMP QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toAscii().data()

void customMessageHandler(QtMsgType type, const char *msg)
{
  QString txt;
  switch (type) {
  case QtDebugMsg:
    txt = QString("[DEBUG] ").append(TIMESTAMP).append(" ").append(msg);
    break;
  case QtWarningMsg:
    txt = QString("[WARN] ").append(TIMESTAMP).append(" ").append(msg);
    break;
  case QtCriticalMsg:
    txt = QString("[CRITICAL] ").append(TIMESTAMP).append(" ").append(msg);
    break;
  case QtFatalMsg:
    txt = QString("[FATAL] ").append(TIMESTAMP).append(" ").append(msg);
    abort();
  }

  if (qApp->property("PROJECT_VIEWER_PATH").toString() != "") {
    QFile outFile(qApp->property("PROJECT_VIEWER_PATH").toString().append("tm.log"));
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << txt << endl;
    outFile.close();
  }
}

int main(int argc, char *argv[])
{
  #ifndef QT_NO_DEBUG
    #ifdef _WIN32
      AllocConsole();
      freopen("CONOUT$", "w", stderr);
      freopen("CONOUT$", "w", stdout);
    #endif
  #endif

  QApplication a(argc, argv);

  // Logging messages to console (eg qDebug()) only in debug mode
  // otherwise print to file in release mode
  #ifdef QT_NO_DEBUG
    qInstallMsgHandler(customMessageHandler);
  #endif

  // don't delete this! it is required for QSettings
  a.setOrganizationName("Create Lab");
  a.setApplicationName("Time Machine Creator");

  // Get root directory
  std::string exedir = filename_directory(executable_path());
  std::string rootdir;

  if (os() == "windows") {
    if (filename_exists(exedir + "/tmc")) {
      // Deployed path
      rootdir = exedir;
    } else {
      // Development path
      rootdir = filename_directory(filename_directory(exedir));
    }
  } else if (os() == "osx") {
    std::string devfile = exedir + "/../../../tmc.pro";
    //fprintf(stderr, "Looking for %s\n", devfile.c_str());
    if (filename_exists(devfile)) {
      //fprintf(stderr, "exists; development\n");
      // Development path
      rootdir = filename_directory(filename_directory(filename_directory(filename_directory(exedir))));
    } else {
      //fprintf(stderr, "doesn't exist; deployed\n");
      // Deployed path
      rootdir = filename_directory(exedir);
    }
  } else {
    rootdir = filename_directory(exedir);
  }

  // Set initial path to the root application path (until a project is opened or created)
  a.setProperty("PROJECT_VIEWER_PATH", QString(rootdir.c_str()).append("/"));

  //fprintf(stderr, "Root directory: '%s'\n", rootdir.c_str());

  std::string path = rootdir + "/tmc/index.html";

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--test")) {
      path = rootdir + "/tmc/test.html";
    }
  }

  QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
  QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageEnabled, true);

  std::string local_storage_path = application_user_state_directory("Time Machine Creator") + "/localstorage.bin";

  make_directory(filename_directory(local_storage_path));

  QWebSettings::globalSettings()->setLocalStoragePath(local_storage_path.c_str());
  API api(rootdir);

  MainWindow * windowMenu = new MainWindow;
  windowMenu->setApi(&api);
  QVBoxLayout *layout = new QVBoxLayout(windowMenu->centralWidget());

  WebViewExt view(&api);
  view.setMinimumSize(640,480); //set a minimum window size so things do not get too distorted

  layout->addWidget(&view);
  windowMenu->centralWidget()->setLayout(layout);
  windowMenu->centralWidget()->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);

  api.setFrame(view.page()->mainFrame());
  api.setWindow(windowMenu);

  // App Version (used for the creator software and time-machine-explorer)
  // Read in the value from the VERSION file found in the time-machine-explorer directory
  QString versionFile = api.getRootAppPath().append("/time-machine-explorer/VERSION");
  QString viewerVersion = api.readFile(versionFile);
  a.setProperty("APP_VERSION", viewerVersion);

  // Ruby path
  if (os() == "windows") {
	a.setProperty("RUBY_PATH", QString(rootdir.c_str()).append("/ruby/windows/bin/ruby.exe"));
  } else {
	a.setProperty("RUBY_PATH", "/usr/bin/ruby");
  }

  // Signal is emitted before frame loads any web content:
  QObject::connect(view.page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()),
		   &api, SLOT(addJSObject()));

  std::string url;
  #ifdef _WIN32
    url = "file:///" + path;
  #else
    url = "file://" + path;
  #endif
  //fprintf(stderr, "Loading '%s'\n", url.c_str());
  view.load(QUrl(url.c_str()));

  // Disable right click in webkit when in release mode
  #ifdef QT_NO_DEBUG
    view.setContextMenuPolicy(Qt::NoContextMenu);
  #endif

  // Set the application icon (ico file in root applicatoin path)
  view.setWindowIcon(QIcon(":/tmc.ico"));

  windowMenu->show();

  return a.exec();
}
