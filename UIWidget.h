#ifndef UIWIDGET_H
#define UIWIDGET_H

#include <QDialog>

class MainWindow;
class QListWidget;
class QListWidgetItem;
class QCheckBox;

namespace Ui {
class UIWidget;
}

class UIWidget : public QWidget {
	Q_OBJECT
private:
	Ui::UIWidget *ui;
	MainWindow *mainwindow_ = nullptr;
	MainWindow *mainwindow();
protected:
	void closeEvent(QCloseEvent *event) override;
public:
	explicit UIWidget(QWidget *parent = nullptr);
	~UIWidget() override;
	void bindMainWindow(MainWindow *mw)
	{
		mainwindow_ = mw;
	}
	QListWidget *listWidget_input_device();
	QListWidget *listWidget_input_connection();
	QListWidget *listWidget_display_mode();
	QCheckBox *checkBox_audio();
	QCheckBox *checkBox_deinterlace();
	QCheckBox *checkBox_display_mode_auto_detection();
private slots:
#if 0
	void on_listWidget_input_device_currentRowChanged(int currentRow);
	void on_listWidget_input_connection_currentRowChanged(int currentRow);
	void on_listWidget_display_mode_currentRowChanged(int currentRow);
	void on_checkBox_audio_stateChanged(int arg1);
	void on_checkBox_deinterlace_stateChanged(int arg1);
	void on_checkBox_display_mode_auto_detection_clicked();
#endif
};

#endif // UIWIDGET_H
