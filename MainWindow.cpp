
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DeckLinkCapture.h"
#include "StatusLabel.h"
#include <QAudioOutput>
#include <QBuffer>
#include <QDebug>
#include <QMessageBox>
#include <functional>
#ifdef USE_VIDEO_RECORDING
#include "VideoEncoder.h"
#endif

enum {
	DisplayModeRole = Qt::UserRole,
	FieldDominanceRole,
	FrameRateRole,
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

const QVector<QPair<DeinterlaceMode, QString>> deinterlace_mode_list = {
	qMakePair(DeinterlaceMode::None,				QString("None")),
	qMakePair(DeinterlaceMode::InterpolateEven,		QString("Interporate Even")),
	qMakePair(DeinterlaceMode::InterpolateOdd,		QString("Interporate Odd")),
	qMakePair(DeinterlaceMode::Merge,				QString("Merge")),
	qMakePair(DeinterlaceMode::MergeX2,				QString("Merge x2 Frames")),
};

struct MainWindow::Private {
	QAction *a_record = nullptr;

	DeckLinkCapture video_capture;

	DeckLinkInputDevice *selected_device = nullptr;
	BMDVideoConnection selected_input_connection = bmdVideoConnectionHDMI;
	DeckLinkDeviceDiscovery *decklink_discovery = nullptr;
	ProfileCallback *profile_callback = nullptr;
	BMDDisplayMode display_mode = bmdModeHD1080i5994;
	BMDFieldDominance field_dominance = bmdUnknownFieldDominance;
	double fps = 0;

	std::shared_ptr<QAudioOutput> audio_output;
	QIODevice *audio_output_device = nullptr;

#ifdef USE_VIDEO_RECORDING
	std::shared_ptr<VideoEncoder> video_encoder;
#endif

	StatusLabel *status_label = nullptr;

