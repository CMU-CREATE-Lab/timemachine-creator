#ifndef API_H
#define API_H

#include <QObject>
#include <QWebFrame>
#include <QString>
#include <QPixmap>

class API : public QObject
{
  Q_OBJECT
  QWebFrame *frame;
public:
  void setFrame(QWebFrame *frame);
  void dropPaths(QStringList files);
public slots:
  int log();
  void addJSObject();
  QPixmap readThumbnail(QString path);
  double exifTime(QString path);
  QStringList droppedFilesRecursive();
signals:
  int logged();
protected:
  QStringList droppedPaths;
};

#endif
