#ifndef WEB_VIEW_EXT_H
#define WEB_VIEW_EXT_H

#include <QWebView>
#include "api.h"

class WebViewExt : public QWebView {
  Q_OBJECT
public:
  WebViewExt(API *api, QWidget *parent = NULL);
protected:
  API *api;
  void dropEvent(QDropEvent *de);
};

#endif
