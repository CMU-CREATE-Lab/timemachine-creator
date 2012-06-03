#include "apiprocess.h"

APIProcess::APIProcess(API *api, int callback_id) : api(api), callback_id(callback_id) {
  connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(receiveFinished(int, QProcess::ExitStatus)));
  connect(&process, SIGNAL(readyReadStandardError()), this, SLOT(receiveReadyReadStandardError()));
  connect(&process, SIGNAL(readyReadStandardOutput()), this, SLOT(receiveReadyReadStandardOutput()));
}

void APIProcess::receiveReadyReadStandardError() {
  fprintf(stderr, "%d: receiveReadyReadStandardError\n", callback_id);
  stderrBuffer.append(process.readAllStandardError());
  while (stderrBuffer.contains('\n')) {
    int pos = stderrBuffer.indexOf('\n');
    fprintf(stderr, "%d: sending line %s", callback_id, QString(stderrBuffer.left(pos+1)).toUtf8().constData());
    api->requestCallback(callback_id, QVariantList() << QString(stderrBuffer.left(pos+1)));
    stderrBuffer.remove(0, pos+1);
  }
}

void APIProcess::receiveReadyReadStandardOutput() {
  // We're ignoring standard output, but need to flush it to keep everything moving
  process.readAllStandardOutput();
}

void APIProcess::receiveFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/) {
  if (stderrBuffer.length()) api->requestCallback(callback_id, QVariantList() << QString(stderrBuffer));

  api->requestCallback(callback_id, QVariantList() << exitCode);
  
  // TODO(RS): figure out how to delete self?
}
