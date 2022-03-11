
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ActionHandler.h"
#include "FrameProcessThread.h"
#include "FrameRateCounter.h"
#include "FullScreenWindow.h"
#include "GlobalData.h"
#include "MySettings.h"
#include "Rational.h"
#include "RecordingDialog.h"
#include "StatusLabel.h"
#include "UIWidget.h"
#include "VideoEncoder.h"
#include "main.h"
#include <QAudioOutput>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QListWidget>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>

qint64 seconds(QTime const &t)
{
	return QTime(0, 0, 0).secsTo(t);
}

enum {
	DisplayModeRole = Qt::UserRole,
	VideoWidthRole,
	VideoHeightRole,
	FieldDominanceRole,
	FrameRateRole,
	DeviceIndexRole,
	InputConnectionRole,
};

static inline void BlockSignals(QWidget *w, std::function<void ()> const &callback)
{
	bool b = w->blockSignals(true);
	callback();
	w->blockSignals(b);
}

// Video input connector map
const QVector<QPair<BMDVideoConnection, QString>> kVideoInputConnections = {
	qMakePair(bmdVideoConnectionSDI,		QString("SDI")),
	qMakePair(bmdVideoConnectionHDMI,		QString("HDMI")),
	qMakePair(bmdVideoConnectionOpticalSDI,	QString("Optical SDI")),
	qMakePair(bmdVideoConnectionComponent,	QString("Component")),
	qMakePair(bmdVideoConnectionComposite,	QString("Composite")),
	qMakePair(bmdVideoConnectionSVideo,		QString("S-Video")),
};

struct MainWindow::Private {
	std::unique_ptr<DeckLinkCapture> video_capture;

	std::vector<std::shared_ptr<DeckLinkInputDevice>> input_devices;

	DeckLinkInputDevice *selected_device = nullptr;
	BMDVideoConnection selected_input_connection = bmdVideoConnectionHDMI;
	DeckLinkDeviceDiscovery *decklink_discovery = nullptr;
	ProfileCallback *profile_callback = nullptr;
	BMDDisplayMode display_mode = bmdModeHD1080i5994;
	BMDFieldDominance field_dominance = bmdUnknownFieldDominance;

	int video_width = 1920;
	int video_height = 1080;
	Rational fps;

	bool valid_signal = false;

	QList<QAudioDeviceInfo> audio_output_devices;
	QAudioFormat audio_format;
	std::shared_ptr<QAudioOutput> audio_output;
	QIODevice *audio_output_device = nullptr;

	std::shared_ptr<VideoEncoder> video_encoder;

	StatusLabel *status_label = nullptr;

	FrameRateCounter frame_rate_counter_;

	QString recording_file_path;
	QDateTime recording_start_time;
	qint64 recording_seconds = 0;

	int timer_count = 0;

	int hide_cursor_count = 0;

	FrameProcessThread frame_process_thread;

	FullScreenWindow *full_screen_window = nullptr;

	bool closing = false;
};


MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

	ui->widget_ui->bindMainWindow(this);

	m->status_label = new StatusLabel(this);
	ui->statusbar->addWidget(m->status_label);

	m->video_capture = std::make_unique<DeckLinkCapture>(this);
	connect(m->video_capture.get(), &DeckLinkCapture::newFrame, this, &MainWindow::newFrame);

	setStatusBarText(QString());

	m->audio_output_devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
	for (QAudioDeviceInfo const &dev : m->audio_output_devices) {
		QString name = dev.deviceName();
		auto a = ui->menu_audio_output_devices->addAction(name);
		new ActionHandler(this, a, name, [&](QString const &name){
			changeAudioOutputDevice(name, true);
		});
	}
	m->audio_format.setByteOrder(QAudioFormat::LittleEndian);
	m->audio_format.setChannelCount(2);
	m->audio_format.setCodec("audio/pcm");
	m->audio_format.setSampleRate(48000);
	m->audio_format.setSampleSize(16);
	m->audio_format.setSampleType(QAudioFormat::SignedInt);

	{
		MySettings s;
		s.beginGroup("Audio");
		QString name = s.value("OutputDevice").toString();
		s.endGroup();

		if (!changeAudioOutputDevice(name, false)) {
			m->audio_output = std::make_shared<QAudioOutput>(m->audio_format);
			m->audio_output_device = m->audio_output->start();
		}
	}


	checkBox_display_mode_auto_detection()->setCheckState(Qt::Checked);
	checkBox_audio()->click();

	connect(new QShortcut(QKeySequence("F11"), this), &QShortcut::activated, [&](){
		setFullScreen(!isFullScreen());
	});

	connect(new QShortcut(QKeySequence("Ctrl+T"), this), &QShortcut::activated, this, &MainWindow::test);

	connect(&m->frame_process_thread, &FrameProcessThread::ready, this, &MainWindow::ready);

	m->frame_process_thread.start();
	m->frame_rate_counter_.start();

	setMouseTracking(true);

	startTimer(100);
}

