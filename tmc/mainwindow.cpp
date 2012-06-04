#include "mainwindow.h"

MainWindow::MainWindow()
{
	// creating the central widget. the web content is shown here.
	this->setCentralWidget(new QWidget);
	
	// creating status bar
	createStatusBar();
	
	// creating actions
	openAction = new QAction(tr("&Open Project"), this);
	openAction->setShortcuts(QKeySequence::Open);
	openAction->setStatusTip(tr("Open a new time machine project"));
	saveAction = new QAction(tr("&Save Project"), this);
	saveAction->setShortcuts(QKeySequence::Save);
	saveAction->setStatusTip(tr("Save your time machine project"));
	addImagesAction = new QAction(tr("Add &Images"), this);
	addImagesAction->setShortcut(Qt::ControlModifier + Qt::ShiftModifier + Qt::Key_I);
	addImagesAction->setStatusTip(tr("Create a new project by adding your image files"));
	addFoldersAction = new QAction(tr("Add Fo&lders"), this);
	addFoldersAction->setShortcut(Qt::ControlModifier + Qt::ShiftModifier + Qt::Key_F);
	addFoldersAction->setStatusTip(tr("Create a new project by adding folders containing your image files"));
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
	connect(addImagesAction, SIGNAL(triggered()), this, SLOT(addImages()));
	connect(addFoldersAction, SIGNAL(triggered()), this, SLOT(addFolders()));
	connect(exitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
	
	connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
	connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));
	
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
	
	// creating the menu bar
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(openAction);
	fileMenu->addAction(saveAction);
	fileMenu->addSeparator();
	fileMenu->addAction(addImagesAction);
	fileMenu->addAction(addFoldersAction);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAction);
	
	fileMenu = menuBar()->addMenu(tr("&Edit"));
	fileMenu->addAction(undoAction);
	setUndoMenu(false);
	fileMenu->addAction(redoAction);
	setRedoMenu(false);
	
	fileMenu = menuBar()->addMenu(tr("&Help"));
	fileMenu->addAction(aboutAction);
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

void MainWindow::save()
{
	api->evaluateJavaScript("saveData(); null");
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
	QMessageBox::about(this,"About","Time Machine Creator, v1.0\nCreate Lab, 2012");
}