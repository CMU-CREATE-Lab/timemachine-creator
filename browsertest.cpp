#include <QApplication>
#include <QWebView>
#include <QUrl>
 
int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
 
  QWebView view;
  view.load(QUrl("http://g7.gigapan.org/timemachines/sabramson/0100-unstitched/canvas.html"));
  view.show();

  return a.exec();
}
