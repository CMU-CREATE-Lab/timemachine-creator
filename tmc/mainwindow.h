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
	void setRedoMenu(bool state);
	void setOpenProjectMenu(bool state);
	void setSaveMenu(bool state);
	void setSaveAsMenu(bool state);
	void setAddImagesMenu(bool state);
	void setAddFoldersMenu(bool state);
	
private slots:
	void open();
	void save();
	void saveAs();
	void addImages();
	void addFolders();
	
	void undo();
	void redo();
	
	void about();
	
private:
	QMenu *fileMenu;
	
	QAction *openAction;
	QAction *addImagesAction;
	QAction *addFoldersAction;
	QAction *saveAction;
	QAction *saveAsAction;
	QAction *exitAction;
	
	QAction *undoAction;
	QAction *redoAction;
	
	QAction *aboutAction;
	
	void createStatusBar();
};

#endif