MainWindow::~MainWindow()
{
	setFullScreen(false);

	m->input_devices.clear();

	if (m->profile_callback) {
		m->profile_callback->Release();
		m->profile_callback = nullptr;
	}

	if (m->decklink_discovery) {
		m->decklink_discovery->Release();
		m->decklink_discovery = nullptr;
	}

	m->frame_rate_counter_.stop();
	m->frame_process_thread.stop();

	delete m;
	delete ui;
}

bool MainWindow::isFullScreen() const
{
	return (bool)m->full_screen_window;
}

void MainWindow::setFullScreen(bool f)
{
	if (f) {
		if (!m->full_screen_window) {
			m->full_screen_window = new FullScreenWindow(this);
		}
		m->full_screen_window->setWindowState(Qt::WindowFullScreen);
		m->full_screen_window->show();
		ui->image_widget->setImage({});
	} else {
		FullScreenWindow *w = nullptr;
		std::swap(w, m->full_screen_window);
		if (w) {
			w->close();
			delete w;
		}
	}
}



QListWidget *MainWindow::listWidget_input_device()
{
	return ui->widget_ui->listWidget_input_device();
}

QListWidget *MainWindow::listWidget_input_connection()
{
	return ui->widget_ui->listWidget_input_connection();
}

QListWidget *MainWindow::listWidget_display_mode()
{
	return ui->widget_ui->listWidget_display_mode();
}

QCheckBox *MainWindow::checkBox_audio()
{
	return ui->widget_ui->checkBox_audio();
}

QCheckBox *MainWindow::checkBox_display_mode_auto_detection()
{
	return ui->widget_ui->checkBox_display_mode_auto_detection();
}

void MainWindow::setStatusBarText(QString const &text)
{
	m->status_label->setText(text);
}

void MainWindow::setup()
{
	// Create and initialise DeckLink device discovery and profile objects
	m->decklink_discovery = new DeckLinkDeviceDiscovery(m->video_capture.get());
	m->profile_callback = new ProfileCallback(m->video_capture.get());

	if ((m->decklink_discovery) && (m->profile_callback)) {
		if (!m->decklink_discovery->enable()) {
			QMessageBox::critical(this, "This application requires the DeckLink drivers installed.", "Please install the Blackmagic DeckLink drivers to use the features of this application.");
			return;
		}
	}
}

void MainWindow::closeEvent(QCloseEvent *)
{
	setFullScreen(false);

	m->closing = true;

	stopRecord();

	if (m->selected_device) {
		stopCapture();

		// Disable profile callback
		if (m->selected_device->getProfileManager()) {
			m->selected_device->getProfileManager()->SetCallback(nullptr);
		}
	}

	m->frame_process_thread.stop();

	// Disable DeckLink device discovery
	m->decklink_discovery->disable();
}

void MainWindow::setSignalStatus(bool valid)
{
	m->valid_signal = valid;
	setStatusBarText(valid ? QString() : tr("No valid input signal"));
}

bool MainWindow::isValidSignal() const
{
	return m->valid_signal;
}

bool MainWindow::isAudioCaptureEnabled() const
{
	return const_cast<MainWindow *>(this)->checkBox_audio()->isChecked();
}

bool MainWindow::isCapturing() const
{
	return m->selected_device && m->selected_device->isCapturing();
}

void MainWindow::updateUI()
{
	ImageWidget::ViewMode vm = ui->image_widget->viewMode();
	ui->action_view_small_lq->setChecked(vm == ImageWidget::ViewMode::SmallLQ);
	ui->action_view_dot_by_dot->setChecked(vm == ImageWidget::ViewMode::DotByDot);
	ui->action_view_fit_window->setChecked(vm == ImageWidget::ViewMode::FitToWindow);
}

