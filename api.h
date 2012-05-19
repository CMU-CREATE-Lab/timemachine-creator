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
  API(const std::string &rootdir);
  void setFrame(QWebFrame *frame);
  void dropPaths(QStringList files);
  void requestCallback(int id, QVariantList args);
  Q_INVOKABLE int log();
  Q_INVOKABLE void addJSObject();
  Q_INVOKABLE QPixmap readThumbnail(QString path);
  Q_INVOKABLE double exifTime(QString path);
  Q_INVOKABLE QStringList droppedFilesRecursive();
  Q_INVOKABLE QString saveAsDialog(QString caption, QString startingDirectory, QString filter);
  Q_INVOKABLE bool writeFile(QString path, QString data);
  Q_INVOKABLE QString readFile(QString path);
  Q_INVOKABLE bool makeDirectory(QString path);
  Q_INVOKABLE bool invokeRubySubprocess(QStringList args, int callback_id);
signals:
  void callback(int id, QVariantList args);
protected:
  QStringList droppedPaths;
  std::string rootdir;
};

#endif
