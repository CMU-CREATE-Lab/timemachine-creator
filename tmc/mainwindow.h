#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include "api.h"

class API;

class MainWindow : public QMainWindow
{
    Q_OBJECT
	API *api;

public:
    MainWindow();
	void setApi(API *api);
	void setUndoMenu(bool state);
	
private slots:
	void open();
	void save();
	void addImages();
	void addFolders();
	
	void undo();
	
	void about();
	
private:
	QMenu *fileMenu;
	
	QAction *openAction;
	QAction *addImagesAction;
	QAction *addFoldersAction;
	QAction *saveAction;
	QAction *exitAction;
	
	QAction *undoAction;
	
	QAction *aboutAction;
};

#endif