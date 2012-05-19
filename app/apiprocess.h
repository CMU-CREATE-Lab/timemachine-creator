#ifndef API_PROCESS_H
#define API_PROCESS_H

#include <QObject>
#include <QPixmap>
#include <QProcess>
#include <QString>
#include <QWebFrame>

#include "api.h"

class APIProcess : public QObject
{
  Q_OBJECT
public:
  APIProcess(API *api, int callback_id);
  QProcess process;
  API *api;
  int callback_id;
signals:
  void receive_stderr_line(QString line);
  void finished(int exitCode);
private slots:
  void receiveReadyReadStandardError();
  void receiveReadyReadStandardOutput();
  void receiveFinished(int exitCode, QProcess::ExitStatus exitStatus);
private:
  QByteArray stderrBuffer;
};

#endif
