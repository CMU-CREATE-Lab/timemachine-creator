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
public slots:
  int log();
  void addJSObject();
  QPixmap readThumbnail(QString path);
signals:
  int logged();
};

#endif