void MainWindow::internalStartCapture(bool start)
{
	stopRecord();

	if (isCapturing()) {
		// stop capture
		m->selected_device->stopCapture();
	}
	ui->image_widget->clear();

	m->frame_process_thread.stop();

	if (start) {
		m->frame_process_thread.start();

		// start capture
		auto *item = listWidget_display_mode()->currentItem();
		if (item) {
			m->display_mode = (BMDDisplayMode)item->data(DisplayModeRole).toInt();
			m->field_dominance = (BMDFieldDominance)item->data(FieldDominanceRole).toUInt();
			m->fps = item->data(FrameRateRole).value<Rational>();
		}
		bool auto_detect = isVideoFormatAutoDetectionEnabled();
		m->video_capture->startCapture(m->selected_device, m->display_mode, m->field_dominance, auto_detect, isAudioCaptureEnabled());
	}
	updateUI();
}

void MainWindow::startCapture()
{
	internalStartCapture(true);
}

void MainWindow::stopCapture()
{
	internalStartCapture(false);
}

void MainWindow::restartCapture()
{
	internalStartCapture(isCapturing());
}

void MainWindow::refreshInputConnectionMenu()
{
	BMDVideoConnection supportedConnections;
	int64_t currentInputConnection;

	// Get the available input video connections for the device
	supportedConnections = m->selected_device->getVideoConnections();

	// Get the current selected input connection
	if (m->selected_device->getDeckLinkConfiguration()->GetInt(bmdDeckLinkConfigVideoInputConnection, &currentInputConnection) != S_OK) {
		currentInputConnection = bmdVideoConnectionUnspecified;
	}

	listWidget_input_connection()->clear();

	for (auto &inputConnection : kVideoInputConnections) {
		if (inputConnection.first & supportedConnections) {
			int row = listWidget_input_connection()->currentRow();
			auto *item = new QListWidgetItem(inputConnection.second);
			item->setData(InputConnectionRole, QVariant::fromValue((int64_t)inputConnection.first));
			listWidget_input_connection()->addItem(item);
			if (inputConnection.first == m->selected_input_connection) {
				listWidget_input_connection()->setCurrentRow(row);
			}
		}
	}

	listWidget_input_connection()->sortItems();
}

void MainWindow::refreshDisplayModeMenu()
{
	IDeckLinkDisplayModeIterator *displayModeIterator;
	IDeckLinkDisplayMode *displayMode;
	IDeckLinkInput *deckLinkInput;

	listWidget_display_mode()->clear();

	if (!m->selected_device) return;

	deckLinkInput = m->selected_device->getDeckLinkInput();

	if (deckLinkInput->GetDisplayModeIterator(&displayModeIterator) != S_OK) return;

	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	while (displayModeIterator->Next(&displayMode) == S_OK) {
		BOOL supported = false;
		BMDDisplayMode mode = displayMode->GetDisplayMode();
		BMDFieldDominance fdom = displayMode->GetFieldDominance();

		Rational fps = DeckLinkInputDevice::frameRate(displayMode);

#ifdef _WIN32
		HRESULT rc = deckLinkInput->DoesSupportVideoMode(m->selected_input_connection, mode, bmdFormatUnspecified, bmdSupportedVideoModeDefault, &supported);
#else
		HRESULT rc = deckLinkInput->DoesSupportVideoMode(m->selected_input_connection, mode, bmdFormatUnspecified, bmdNoVideoInputConversion, bmdSupportedVideoModeDefault, nullptr, &supported);
#endif
		if ((rc == S_OK) && supported) {
			QString name;
			{
				DLString modeName;
				if (displayMode->GetName(&modeName) == S_OK && !modeName.empty()) {
					name = modeName;
				}
			}
			if (!name.isEmpty()) {
				int row = listWidget_display_mode()->count();
				auto *item = new QListWidgetItem(name);
				item->setData(DisplayModeRole, QVariant::fromValue((uint64_t)mode));
				item->setData(VideoWidthRole, QVariant::fromValue(displayMode->GetWidth()));
				item->setData(VideoHeightRole, QVariant::fromValue(displayMode->GetHeight()));
				item->setData(FieldDominanceRole, QVariant::fromValue((uint32_t)fdom));
				item->setData(FrameRateRole, QVariant::fromValue(fps));
				listWidget_display_mode()->addItem(item);
				if (mode == m->display_mode) {
					listWidget_display_mode()->setCurrentRow(row);
				}
			}
		}

		displayMode->Release();
		displayMode = nullptr;
	}
	displayModeIterator->Release();
}

