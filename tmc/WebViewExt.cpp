#include <QDropEvent>
#include "WebViewExt.h"

WebViewExt::WebViewExt(API *api, QWidget *parent) : QWebView(parent), api(api) {}

void WebViewExt::dropEvent(QDropEvent *de) {
  //fprintf(stderr, "drop event!\n");
  for (int i = 0; de->format(i); i++) {
    //fprintf(stderr, "  format %s\n", de->format(i));
  }
  if (de->provides("text/uri-list")) {
    QByteArray data = de->encodedData("text/uri-list");
    //fprintf(stderr, "Got test/uri-list of %d bytes:\n", data.length());
    //fwrite(data.constData(), data.length(), 1, stderr);
    QStringList uris = QUrl::fromPercentEncoding(data).split("\r\n", QString::SkipEmptyParts);
    for (int i = 0; i < uris.length(); i++) {
			#ifdef _WIN32
				uris[i] = uris[i].remove("file:///");
			#else
				uris[i] = uris[i].remove("file://");
			#endif
    }
    api->dropPaths(uris);
  }
  QWebView::dropEvent(de);
}
