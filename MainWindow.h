#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ui_MainWindow.h"
#include "DeckLinkCapture.h"
#include "DeckLinkDeviceDiscovery.h"
#include "ImageWidget.h"
#include "ProfileCallback.h"
#include "common.h"
#include <QEvent>
#include <QMainWindow>
#include <QWidget>
#include <memory>

class DeckLinkDeviceDiscovery;
class DeckLinkInputDevice;
class ProfileCallback;

class MainWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::MainWindow *ui;

	struct Private;
	Private *m;

	void setStatusBarText(const QString &text);
	bool isVideoFormatAutoDetectionEnabled() const;
	void updateUI();
	void internalStartCapture(bool start);
	void toggleRecord();
public:
	explicit MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

	void setPixelFormat(BMDPixelFormat pixel_format);

	void customEvent(QEvent* event);
	void closeEvent(QCloseEvent *event);

	void setup();

	void startCapture();
	void stopCapture();
	void restartCapture();
	void toggleCapture();

	void refreshDisplayModeMenu(void);
	void refreshInputConnectionMenu(void);
	void addDevice(IDeckLink *decklink);
	void removeDevice(IDeckLink *deckLink);
	void haltStreams(void);
	void updateProfile(IDeckLinkProfile* newProfile);

	bool isCapturing() const;

	void changeInputDevice(int selectedDeviceIndex);
	void changeInputConnection(BMDVideoConnection conn, bool errorcheck);
	void changeDisplayMode(BMDDisplayMode dispmode, double fps);
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool isAudioCaptureEnabled() const;
	void stopRecord();
private slots:
	void onPlayAudio(QByteArray const &samples);
	void on_checkBox_display_mode_auto_detection_clicked(bool checked);
	void on_listWidget_display_mode_currentRowChanged(int currentRow);
	void on_listWidget_display_mode_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_input_connection_currentRowChanged(int currentRow);
	void on_listWidget_input_device_currentRowChanged(int currentRow);
	void on_pushButton_start_clicked();
	void on_comboBox_deinterlace_currentIndexChanged(int index);
	void on_checkBox_audio_stateChanged(int arg1);
	void setImage(const QImage &image0, const QImage &image1);
	void on_action_record_triggered(bool);
};

#endif // MAINWINDOW_H
