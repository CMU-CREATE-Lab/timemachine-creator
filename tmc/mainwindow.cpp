#include "mainwindow.h"

MainWindow::MainWindow()
{
	// creating the central widget. the web content is shown here.
	this->setCentralWidget(new QWidget);
        this->setWindowTitle("Time Machine Creator");

	// creating status bar
	createStatusBar();
	
	// creating actions
	openAction = new QAction(tr("&Open Project"), this);
	openAction->setShortcuts(QKeySequence::Open);
	openAction->setStatusTip(tr("Open a new time machine project"));
	saveAction = new QAction(tr("&Save"), this);
	saveAction->setShortcuts(QKeySequence::Save);
	saveAction->setStatusTip(tr("Save your time machine project"));
	saveAsAction = new QAction(tr("Save &As..."), this);
	saveAsAction->setShortcuts(QKeySequence::SaveAs);
	saveAsAction->setStatusTip(tr("Save as your time machine project"));
	addImagesAction = new QAction(tr("Add &Images"), this);
	addImagesAction->setShortcut(Qt::ControlModifier + Qt::ShiftModifier + Qt::Key_I);
	addImagesAction->setStatusTip(tr("Create a new project by adding your image files"));
	addFoldersAction = new QAction(tr("Add Fo&lders"), this);
	addFoldersAction->setShortcut(Qt::ControlModifier + Qt::ShiftModifier + Qt::Key_F);
	addFoldersAction->setStatusTip(tr("Create a new project by adding folders containing your image files"));
	
	for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActs[i] = new QAction(this);
        recentFileActs[i]->setVisible(false);
        connect(recentFileActs[i], SIGNAL(triggered()),
                this, SLOT(openRecentFile()));
    }
	
	exitAction = new QAction(tr("E&xit"), this);
	exitAction->setShortcuts(QKeySequence::Close);
	exitAction->setStatusTip(tr("Close the software"));
	
	undoAction = new QAction(tr("&Undo"), this);
	undoAction->setShortcuts(QKeySequence::Undo);
	undoAction->setStatusTip(tr("Undo your previous deletes"));
	redoAction = new QAction(tr("&Redo"), this);
	redoAction->setShortcuts(QKeySequence::Redo);
	redoAction->setStatusTip(tr("Redo your previous deletes"));
	
	aboutAction = new QAction(tr("&About"), this);
	aboutAction->setShortcut(Qt::Key_F1);
	aboutAction->setStatusTip(tr("About time machine creator software"));
	
	// creating connections
	connect(openAction, SIGNAL(triggered()), this, SLOT(open()));
	connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveAs()));
	connect(addImagesAction, SIGNAL(triggered()), this, SLOT(addImages()));
	connect(addFoldersAction, SIGNAL(triggered()), this, SLOT(addFolders()));
	connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));
	
	connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
	connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));
	
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
	
	// creating the menu bar
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(openAction);
	fileMenu->addAction(saveAction);
	fileMenu->addAction(saveAsAction);
	fileMenu->addSeparator();
	fileMenu->addAction(addImagesAction);
	fileMenu->addAction(addFoldersAction);
	separatorAct = fileMenu->addSeparator();
    for (int i = 0; i < MaxRecentFiles; ++i)
        fileMenu->addAction(recentFileActs[i]);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAction);
	updateRecentFileActions();
	
	fileMenu = menuBar()->addMenu(tr("&Edit"));
	fileMenu->addAction(undoAction);
	setUndoMenu(false);
	fileMenu->addAction(redoAction);
	setRedoMenu(false);
	
	fileMenu = menuBar()->addMenu(tr("&Help"));
	fileMenu->addAction(aboutAction);
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    //setWindowFilePath(curFile);
	this->setWindowTitle("Time Machine Creator - "+strippedName(curFile));

    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    settings.setValue("recentFileList", files);
	
    /*foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
        if (mainWin)
            mainWin->updateRecentFileActions();
    }*/
	updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(files[i]));
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData(files[i]);
        recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        recentFileActs[j]->setVisible(false);

    separatorAct->setVisible(numRecentFiles > 0);
 }

QString MainWindow::strippedName(const QString &fullFileName)
{
	return QFileInfo(fullFileName).dir().dirName();
}

void MainWindow::openRecentFile()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)
	{
		if (QFileInfo(action->data().toString()).exists())
			api->evaluateJavaScript("openRecentProject('"+action->data().toString()+"'); null");
		else {
			QSettings settings;
			QStringList files = settings.value("recentFileList").toStringList();
			files.removeAll(action->data().toString());
			settings.setValue("recentFileList", files);
			updateRecentFileActions();
			QMessageBox::critical(this,tr("File Does Not Exist"),tr("This file does not exist."));
		}
	}
}

void MainWindow::closeEvent(QCloseEvent *event)
 {
    if (api->closeApp()) {
		event->accept();
    } else {
        event->ignore();
    }
 }

void MainWindow::createStatusBar()
{
	statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setUndoMenu(bool state)
{
	undoAction->setEnabled(state);
}

void MainWindow::setRedoMenu(bool state)
{
	redoAction->setEnabled(state);
}

void MainWindow::setOpenProjectMenu(bool state)
{
	openAction->setEnabled(state);
}

void MainWindow::setSaveMenu(bool state)
{
	saveAction->setEnabled(state);
}

void MainWindow::setSaveAsMenu(bool state)
{
	saveAsAction->setEnabled(state);
}

void MainWindow::setAddImagesMenu(bool state)
{
	addImagesAction->setEnabled(state);
}

void MainWindow::setAddFoldersMenu(bool state)
{
	addFoldersAction->setEnabled(state);
}

void MainWindow::setRecentlyAddedMenu(bool state)
{
        for (int i = 0; i < MaxRecentFiles; ++i)
        recentFileActs[i]->setEnabled(state);
}

void MainWindow::setApi(API *api)
{
	this->api = api;
}

void MainWindow::open()
{
	api->evaluateJavaScript("openData(); null");
}

void MainWindow::addImages()
{
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFiles);
	dialog.setNameFilter("Image files (*.jpg *.jpeg *.png)");
	dialog.setViewMode(QFileDialog::Detail);
	if (dialog.exec()) {
		QStringList selectedFiles = dialog.selectedFiles();
		api->dropPaths(selectedFiles);
		api->evaluateJavaScript("imagesDropped(); null");
	}
	
	return;
}

void MainWindow::addFolders()
{
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setViewMode(QFileDialog::Detail);
	if (dialog.exec()) {
		QStringList selectedFiles = dialog.selectedFiles();
		api->dropPaths(selectedFiles);
		api->evaluateJavaScript("imagesDropped(); null");
	}
	
	return;
}

void MainWindow::saveAs()
{
	api->evaluateJavaScript("saveAs(); null");
}

void MainWindow::save()
{
	api->evaluateJavaScript("save(); null");
}

void MainWindow::undo()
{
	api->evaluateJavaScript("undoAction(); null");
}

void MainWindow::redo()
{
	api->evaluateJavaScript("redoAction(); null");
}

void MainWindow::about()
{
	QMessageBox::about(this,"About","Time Machine Creator, v1.0\nCREATE Lab, 2012");
}