void MainWindow::addDevice(IDeckLink *decklink)
{
	std::shared_ptr<DeckLinkInputDevice> newDevice = std::make_shared<DeckLinkInputDevice>(m->video_capture.get(), decklink);
	m->input_devices.push_back(newDevice);

	// Initialise new DeckLinkDevice object
	if (!newDevice->init()) {
		// Device does not have IDeckLinkInput interface, eg it is a DeckLink Mini Monitor
		newDevice->Release();
		return;
	}

//	connect(newDevice.get(), &DeckLinkInputDevice::audio, this, &MainWindow::onPlayAudio);

	auto *item = new QListWidgetItem(newDevice->getDeviceName());
	item->setData(DeviceIndexRole, QVariant::fromValue((void *)newDevice.get()));
	listWidget_input_device()->addItem(item);

	listWidget_input_device()->sortItems();

	ui->image_widget->clear();
	changeInputDevice(0);
}

void MainWindow::removeDevice(IDeckLink *decklink)
{
	int deviceIndex = -1;
	DeckLinkInputDevice *deviceToRemove = nullptr;

	for (deviceIndex = 0; deviceIndex < listWidget_input_device()->count(); deviceIndex++) {
		auto *item = listWidget_input_device()->item(deviceIndex);
		if (item) {
			auto *device = reinterpret_cast<DeckLinkInputDevice *>(item->data(DeviceIndexRole).value<void *>());
			if (device->getDeckLinkInstance() == decklink) {
				deviceToRemove = device;
				break;
			}
		}
	}

	if (!deviceToRemove) return;

	// Remove device from list
	delete listWidget_input_device()->takeItem(deviceIndex);

	// If playback is ongoing, stop it
	if ((m->selected_device == deviceToRemove) && m->selected_device->isCapturing()) {
		m->selected_device->stopCapture();
	}

	// Check how many devices are left
	if (listWidget_input_device()->count() == 0) {
		// We have removed the last device, disable the interface.
		m->selected_device = nullptr;
	} else if (m->selected_device == deviceToRemove) {
		changeInputDevice(0);
	}

	// Release DeckLinkDevice instance
	deviceToRemove->Release();
}

bool MainWindow::isVideoFormatAutoDetectionEnabled() const
{
	return const_cast<MainWindow *>(this)->checkBox_display_mode_auto_detection()->isChecked();
}

void MainWindow::updateProfile(IDeckLinkProfile* /* newProfile */)
{
	changeInputDevice(listWidget_input_device()->currentRow());
}

void MainWindow::changeInputDevice(int selectedDeviceIndex)
{
	if (m->closing) return;
	if (selectedDeviceIndex == -1) return;

	BlockSignals(listWidget_input_device(), [&](){
		listWidget_input_device()->setCurrentRow(selectedDeviceIndex);
	});

	stopCapture();

	// Disable profile callback for previous selected device
	if (m->selected_device && m->selected_device->getProfileManager()) {
		m->selected_device->getProfileManager()->SetCallback(nullptr);
	}

	QVariant selectedDeviceVariant = listWidget_input_device()->item(selectedDeviceIndex)->data(DeviceIndexRole);

	m->selected_device = reinterpret_cast<DeckLinkInputDevice *>(selectedDeviceVariant.value<void *>());

	// Register profile callback with newly selected device's profile manager
	if (m->selected_device) {
		IDeckLinkProfileAttributes *deckLinkAttributes = nullptr;

		if (m->selected_device->getProfileManager()) {
			m->selected_device->getProfileManager()->SetCallback(m->profile_callback);
		}

		// Query duplex mode attribute to check whether sub-device is active
		if (m->selected_device->getDeckLinkInstance()->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&deckLinkAttributes) == S_OK) {
			int64_t duplexMode;

			if ((deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &duplexMode) == S_OK) && (duplexMode != bmdDuplexInactive)) {
				refreshInputConnectionMenu();
			} else {
				listWidget_input_connection()->clear();
				listWidget_display_mode()->clear();
			}

			deckLinkAttributes->Release();
		}
	}

	changeInputConnection(m->selected_input_connection, false);

	changeDisplayMode(m->display_mode, m->fps);
	startCapture();
}

