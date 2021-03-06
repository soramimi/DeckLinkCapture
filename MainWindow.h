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

class MainWindow : public QMainWindow, public DeckLinkCaptureDelegate {
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
	void setSignalStatus(bool valid);
public:
	explicit MainWindow(QWidget *parent = 0);
	virtual ~MainWindow();

	void closeEvent(QCloseEvent *event);

	void setup();

	void startCapture();
	void stopCapture();
	void restartCapture();
	void toggleCapture();

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
	void newFrame();
	void on_action_record_triggered(bool);
	void test();
};

#endif // MAINWINDOW_H