	bool closing = false;
};

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m(new Private)
{
	ui->setupUi(this);

#ifdef USE_VIDEO_RECORDING
	{
		QMenu *menu = new QMenu(tr("Experimental"), this);
		m->a_record = new QAction("Record", this);
		m->a_record->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
		menu->addAction(m->a_record);
		ui->menubar->addMenu(menu);
		connect(m->a_record, SIGNAL(triggered(bool)), this, SLOT(on_action_record_triggered(bool)));
	}
#endif

	m->status_label = new StatusLabel(this);
	ui->statusbar->addWidget(m->status_label);

	int i;
	i = 0;
	for (auto const &pair : deinterlace_mode_list) {
		DeinterlaceMode mode = pair.first;
		QString name = pair.second;
		ui->comboBox_deinterlace->addItem(name, QVariant((int)mode));
		i++;
	}

	connect(&m->video_capture, &DeckLinkCapture::newFrame, this, &MainWindow::setImage);

	setStatusBarText(QString());

	setDeinterlaceMode(DeinterlaceMode::None);

	QAudioFormat format;
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setChannelCount(2);
	format.setCodec("audio/pcm");
	format.setSampleRate(48000);
	format.setSampleSize(16);
	format.setSampleType(QAudioFormat::SignedInt);
	m->audio_output = std::make_shared<QAudioOutput>(format);
	m->audio_output_device = m->audio_output->start();

	ui->checkBox_display_mode_auto_detection->setCheckState(Qt::Checked);
	ui->checkBox_audio->click();
}

MainWindow::~MainWindow()
{
	if (m->profile_callback) {
		m->profile_callback->Release();
		m->profile_callback = nullptr;
	}

	if (m->decklink_discovery) {
		m->decklink_discovery->Release();
		m->decklink_discovery = nullptr;
	}

	delete m;
	delete ui;
}

void MainWindow::setPixelFormat(BMDPixelFormat pixel_format)
{
	m->video_capture.setPixelFormat(pixel_format);
}

void MainWindow::setStatusBarText(QString const &text)
{
	m->status_label->setText(text);
}

void MainWindow::setup()
{
	// Create and initialise DeckLink device discovery and profile objects
	m->decklink_discovery = new DeckLinkDeviceDiscovery(this);
	m->profile_callback = new ProfileCallback(this);

	if ((m->decklink_discovery) && (m->profile_callback)) {
		if (!m->decklink_discovery->enable()) {
			QMessageBox::critical(this, "This application requires the DeckLink drivers installed.", "Please install the Blackmagic DeckLink drivers to use the features of this application.");
		}
	}
}

void MainWindow::customEvent(QEvent *event)
{
	if (event->type() == kAddDeviceEvent) {
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		addDevice(discoveryEvent->decklink());
	} else if (event->type() == kRemoveDeviceEvent) {
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		removeDevice(discoveryEvent->decklink());
	} else if (event->type() == kVideoFormatChangedEvent) {
		DeckLinkInputFormatChangedEvent* formatEvent = dynamic_cast<DeckLinkInputFormatChangedEvent*>(event);
		changeDisplayMode(formatEvent->DisplayMode(), formatEvent->fps());
	} else if (event->type() == kVideoFrameArrivedEvent) {
		DeckLinkInputFrameArrivedEvent* frameArrivedEvent = dynamic_cast<DeckLinkInputFrameArrivedEvent*>(event);
		setStatusBarText(frameArrivedEvent->SignalValid() ? QString() : tr("No valid input signal"));
		delete frameArrivedEvent->AncillaryData();
		delete frameArrivedEvent->HDRMetadata();
	} else if (event->type() == kProfileActivatedEvent) {
		DeckLinkProfileCallbackEvent* profileChangedEvent = dynamic_cast<DeckLinkProfileCallbackEvent*>(event);
		updateProfile(profileChangedEvent->Profile());
	}
}

void MainWindow::closeEvent(QCloseEvent *)
{
	m->closing = true;

	if (m->selected_device) {
		stopCapture();

		// Disable profile callback
		if (m->selected_device->getProfileManager()) {
			m->selected_device->getProfileManager()->SetCallback(nullptr);
		}
	}

	// Disable DeckLink device discovery
	m->decklink_discovery->disable();
}

bool MainWindow::isAudioCaptureEnabled() const
{
	return ui->checkBox_audio->isChecked();
}

bool MainWindow::isCapturing() const
{
	return m->selected_device && m->selected_device->isCapturing();
}

void MainWindow::updateUI()
{
	if (isCapturing()) {
		ui->pushButton_start->setText("Stop");
	} else {
		setStatusBarText(QString());
		ui->pushButton_start->setText("Start");
	}
}

void MainWindow::internalStartCapture(bool start)
{
	stopRecord();
	if (isCapturing()) {
		// stop capture
		m->selected_device->stopCapture();
		m->video_capture.stop();
	}
	ui->widget_image->clear();
	if (start) {
		// start capture
		auto *item = ui->listWidget_display_mode->currentItem();
		if (item) {
			m->display_mode = (BMDDisplayMode)item->data(DisplayModeRole).toInt();
			m->field_dominance = (BMDFieldDominance)item->data(FieldDominanceRole).toUInt();
			m->fps = item->data(FrameRateRole).toDouble();
		}
		bool auto_detect = isVideoFormatAutoDetectionEnabled();
		m->video_capture.start(m->selected_device, m->display_mode, m->field_dominance, auto_detect, isAudioCaptureEnabled());
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

void MainWindow::toggleCapture()
{
	internalStartCapture(!isCapturing());
}

void MainWindow::refreshInputConnectionMenu(void)
{
	BMDVideoConnection supportedConnections;
	int64_t currentInputConnection;

	// Get the available input video connections for the device
	supportedConnections = m->selected_device->getVideoConnections();

	// Get the current selected input connection
	if (m->selected_device->getDeckLinkConfiguration()->GetInt(bmdDeckLinkConfigVideoInputConnection, &currentInputConnection) != S_OK) {
		currentInputConnection = bmdVideoConnectionUnspecified;
	}

	ui->listWidget_input_connection->clear();

	for (auto &inputConnection : kVideoInputConnections) {
		if (inputConnection.first & supportedConnections) {
			int row = ui->listWidget_input_connection->currentRow();
			auto *item = new QListWidgetItem(inputConnection.second);
			item->setData(Qt::UserRole, QVariant::fromValue((int64_t)inputConnection.first));
			ui->listWidget_input_connection->addItem(item);
			if (inputConnection.first == m->selected_input_connection) {
				ui->listWidget_input_connection->setCurrentRow(row);
			}
		}
	}

	ui->listWidget_input_connection->sortItems();
}

void MainWindow::refreshDisplayModeMenu(void)
{
	IDeckLinkDisplayModeIterator *displayModeIterator;
	IDeckLinkDisplayMode *displayMode;
	IDeckLinkInput *deckLinkInput;

	ui->listWidget_display_mode->clear();

	deckLinkInput = m->selected_device->getDeckLinkInput();

	if (deckLinkInput->GetDisplayModeIterator(&displayModeIterator) != S_OK) return;

	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	while (displayModeIterator->Next(&displayMode) == S_OK) {
		BOOL supported = false;
		BMDDisplayMode mode = displayMode->GetDisplayMode();
		BMDFieldDominance fdom = displayMode->GetFieldDominance();

		double fps = DeckLinkInputDevice::frameRate(displayMode);

		if ((deckLinkInput->DoesSupportVideoMode(m->selected_input_connection, mode, bmdFormatUnspecified, bmdSupportedVideoModeDefault, &supported) == S_OK) && supported) {
			QString name;
			{
				DLString modeName;
				if (displayMode->GetName(&modeName) == S_OK && !modeName.empty()) {
					name = modeName;
				}
			}
			if (!name.isEmpty()) {
				int row = ui->listWidget_display_mode->count();
				auto *item = new QListWidgetItem(name);
				item->setData(DisplayModeRole, QVariant::fromValue((uint64_t)mode));
				item->setData(FieldDominanceRole, QVariant::fromValue((uint32_t)fdom));
				item->setData(FrameRateRole, QVariant::fromValue(fps));
				ui->listWidget_display_mode->addItem(item);
				if (mode == m->display_mode) {
					ui->listWidget_display_mode->setCurrentRow(row);
				}
			}
		}

		displayMode->Release();
		displayMode = nullptr;
	}
	displayModeIterator->Release();

	ui->pushButton_start->setEnabled(ui->listWidget_display_mode->count() != 0);
}

void MainWindow::addDevice(IDeckLink *decklink)
{
	DeckLinkInputDevice *newDevice = new DeckLinkInputDevice(this, decklink);

	// Initialise new DeckLinkDevice object
	if (!newDevice->init()) {
		// Device does not have IDeckLinkInput interface, eg it is a DeckLink Mini Monitor
		newDevice->Release();
		return;
	}

	connect(newDevice, &DeckLinkInputDevice::audio, this, &MainWindow::onPlayAudio);

	auto *item = new QListWidgetItem(newDevice->getDeviceName());
	item->setData(Qt::UserRole, QVariant::fromValue((void *)newDevice));
	ui->listWidget_input_device->addItem(item);

	ui->listWidget_input_device->sortItems();

	ui->widget_image->setImage(QImage(), QImage());
	ui->pushButton_start->setText("Start");
	changeInputDevice(0);
}

void MainWindow::removeDevice(IDeckLink* deckLink)
{
	int deviceIndex = -1;
	DeckLinkInputDevice *deviceToRemove = nullptr;

	for (deviceIndex = 0; deviceIndex < ui->listWidget_input_device->count(); deviceIndex++) {
		auto *item = ui->listWidget_input_device->item(deviceIndex);
		if (item) {
			auto *device = reinterpret_cast<DeckLinkInputDevice *>(item->data(Qt::UserRole).value<void *>());
			if (device->getDeckLinkInstance() == deckLink) {
				deviceToRemove = device;
				break;
			}
		}
	}

	if (!deviceToRemove) return;

	// Remove device from list
	delete ui->listWidget_input_device->takeItem(deviceIndex);

	// If playback is ongoing, stop it
	if ((m->selected_device == deviceToRemove) && m->selected_device->isCapturing()) {
		m->selected_device->stopCapture();
	}

	// Check how many devices are left
	if (ui->listWidget_input_device->count() == 0) {
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
	return ui->checkBox_display_mode_auto_detection->isChecked();
}



void MainWindow::haltStreams(void)
{
	// Profile is changing, stop capture if running
	stopCapture();
	updateUI();
}

void MainWindow::updateProfile(IDeckLinkProfile* /* newProfile */)
{
	changeInputDevice(ui->listWidget_input_device->currentRow());
}

void MainWindow::changeInputDevice(int selectedDeviceIndex)
{
	if (m->closing) return;
	if (selectedDeviceIndex == -1) return;

	BlockSignals(ui->listWidget_input_device, [&](){
		ui->listWidget_input_device->setCurrentRow(selectedDeviceIndex);
	});

	bool capturing = isCapturing();
	stopCapture();

	// Disable profile callback for previous selected device
	if (m->selected_device && m->selected_device->getProfileManager()) {
		m->selected_device->getProfileManager()->SetCallback(nullptr);
	}

	QVariant selectedDeviceVariant = ui->listWidget_input_device->item(selectedDeviceIndex)->data(Qt::UserRole);

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
				ui->listWidget_input_connection->clear();
				ui->listWidget_display_mode->clear();
				ui->pushButton_start->setEnabled(false);
			}

			deckLinkAttributes->Release();
		}
	}

	changeInputConnection(m->selected_input_connection, false);

	if (capturing) {
		changeDisplayMode(m->display_mode, m->fps);
		startCapture();
	}
}

void MainWindow::changeInputConnection(BMDVideoConnection conn, bool errorcheck)
{
	if (m->closing) return;

	for (int i = 0; i < ui->listWidget_input_connection->count(); i++) {
		auto *item = ui->listWidget_input_connection->item(i);
		if (item && (BMDVideoConnection)item->data(Qt::UserRole).toLongLong() == conn) {
			BlockSignals(ui->listWidget_input_connection, [&](){
				ui->listWidget_input_connection->setCurrentRow(i);
			});
			break;
		}
	}

	m->selected_input_connection = conn;

	HRESULT result = m->selected_device->getDeckLinkConfiguration()->SetInt(bmdDeckLinkConfigVideoInputConnection, (int64_t)m->selected_input_connection);
	if (errorcheck && result != S_OK) {
		QMessageBox::critical(this, "Input connection error", "Unable to set video input connector");
	}

	result = m->selected_device->getDeckLinkConfiguration()->SetInt(bmdDeckLinkConfigAudioInputConnection, bmdAudioConnectionEmbedded);

	refreshDisplayModeMenu();
}

void MainWindow::changeDisplayMode(BMDDisplayMode dispmode, double fps)
{
	for (int i = 0; i < ui->listWidget_display_mode->count(); i++) {
		auto *item = ui->listWidget_display_mode->item(i);
		if (item) {
			if ((BMDDisplayMode)item->data(Qt::UserRole).toInt() == dispmode) {
				BlockSignals(ui->listWidget_display_mode, [&](){
					ui->listWidget_display_mode->setCurrentRow(i);
				});
				m->display_mode = dispmode;
				m->fps = fps;
				restartCapture();
				return;
			}
		}
	}
}

void MainWindow::on_listWidget_input_device_currentRowChanged(int currentRow)
{
	changeInputDevice(currentRow);
}

void MainWindow::on_listWidget_input_connection_currentRowChanged(int currentRow)
{
	(void)currentRow;
	auto *item = ui->listWidget_input_connection->currentItem();
	if (item) {
		auto conn = (BMDVideoConnection)item->data(Qt::UserRole).toLongLong();
		changeInputConnection(conn, true);
	}
}

void MainWindow::on_listWidget_display_mode_currentRowChanged(int currentRow)
{
	(void)currentRow;
	QListWidgetItem *item = ui->listWidget_display_mode->currentItem();
	if (!item) return;
	auto mode = item->data(DisplayModeRole).toUInt();
	auto fps = item->data(FrameRateRole).toDouble();
	changeDisplayMode((BMDDisplayMode)mode, fps);
}

void MainWindow::on_listWidget_display_mode_itemDoubleClicked(QListWidgetItem *item)
{
	(void)item;
	restartCapture();
}

void MainWindow::on_pushButton_start_clicked()
{
	toggleCapture();
}

void MainWindow::on_checkBox_display_mode_auto_detection_clicked(bool checked)
{
	if (checked) {
		restartCapture();
	}
}

void MainWindow::setDeinterlaceMode(DeinterlaceMode mode)
{
	for (int i = 0; i < ui->comboBox_deinterlace->count(); i++) {
		DeinterlaceMode v = (DeinterlaceMode)ui->comboBox_deinterlace->itemData(i).toInt();
		if (v == mode) {
			BlockSignals(ui->comboBox_deinterlace, [&](){
				ui->comboBox_deinterlace->setCurrentIndex(i);
			});
			m->video_capture.setDeinterlaceMode(mode);
			break;
		}
	}
}

void MainWindow::onPlayAudio(const QByteArray &samples)
{
	if (m->audio_output_device) {
		m->audio_output_device->write(samples);
	}
#ifdef USE_VIDEO_RECORDING
	if (m->video_encoder) {
		m->video_encoder->putAudioFrame(samples);
	}
#endif
}

void MainWindow::on_comboBox_deinterlace_currentIndexChanged(int index)
{
	DeinterlaceMode mode = (DeinterlaceMode)ui->comboBox_deinterlace->itemData(index).toInt();
	setDeinterlaceMode(mode);
}

void MainWindow::on_checkBox_audio_stateChanged(int arg1)
{
	(void)arg1;
	restartCapture();
}

void MainWindow::setImage(const QImage &image0, const QImage &image1)
{
	ui->widget_image->setImage(image0, image1);

#ifdef USE_VIDEO_RECORDING
	if (m->video_encoder) {
		m->video_encoder->putVideoFrame(image0);
		if (m->video_capture.deinterlaceMode() == DeinterlaceMode::MergeX2) {
			m->video_encoder->putVideoFrame(image1);
		}
	}
#endif
}

void MainWindow::stopRecord()
{
#ifdef USE_VIDEO_RECORDING
	if (m->video_encoder) {
		m->video_encoder->thread_stop();
		m->video_encoder.reset();
	}
#endif
}

void MainWindow::toggleRecord()
{
#ifdef USE_VIDEO_RECORDING
	if (m->video_encoder) {
		stopRecord();
	} else {
		VideoEncoder::VideoOption vopt;
		vopt.fps = m->fps;
		if (m->video_capture.deinterlaceMode() == DeinterlaceMode::MergeX2) {
			vopt.fps *= 2;
		}
		VideoEncoder::AudioOption aopt;
		m->video_encoder = std::make_shared<VideoEncoder>();
		m->video_encoder->thread_start("a.avi", vopt, aopt);
	}
#endif
}

void MainWindow::on_action_record_triggered(bool)
{
	toggleRecord();
}
