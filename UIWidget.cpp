#include "MainWindow.h"
#include "UIWidget.h"
#include "ui_UIWidget.h"

UIWidget::UIWidget(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::UIWidget)
{
	ui->setupUi(this);
//	setWindowFlags(Qt::Popup | Qt::Sheet | Qt::FramelessWindowHint);

//	connect(ui->pushButton_start, &QPushButton::clicked, mainwindow(), &MainWindow::on_pushButton_start_clicked);
}

UIWidget::~UIWidget()
{
	delete ui;
}

QListWidget *UIWidget::listWidget_input_device()
{
	return ui->listWidget_input_device;
}

QListWidget *UIWidget::listWidget_input_connection()
{
	return ui->listWidget_input_connection;
}

QListWidget *UIWidget::listWidget_display_mode()
{
	return ui->listWidget_display_mode;
}

QCheckBox *UIWidget::checkBox_audio()
{
	return ui->checkBox_audio;
}

QCheckBox *UIWidget::checkBox_display_mode_auto_detection()
{
	return ui->checkBox_display_mode_auto_detection;
}

MainWindow *UIWidget::mainwindow()
{
//	return qobject_cast<MainWindow *>(parent());
	return mainwindow_;
}

void UIWidget::closeEvent(QCloseEvent *event)
{
	mainwindow()->close();
}

void UIWidget::on_listWidget_input_device_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_input_device_currentRowChanged(currentRow);
}


void UIWidget::on_listWidget_input_connection_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_input_connection_currentRowChanged(currentRow);
}

void UIWidget::on_listWidget_display_mode_currentRowChanged(int currentRow)
{
	mainwindow()->on_listWidget_display_mode_currentRowChanged(currentRow);
}

void UIWidget::on_checkBox_audio_stateChanged(int arg1)
{
	mainwindow()->on_checkBox_audio_stateChanged(arg1);
}

void UIWidget::on_checkBox_display_mode_auto_detection_clicked()
{
	mainwindow()->checkBox_display_mode_auto_detection();
}
