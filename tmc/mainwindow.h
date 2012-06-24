#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui>
#include <QList>
#include "api.h"

class API;

class MainWindow : public QMainWindow
{
    Q_OBJECT
	API *api;

public:
    MainWindow();
	void setApi(API *api);
        void setDeleteMenu(bool state);
	void setUndoMenu(bool state);
	void setRedoMenu(bool state);
	void setNewProjectMenu(bool state);
	void setOpenProjectMenu(bool state);
	void setSaveMenu(bool state);
	void setSaveAsMenu(bool state);
	void setAddImagesMenu(bool state);
	void setAddFoldersMenu(bool state);
	void setRecentlyAddedMenu(bool state);
	void setCurrentFile(const QString &fileName);
	
protected:
    void closeEvent(QCloseEvent *event);
	
private slots:
	void newProject();
	void open();
	void save();
	void saveAs();
	void addImages();
	void addFolders();
	void openRecentFile();
	
        void deleteImages();

	void undo();
	void redo();
	
	void about();
	
private:
	QMenu *fileMenu;
	
	QAction *newAction;
	QAction *openAction;
	QAction *addImagesAction;
	QAction *addFoldersAction;
	QAction *saveAction;
	QAction *saveAsAction;
	QAction *separatorAct;
	QAction *exitAction;
	
        QAction *deleteAction;

	QAction *undoAction;
	QAction *redoAction;
	
	QAction *aboutAction;
	
	enum { MaxRecentFiles = 5 };
    QAction *recentFileActs[MaxRecentFiles];
	
	void createStatusBar();
    void updateRecentFileActions();
    QString strippedName(const QString &fullFileName);
	
	QString curFile;
};

#endif