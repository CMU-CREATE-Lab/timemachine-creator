#include <QApplication>
#include <QWebView>
#include <QWebSettings>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QUrl>
#include <QMenu>
#include <QSystemTrayIcon>
#include <iostream>

#include "cpp-utils.h"

#include "api.h"

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
  QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageEnabled, true);
  
  std::string local_storage_path = application_user_state_directory("Time Machine Creator") + "/localstorage.bin";

  make_directory(filename_directory(local_storage_path));
  
  QWebSettings::globalSettings()->setLocalStoragePath(local_storage_path.c_str());
  QWebView view;
  //view.setGeometry(QRect(0,0,1875,210));

  //const QRect rect = QApplication::desktop()->rect();
  //fprintf(stderr, "%d %d %d %d\n", rect.left(), rect.top(), rect.width(), rect.height());

  //string svg="<svg width=\"100\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\"><g><text transform=\"matrix(1.7105170712402766,0,0,2.764879860687217,-8.190915471942073,-163.57711807450636) \" xml:space=\"preserve\" text-anchor=\"middle\" font-family=\"Sans-serif\" font-size=\"24\" id=\"svg_1\" y=\"94.99031\" x=\"34.03279\" stroke-width=\"0\" stroke=\"#000000\" fill=\"#000000\">25:00</text><text transform=\"matrix(3.4957764778942484,0,0,2.175630190673746,-28.347959144049643,8.474789503736556) \" font-weight=\"bold\" xml:space=\"preserve\" text-anchor=\"middle\" font-family=\"Sans-serif\" font-size=\"24\" id=\"svg_2\" y=\"13.74224\" x=\"21.55085\" stroke-width=\"0\" stroke=\"#000000\" fill=\"#007f00\">PT</text></g></svg>";

  API api;
  
  api.setFrame(view.page()->mainFrame());
  // Signal is emitted before frame loads any web content:
  QObject::connect(view.page()->mainFrame(), SIGNAL(javaScriptWindowObjectCleared()),
		   &api, SLOT(addJSObject()));
  

  QString path = QFileInfo("test.html").absoluteFilePath();
  view.load(QUrl("file://" + path));
  view.show();

  return a.exec();
}
