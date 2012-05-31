#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include "api.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
	API *api;

public:
    MainWindow();
	void setApi(API *api);
	
private slots:
	void open();
	void save();
	
	void about();
	
private:
	QMenu *fileMenu;
	
	QAction *openAction;
	QAction *saveAction;
	QAction *exitAction;
	
	QAction *undoAction;
	
	QAction *aboutAction;
};

#endif