void MainWindow::changeInputConnection(BMDVideoConnection conn, bool errorcheck)
{
	if (m->closing) return;

	for (int i = 0; i < listWidget_input_connection()->count(); i++) {
		auto *item = listWidget_input_connection()->item(i);
		if (item && (BMDVideoConnection)item->data(InputConnectionRole).toLongLong() == conn) {
			BlockSignals(listWidget_input_connection(), [&](){
				listWidget_input_connection()->setCurrentRow(i);
			});
			break;
		}
	}

	m->selected_input_connection = conn;

	if (m->selected_device) {
		HRESULT result = m->selected_device->getDeckLinkConfiguration()->SetInt(bmdDeckLinkConfigVideoInputConnection, (int64_t)m->selected_input_connection);
		if (errorcheck && result != S_OK) {
			QMessageBox::critical(this, "Input connection error", "Unable to set video input connector");
		}

		result = m->selected_device->getDeckLinkConfiguration()->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionEmbedded);
	}

	refreshDisplayModeMenu();
}

void MainWindow::changeDisplayMode(BMDDisplayMode dispmode, Rational const &fps)
{
	for (int i = 0; i < listWidget_display_mode()->count(); i++) {
		auto *item = listWidget_display_mode()->item(i);
		if (item) {
			m->video_width = item->data(VideoWidthRole).toInt();
			m->video_height = item->data(VideoHeightRole).toInt();
			if (m->video_width < 1 || m->video_height < 1) {
				m->video_width = 1920;
				m->video_height = 1080;
			}
			if ((BMDDisplayMode)item->data(DisplayModeRole).toInt() == dispmode) {
				BlockSignals(listWidget_display_mode(), [&](){
					listWidget_display_mode()->setCurrentRow(i);
				});
				m->display_mode = dispmode;
				m->fps = fps;
				restartCapture();
				return;
			}
		}
	}
}

void MainWindow::videoFrameArrived(const AncillaryDataStruct *ancillary_data, const HDRMetadataStruct *hdr_metadata, bool signal_valid)
{
	(void)ancillary_data;
	(void)hdr_metadata;

	setSignalStatus(signal_valid);
}

void MainWindow::haltStreams()
{
	// Profile is changing, stop capture if running
	stopCapture();
	updateUI();
}

void MainWindow::criticalError(const QString &title, const QString &message)
{
	QMessageBox::critical(this, title, message);
}

void MainWindow::on_listWidget_input_device_currentRowChanged(int currentRow)
{
	changeInputDevice(currentRow);
}

void MainWindow::on_listWidget_input_connection_currentRowChanged(int currentRow)
{
	(void)currentRow;
	auto *item = listWidget_input_connection()->currentItem();
	if (item) {
		auto conn = (BMDVideoConnection)item->data(InputConnectionRole).toLongLong();
		changeInputConnection(conn, true);
	}
}

void MainWindow::on_listWidget_display_mode_currentRowChanged(int currentRow)
{
	(void)currentRow;
	QListWidgetItem *item = listWidget_display_mode()->currentItem();
	if (!item) return;
	auto mode = item->data(DisplayModeRole).toUInt();
	auto fps = item->data(FrameRateRole).value<Rational>();
	changeDisplayMode((BMDDisplayMode)mode, fps);
}

void MainWindow::on_listWidget_display_mode_itemDoubleClicked(QListWidgetItem *item)
{
	(void)item;
	restartCapture();
}

void MainWindow::on_checkBox_display_mode_auto_detection_clicked(bool checked)
{
	if (checked) {
		restartCapture();
	}
}

void MainWindow::on_checkBox_audio_stateChanged(int arg1)
{
	(void)arg1;
	restartCapture();
}

void MainWindow::ready(CaptureFrame const &frame)
{
	if (m->audio_output_device) {
		m->audio_output_device->write(frame.audio);
	}
	if (m->full_screen_window) {
		m->full_screen_window->setImage(frame.image_for_view);
	} else {
		ui->image_widget->setImage(frame.image_for_view);
	}
}

void MainWindow::newFrame(CaptureFrame const &frame)
{
	m->frame_rate_counter_.increment();

	QSize size;
	if (m->full_screen_window) {
		size = m->full_screen_window->scaledSize(frame.image);
	} else {
		size = ui->image_widget->scaledSize(frame.image);
	}

	m->frame_process_thread.request(frame, size);

	if (m->video_encoder) {
		m->video_encoder->put_frame(frame);
	}
}

bool MainWindow::isRecording() const
{
	return (bool)m->video_encoder;
}

void MainWindow::stopRecord()
{
	if (isRecording()) {
		if (isRecording()) {
			qDebug() << "stop recording";

			m->video_encoder->close();
			m->video_encoder.reset();
		}

		notifyRecordingProgress(0, 0);

		updateUI();
	}
}

