#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <QApplication>
//#include <QWebView>
#include <QWebSettings>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QUrl>
#include <QMenu>
#include <QSystemTrayIcon>
#include <iostream>
#include "WebViewExt.h"

#include "cpp-utils.h"

#include "api.h"

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
	#ifdef _WIN32
		AllocConsole();
		freopen("CONOUT$", "w", stderr);
		freopen("CONOUT$", "w", stdout);
	#endif

  QApplication a(argc, argv);

  QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
  QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageEnabled, true);

  std::string local_storage_path = application_user_state_directory("Time Machine Creator") + "/localstorage.bin";

  make_directory(filename_directory(local_storage_path));

  QWebSettings::globalSettings()->setLocalStoragePath(local_storage_path.c_str());
  //QWebView view;
  API api;
  WebViewExt view(&api);
  view.setMinimumSize(640, 480); //set a minimum window size so things do not get too distorted
  //view.setGeometry(QRect(0,0,1875,210));

  //const QRect rect = QApplication::desktop()->rect();
  //fprintf(stderr, "%d %d %d %d\n", rect.left(), rect.top(), rect.width(), rect.height());

  //string svg="<svg width=\"100\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\"><g><text transform=\"matrix(1.7105170712402766,0,0,2.764879860687217,-8.190915471942073,-163.57711807450636) \" xml:space=\"preserve\" text-anchor=\"middle\" font-family=\"Sans-serif\" font-size=\"24\" id=\"svg_1\" y=\"94.99031\" x=\"34.03279\" stroke-width=\"0\" stroke=\"#000000\" fill=\"#000000\">25:00</text><text transform=\"matrix(3.4957764778942484,0,0,2.175630190673746,-28.347959144049643,8.474789503736556) \" font-weight=\"bold\" xml:space=\"preserve\" text-anchor=\"middle\" font-family=\"Sans-serif\" font-size=\"24\" id=\"svg_2\" y=\"13.74224\" x=\"21.55085\" stroke-width=\"0\" stroke=\"#000000\" fill=\"#007f00\">PT</text></g></svg>";

  api.setFrame(view.page()->mainFrame());
  // Signal is emitted before frame loads any web content:
  QObject::connect(view.page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()),
		   &api, SLOT(addJSObject()));

  QString path = QFileInfo("index.html").absoluteFilePath();

	#ifdef _WIN32
		view.load(QUrl("file:///" + path));
	#else
		view.load(QUrl("file://" + path));
	#endif

  view.show();

  return a.exec();
}
