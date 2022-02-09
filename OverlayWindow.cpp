#include "MainWindow.h"
#include "OverlayWindow.h"
#include "ui_OverlayWindow.h"

OverlayWindow::OverlayWindow(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::OverlayWindow)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Popup | Qt::Sheet | Qt::FramelessWindowHint);

//	connect(ui->pushButton_start, &QPushButton::clicked, mainwindow(), &MainWindow::on_pushButton_start_clicked);
}

OverlayWindow::~OverlayWindow()
{
	delete ui;
}

QListWidget *OverlayWindow::listWidget_input_device()
{
	return ui->listWidget_input_device;
}

QListWidget *OverlayWindow::listWidget_input_connection()
{
	return ui->listWidget_input_connection;
}

QListWidget *OverlayWindow::listWidget_display_mode()
{
	return ui->listWidget_display_mode;
}

QCheckBox *OverlayWindow::checkBox_audio()
{
	return ui->checkBox_audio;
}

QCheckBox *OverlayWindow::checkBox_display_mode_auto_detection()
{
	return ui->checkBox_display_mode_auto_detection;
}

MainWindow *OverlayWindow::mainwindow()
{
	return qobject_cast<MainWindow *>(parent());
}

void OverlayWindow::closeEvent(QCloseEvent *event)
{
	mainwindow()->close();
}

void OverlayWindow::on_listWidget_input_device_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_input_device_currentRowChanged(currentRow);
}


void OverlayWindow::on_listWidget_input_connection_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_input_connection_currentRowChanged(currentRow);
}

void OverlayWindow::on_listWidget_display_mode_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_display_mode_currentRowChanged(currentRow);
}

void OverlayWindow::on_checkBox_audio_stateChanged(int arg1)
{
	mainwindow()->on_checkBox_audio_stateChanged(arg1);
}

void OverlayWindow::on_checkBox_display_mode_auto_detection_clicked()
{
	mainwindow()->checkBox_display_mode_auto_detection();
}
