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

class QListWidget;
class QListWidgetItem;
class QCheckBox;

class MainWindow : public QMainWindow, public DeckLinkCaptureDelegate {
	Q_OBJECT
	friend class OverlayWindow;
public:
	enum {
		Dummy_ = QEvent::User,

	};
private:
	Ui::MainWindow *ui;

	struct Private;
	Private *m;

	QListWidget *listWidget_input_device();
	QListWidget *listWidget_input_connection();
	QListWidget *listWidget_display_mode();
	QCheckBox *checkBox_audio();
	QCheckBox *checkBox_display_mode_auto_detection();

	void setStatusBarText(const QString &text);
	bool isVideoFormatAutoDetectionEnabled() const;
	void updateUI();
	void internalStartCapture(bool start);
	void toggleRecord();
	void setSignalStatus(bool valid);
	bool isRecording() const;
	bool isValidSignal() const;
	void onInterval1s();
	void notifyRecordingProgress(qint64 current, qint64 length);
	void updateCursor();
protected:
	void timerEvent(QTimerEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	bool event(QEvent *event);
public:
	explicit MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

	void closeEvent(QCloseEvent *event);

	void setup();

	void startCapture();
	void stopCapture();
	void restartCapture();

	void refreshDisplayModeMenu();
	void refreshInputConnectionMenu();

	void addDevice(IDeckLink *decklink) override;
	void removeDevice(IDeckLink *decklink) override;
	void haltStreams() override;
	void videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid) override;
	void updateProfile(IDeckLinkProfile* newProfile) override;
	void criticalError(const QString &title, const QString &message) override;
	void changeDisplayMode(BMDDisplayMode dispmode, double fps) override;

	bool isCapturing() const;

	void changeInputDevice(int selectedDeviceIndex);
	void changeInputConnection(BMDVideoConnection conn, bool errorcheck);
	bool isAudioCaptureEnabled() const;
	void stopRecord();
	void show();

	void startRecord();
private slots:
	void newFrame(VideoFrame const &frame);
	void onPlayAudio(QByteArray const &samples);
	void on_action_recording_start_triggered();
	void on_action_recording_stop_triggered();
	void on_action_view_small_lq_triggered();
	void on_action_view_dot_by_dot_triggered();
	void on_action_view_fit_window_triggered();
	void on_checkBox_audio_stateChanged(int arg1);
	void on_checkBox_display_mode_auto_detection_clicked(bool checked);
	void on_listWidget_display_mode_currentRowChanged(int currentRow);
	void on_listWidget_display_mode_itemDoubleClicked(QListWidgetItem *item);
	void on_listWidget_input_connection_currentRowChanged(int currentRow);
	void on_listWidget_input_device_currentRowChanged(int currentRow);
	void test();
};

#endif // MAINWINDOW_H
