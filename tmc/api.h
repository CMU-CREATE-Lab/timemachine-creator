#ifndef API_H
#define API_H

#include <QObject>
#include <QWebFrame>
#include <QString>
#include <QPixmap>
#include "mainwindow.h"

class MainWindow;

class API : public QObject
{
  Q_OBJECT
  QWebFrame *frame;
  MainWindow *mainwindow;
  
public:
  API(const std::string &rootdir);
  void setFrame(QWebFrame *frame);
  void setWindow(MainWindow *mainwindow);
  void evaluateJavaScript(const QString & scriptSource);
  void dropPaths(QStringList files);
  void requestCallback(int id, QVariantList args);
  bool closeApp();
  
  Q_INVOKABLE int log();
  Q_INVOKABLE void addJSObject();
  Q_INVOKABLE QPixmap readThumbnail(QString path);
  Q_INVOKABLE double exifTime(QString path);
  Q_INVOKABLE QStringList droppedFilesRecursive();
  Q_INVOKABLE QString saveAsDialog(QString caption, QString startingDirectory, QString filter);
  Q_INVOKABLE bool writeFile(QString path, QString data);
  Q_INVOKABLE QString readFileDialog(QString caption, QString startingDirectory, QString filter);
  Q_INVOKABLE QString readFile(QString path);
  Q_INVOKABLE bool makeDirectory(QString path);
  Q_INVOKABLE bool invokeRubySubprocess(QStringList args, int callback_id);
  Q_INVOKABLE void setUndoMenu(bool state);
  Q_INVOKABLE void setRedoMenu(bool state);
  Q_INVOKABLE QString getOpenedProjectPath();
  Q_INVOKABLE void setOpenProjectMenu(bool state);
  Q_INVOKABLE void setSaveMenu(bool state);
  Q_INVOKABLE void setSaveAsMenu(bool state);
  Q_INVOKABLE void setAddImagesMenu(bool state);
  Q_INVOKABLE void setAddFoldersMenu(bool state);
  Q_INVOKABLE void setRecentlyAddedMenu(bool state);
  Q_INVOKABLE void openBrowser(QString url);
  Q_INVOKABLE bool makeFullDirectoryPath(QString path);
  Q_INVOKABLE bool fileExists(QString path);
  Q_INVOKABLE void doCloseApp();
  
signals:
  void callback(int id, QVariantList args);
  
protected:
  QStringList droppedPaths;
  QString openedProject;
  std::string rootdir;
};

#endif
