#include "FullScreenWindow.h"
#include "ui_FullScreenWindow.h"
#include "GlobalData.h"
#include "Image.h"
#include "MainWindow.h"
#include <QCloseEvent>
#include <QShortcut>

FullScreenWindow::FullScreenWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::FullScreenWindow)
{
	ui->setupUi(this);

	ui->centralwidget->setViewMode(ImageWidget::ViewMode::FitToWindow);

	connect(new QShortcut(QKeySequence("F11"), this), &QShortcut::activated, [&](){
		qobject_cast<MainWindow *>(this->parent())->setFullScreen(false);
	});

	setCursor(global->invisible_cursor);
}

FullScreenWindow::~FullScreenWindow()
{
	delete ui;
}

void FullScreenWindow::setImage(const QImage &image)
{
	ui->centralwidget->setImage(image);
}

QSize FullScreenWindow::scaledSize(const Image &image)
{
	return ui->centralwidget->scaledSize(image);
}

void FullScreenWindow::closeEvent(QCloseEvent *event)
{
	event->ignore();
	qobject_cast<MainWindow *>(this->parent())->setFullScreen(false);
}
