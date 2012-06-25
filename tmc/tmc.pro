SOURCES = main.cpp api.cpp apiprocess.cpp Exif.cpp ExifData.cpp mainwindow.cpp WebViewExt.cpp ../cpp_utils/cpp_utils.cpp
INCLUDEPATH += ../cpp_utils
HEADERS += mainwindow.h api.h apiprocess.h WebViewExt.h ../cpp_utils/cpp_utils.h
CONFIG(debug, debug|release) {
    CONFIG += console
}
QT += webkit

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5

win32:RC_FILE += tmc.rc
