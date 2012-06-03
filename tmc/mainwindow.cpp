#include "mainwindow.h"

MainWindow::MainWindow()
{
	// creating the central widget. the web content is shown here.
	this->setCentralWidget(new QWidget);
	
	// creating actions
	openAction = new QAction(tr("&Open"), this);
	saveAction = new QAction(tr("&Save"), this);
	exitAction = new QAction(tr("E&xit"), this);
	
	undoAction = new QAction(tr("&Undo"), this);
	
	aboutAction = new QAction(tr("&About"), this);
	
	// creating connections
	connect(openAction, SIGNAL(triggered()), this, SLOT(open()));
	connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));
	connect(exitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
	
	connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));
	
	connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
	
	// creating the menu bar
	fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(openAction);
	fileMenu->addAction(saveAction);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAction);
	
	fileMenu = menuBar()->addMenu(tr("&Edit"));
	fileMenu->addAction(undoAction);
	setUndoMenu(false);
	
	fileMenu = menuBar()->addMenu(tr("&Help"));
	fileMenu->addAction(aboutAction);
}

void MainWindow::setUndoMenu(bool state)
{
	undoAction->setEnabled(state);
}

void MainWindow::setApi(API *api)
{
	this->api = api;
}

void MainWindow::open()
{
	api->evaluateJavaScript("openData(); null");
}

void MainWindow::save()
{
	api->evaluateJavaScript("saveData(); null");
}

void MainWindow::undo()
{
	api->evaluateJavaScript("undoAction(); null");
}

void MainWindow::about()
{
	QMessageBox::about(this,"About","Time Machine Creator, v1.0\nCreate Lab, 2012");
}