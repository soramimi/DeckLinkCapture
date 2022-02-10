#ifndef OVERLAYWINDOW_H
#define OVERLAYWINDOW_H

#include <QDialog>

class MainWindow;
class QListWidget;
class QListWidgetItem;
class QCheckBox;

namespace Ui {
class OverlayWindow;
}

class OverlayWindow : public QWidget {
	Q_OBJECT
private:
	Ui::OverlayWindow *ui;
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow();
protected:
	void closeEvent(QCloseEvent *event);
public:
	explicit OverlayWindow(QWidget *parent = nullptr);
	~OverlayWindow();
	void bindMainWindow(MainWindow *mw)
	{
		mainwindow_ = mw;
	}
	QListWidget *listWidget_input_device();
	QListWidget *listWidget_input_connection();
	QListWidget *listWidget_display_mode();
	QCheckBox *checkBox_audio();
	QCheckBox *checkBox_display_mode_auto_detection();

public slots:
	void accept() {}
	void reject() {}
private slots:
	void on_listWidget_input_device_currentRowChanged(int currentRow);
	void on_listWidget_input_connection_currentRowChanged(int currentRow);
	void on_listWidget_display_mode_currentRowChanged(int currentRow);
	void on_checkBox_audio_stateChanged(int arg1);
	void on_checkBox_display_mode_auto_detection_clicked();
};

#endif // OVERLAYWINDOW_H