void MainWindow::toggleRecord()
{
	if (isRecording()) {
		stopRecord();
	} else {
		m->recording_start_time = QDateTime::currentDateTime();
		VideoEncoder::VideoOption vopt;
		VideoEncoder::AudioOption aopt;
		vopt.src_w = m->video_width;
		vopt.src_h = m->video_height;
		vopt.fps = m->fps;
		m->video_encoder = std::make_shared<VideoEncoder>();
#ifdef Q_OS_WIN
		m->video_encoder->open(m->recording_file_path.toStdString(), vopt, aopt);
#else
		m->video_encoder->open(m->recording_file_path.toStdString(), vopt, aopt);
#endif
		notifyRecordingProgress(0, m->recording_seconds);
	}
	updateUI();
}

void MainWindow::onInterval1s()
{
	if (!isValidSignal()) {
		switch (m->selected_input_connection) {
		case bmdVideoConnectionSDI:
			changeInputConnection(bmdVideoConnectionHDMI, false);
			break;
		case bmdVideoConnectionHDMI:
			changeInputConnection(bmdVideoConnectionSDI, false);
			break;
		}
	}
}

void MainWindow::notifyRecordingProgress(qint64 current, qint64 length)
{
	ui->image_widget->updateRecordingProgress(current, length);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
	m->timer_count = (m->timer_count + 1) % 10;
	if (m->timer_count == 1) {
		onInterval1s();
	}

	if (m->hide_cursor_count > 0) {
		m->hide_cursor_count--;
		if (m->hide_cursor_count == 0) {
			setCursor(global->invisible_cursor);
		}
	}

	if (isRecording()) {
		auto now = QDateTime::currentDateTime();
		int64_t current = m->recording_start_time.secsTo(now);
		if (m->recording_seconds > 0) {
			if (current >= m->recording_seconds) {
				stopRecord();
				current = m->recording_seconds = 0;
			}
		}
		notifyRecordingProgress(current, m->recording_seconds);
	}
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
	bool v = false;
	auto state = windowState();
	if (state & Qt::WindowFullScreen) {
		state &= ~Qt::WindowFullScreen;
		v = true;
	} else {
		state |= Qt::WindowFullScreen;
		v = false;
	}
	ui->menubar->setVisible(v);
	ui->statusbar->setVisible(v);
	ui->frame_ui->setVisible(v);
	setWindowState(state);
}

void MainWindow::updateCursor()
{
	setCursor(Qt::ArrowCursor);
	m->hide_cursor_count = 10;
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
	updateCursor();
}

bool MainWindow::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverMove:
		updateCursor();
		break;
	}
	return QMainWindow::event(event);
}

bool MainWindow::changeAudioOutputDevice(QString const &name, bool save)
{
	int index = -1;
	for (int i = 0; i < m->audio_output_devices.size(); i++) {
		if (m->audio_output_devices[i].deviceName() == name) {
			index = i;
			break;
		}
	}

	if (index >= 0 && index < m->audio_output_devices.size()) {
		if (m->audio_output) {
			m->audio_output->stop();
			m->audio_output.reset();
		}

		m->audio_output = std::shared_ptr<QAudioOutput>(new QAudioOutput(m->audio_output_devices[index], m->audio_format));
		m->audio_output_device = m->audio_output->start();

		if (save) {
			MySettings s;
			s.beginGroup("Audio");
			s.setValue("OutputDevice", name);
			s.endGroup();
		}

		return true;
	}
	return false;
}

void MainWindow::startRecord()
{
	RecordingDialog dlg(this);
	if (dlg.exec() == QDialog::Accepted) {
		stopRecord();

		m->recording_file_path = dlg.path();

		QTime t = dlg.maximumLength();
		m->recording_seconds = seconds(t);
		toggleRecord();
	}
}

void MainWindow::on_action_view_small_lq_triggered()
{
	ui->image_widget->setViewMode(ImageWidget::ViewMode::SmallLQ);
	updateUI();
}

void MainWindow::on_action_view_dot_by_dot_triggered()
{
	ui->image_widget->setViewMode(ImageWidget::ViewMode::DotByDot);
	updateUI();
}

void MainWindow::on_action_view_fit_window_triggered()
{
	ui->image_widget->setViewMode(ImageWidget::ViewMode::FitToWindow);
	updateUI();
}

void MainWindow::on_action_recording_start_triggered()
{
	startRecord();
}

void MainWindow::on_action_recording_stop_triggered()
{
	stopRecord();
}

void MainWindow::test()
{
	qDebug() << Q_FUNC_INFO;
}

