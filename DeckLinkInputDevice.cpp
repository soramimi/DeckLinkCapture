/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
** 
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#include "AncillaryDataTable.h"
#include "DeckLinkCapture.h"
#include "DeckLinkInputDevice.h"
#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>
#include <QTextStream>
#include "common.h"
#include <omp.h>

struct DeckLinkInputDevice::Private {
	QAtomicInt refcount = 1;

	DeckLinkCapture *capture = nullptr;

	QString device_name;
	IDeckLink *decklink = nullptr;
	IDeckLinkInput *decklink_input = nullptr;
	IDeckLinkConfiguration *decklink_config = nullptr;
	IDeckLinkHDMIInputEDID *decklink_hdmi_input_edid = nullptr;
	IDeckLinkProfileManager *decklink_profile_manager = nullptr;

	BOOL supports_format_detection = false;
	bool currently_capturing = false;
	bool apply_detected_input_mode = false;
	int64_t supported_input_connections = 0;
};

DeckLinkInputDevice::DeckLinkInputDevice(DeckLinkCapture *capture, IDeckLink *device)
	: m(new Private)
{
	m->capture = capture;
	m->decklink = device;
	m->decklink->AddRef();
}

DeckLinkInputDevice::~DeckLinkInputDevice()
{
	if (m->decklink_hdmi_input_edid) {
		m->decklink_hdmi_input_edid->Release();
		m->decklink_hdmi_input_edid = nullptr;
	}

	if (m->decklink_profile_manager) {
		m->decklink_profile_manager->Release();
		m->decklink_profile_manager = nullptr;
	}

	if (m->decklink_input) {
		m->decklink_input->Release();
		m->decklink_input = nullptr;
	}

	if (m->decklink_config) {
		m->decklink_config->Release();
		m->decklink_config = nullptr;
	}

	if (m->decklink) {
		m->decklink->Release();
		m->decklink = nullptr;
	}

	delete m;
}



HRESULT	DeckLinkInputDevice::QueryInterface(REFIID iid, void **ppv)
{
	CFUUIDBytes iunknown;
	HRESULT result = E_NOINTERFACE;

	if (!ppv)
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = nullptr;

	// Obtain the IUnknown interface and compare it the provided REFIID
#ifdef Q_OS_WIN
	iunknown = IID_IUnknown;
#else
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
#endif
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0) {
		*ppv = this;
		AddRef();
		result = S_OK;
	} else if (memcmp(&iid, &IID_IDeckLinkInputCallback, sizeof(REFIID)) == 0) {
		*ppv = (IDeckLinkInputCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG DeckLinkInputDevice::AddRef(void)
{
	return (ULONG)m->refcount.fetchAndAddAcquire(1);
}

ULONG DeckLinkInputDevice::Release(void)
{
	ULONG newRefValue = m->refcount.fetchAndAddAcquire(-1);
	
	if (newRefValue == 0) {
		delete this;
		return 0;
	}

	return newRefValue;
}

bool DeckLinkInputDevice::init()
{
	HRESULT result;
	IDeckLinkProfileAttributes *deckLinkAttributes = nullptr;
	DLString deviceNameStr;

	// Get input interface
	result = m->decklink->QueryInterface(IID_IDeckLinkInput, (void**)&m->decklink_input);
	if (result != S_OK) {
		// This may occur if device does not have input interface, for instance DeckLink Mini Monitor.
		return false;
	}
		
	// Get configuration interface so we can change input connector
	// We hold onto IDeckLinkConfiguration until destructor to retain input connector setting
	result = m->decklink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&m->decklink_config);
	if (result != S_OK) {
		m->capture->criticalError("DeckLink Input initialization error", "Unable to query IDeckLinkConfiguration object interface");
		return false;
	}

	// Get attributes interface
	result = m->decklink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**) &deckLinkAttributes);
	if (result != S_OK) {
		m->capture->criticalError("DeckLink Input initialization error", "Unable to query IDeckLinkProfileAttributes object interface");
		return false;
	}

	// Check if input mode detection is supported.
	if (deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &m->supports_format_detection) != S_OK) {
		m->supports_format_detection = false;
	}

	// Get the supported input connections for the device
	if (deckLinkAttributes->GetInt(BMDDeckLinkVideoInputConnections, &m->supported_input_connections) != S_OK) {
		m->supported_input_connections = 0;
	}
		
	deckLinkAttributes->Release();

	// Enable all EDID functionality if possible
	if (m->decklink->QueryInterface(IID_IDeckLinkHDMIInputEDID, (void **)&m->decklink_hdmi_input_edid) == S_OK && m->decklink_hdmi_input_edid) {
		int64_t allKnownRanges = bmdDynamicRangeSDR | bmdDynamicRangeHDRStaticPQ | bmdDynamicRangeHDRStaticHLG;
		m->decklink_hdmi_input_edid->SetInt(bmdDeckLinkHDMIInputEDIDDynamicRange, allKnownRanges);
		m->decklink_hdmi_input_edid->WriteToEDID();
	}

	// Get device name
	result = m->decklink->GetDisplayName(&deviceNameStr);
	if (result == S_OK) {
		m->device_name = deviceNameStr;
	} else {
		m->device_name = "DeckLink";
	}

	// Get the profile manager interface
	// Will return S_OK when the device has > 1 profiles
	if (m->decklink->QueryInterface(IID_IDeckLinkProfileManager, (void**) &m->decklink_profile_manager) != S_OK) {
		m->decklink_profile_manager = nullptr;
	}

	return true;
}

const QString &DeckLinkInputDevice::getDeviceName() const
{
	return m->device_name;
}

bool DeckLinkInputDevice::isCapturing() const
{
	return m->currently_capturing;
}

bool DeckLinkInputDevice::supportsFormatDetection() const
{
	return m->supports_format_detection;
}

BMDVideoConnection DeckLinkInputDevice::getVideoConnections() const
{
	return (BMDVideoConnection)m->supported_input_connections;
}

bool DeckLinkInputDevice::startCapture(BMDDisplayMode displayMode, IDeckLinkScreenPreviewCallback *screenPreviewCallback, bool applyDetectedInputMode, bool input_audio)
{
	HRESULT result;
	BMDVideoInputFlags video_input_flags = bmdVideoInputFlagDefault;

	m->apply_detected_input_mode = applyDetectedInputMode;

	// Enable input video mode detection if the device supports it
	if (m->supports_format_detection) {
		video_input_flags |=  bmdVideoInputEnableFormatDetection;
	}

	// Set the screen preview
	m->decklink_input->SetScreenPreviewCallback(screenPreviewCallback);

	// Set capture callback
	m->decklink_input->SetCallback(this);

	// Set the video input mode
	result = m->decklink_input->EnableVideoInput(displayMode, bmdFormat8BitYUV, video_input_flags);
	if (result != S_OK) {
		m->capture->criticalError("Error starting the capture", "This application was unable to select the chosen video mode. Perhaps, the selected device is currently in-use.");
		return false;
	}

	if (input_audio) {
		result = m->decklink_input->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2);
	} else {
		result = m->decklink_input->DisableAudioInput();
	}

	// Start the capture
	result = m->decklink_input->StartStreams();
	if (result != S_OK) {
		m->capture->criticalError("Error starting the capture", "This application was unable to start the capture. Perhaps, the selected device is currently in-use.");
		return false;
	}

	m->currently_capturing = true;

	return true;
}

void DeckLinkInputDevice::stopCapture()
{
	if (m->decklink_input) {
		// Stop the capture
		m->decklink_input->StopStreams();

		//
		m->decklink_input->SetScreenPreviewCallback(nullptr);

		// Delete capture callback
		m->decklink_input->SetCallback(nullptr);
	}

	m->currently_capturing = false;
}

IDeckLink *DeckLinkInputDevice::getDeckLinkInstance()
{
	return m->decklink;
}

IDeckLinkInput *DeckLinkInputDevice::getDeckLinkInput()
{
	return m->decklink_input;
}

IDeckLinkConfiguration *DeckLinkInputDevice::getDeckLinkConfiguration()
{
	return m->decklink_config;
}

IDeckLinkProfileManager *DeckLinkInputDevice::getProfileManager()
{
	return m->decklink_profile_manager;
}

Rational DeckLinkInputDevice::frameRate(IDeckLinkDisplayMode *mode)
{
	BMDTimeValue duration = 0;
	BMDTimeScale scale = 0;
	mode->GetFrameRate(&duration, &scale);
	return {scale, duration};
}

HRESULT DeckLinkInputDevice::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	HRESULT result;
	BMDPixelFormat pixel_format = bmdFormat8BitYUV;

	// Unexpected callback when auto-detect mode not enabled
	if (!m->apply_detected_input_mode) {
		return E_FAIL;
	}

	if (detectedSignalFlags & bmdDetectedVideoInputRGB444) {
		pixel_format = bmdFormat10BitRGB;
	}

	m->capture->setPixelFormat(pixel_format);

	// Stop the capture
	m->decklink_input->StopStreams();

	// Set the video input mode
	result = m->decklink_input->EnableVideoInput(newMode->GetDisplayMode(), pixel_format, bmdVideoInputEnableFormatDetection);
	if (result != S_OK) {
		m->capture->criticalError("Error restarting the capture", "This application was unable to set new display mode");
		return result;
	}

	Rational fps = frameRate(newMode);

	// Start the capture
	result = m->decklink_input->StartStreams();
	if (result != S_OK) {
		m->capture->criticalError("Error restarting the capture", "This application was unable to restart capture");
		return result;
	}

	// Notify UI of new display mode
	if (notificationEvents & bmdVideoInputDisplayModeChanged) {
		QCoreApplication::postEvent(m->capture, new DeckLinkInputFormatChangedEvent(newMode->GetDisplayMode(), fps));
	}

	return S_OK;
}

HRESULT DeckLinkInputDevice::VideoInputFrameArrived(IDeckLinkVideoInputFrame *videoFrame, IDeckLinkAudioInputPacket *audioPacket)
{
	if (!videoFrame) return S_OK;

	bool validFrame = (videoFrame->GetFlags() & bmdFrameHasNoInputSource) == 0;

	if (m->capture) {
		DeckLinkInputFrameArrivedEvent *e = new DeckLinkInputFrameArrivedEvent(validFrame);

		// Get the various timecodes and userbits attached to this frame

		getAncillaryDataFromFrame(videoFrame, bmdTimecodeVITC,					&e->ancillary_data_.vitcF1Timecode,		&e->ancillary_data_.vitcF1UserBits);
		getAncillaryDataFromFrame(videoFrame, bmdTimecodeVITCField2,			&e->ancillary_data_.vitcF2Timecode,		&e->ancillary_data_.vitcF2UserBits);
		getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188VITC1,			&e->ancillary_data_.rp188vitc1Timecode,	&e->ancillary_data_.rp188vitc1UserBits);
		getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188VITC2,			&e->ancillary_data_.rp188vitc2Timecode,	&e->ancillary_data_.rp188vitc2UserBits);
		getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188LTC,				&e->ancillary_data_.rp188ltcTimecode,	&e->ancillary_data_.rp188ltcUserBits);
		getAncillaryDataFromFrame(videoFrame, bmdTimecodeRP188HighFrameRate,	&e->ancillary_data_.rp188hfrtcTimecode,	&e->ancillary_data_.rp188hfrtcUserBits);

		getHDRMetadataFromFrame(videoFrame, &e->hdr_metadata_);

		QCoreApplication::postEvent(m->capture, e);
	}

	CaptureFrame t;

	if (audioPacket) {
		const int channels = 2;
		const int frames = audioPacket->GetSampleFrameCount();
		const int bytes = frames * sizeof(int16_t) * channels;
		void *data = nullptr;
		audioPacket->GetBytes(&data);
		if (data && bytes > 0) {
			t.audio = QByteArray((char const *)data, bytes);
		}
	}

	if (videoFrame) {
		int w = videoFrame->GetWidth();
		int h = videoFrame->GetHeight();
		int stride = videoFrame->GetRowBytes();
		uint8_t const *bytes = nullptr;
		if (w > 0 && h > 0 && videoFrame->GetBytes((void **)&bytes) == S_OK && bytes) {
			t.image = DeckLinkCapture::createImage(w, h, m->capture->pixelFormat(), bytes, stride * h);
		}
	}

	emit m->capture->newFrame(t);

	return S_OK;
}

void DeckLinkInputDevice::getAncillaryDataFromFrame(IDeckLinkVideoInputFrame *videoFrame, BMDTimecodeFormat timecodeFormat, QString *timecodeString, QString *userBitsString)
{
	IDeckLinkTimecode *timecode	= nullptr;
	DLString timecodeStr;
	BMDTimecodeUserBits userBits = 0;

	if (videoFrame && timecodeString && userBitsString && videoFrame->GetTimecode(timecodeFormat, &timecode) == S_OK) {
		if (timecode->GetString(&timecodeStr) == S_OK) {
			*timecodeString = timecodeStr;
		} else {
			*timecodeString = "";
		}

		timecode->GetTimecodeUserBits(&userBits);
		*userBitsString = QString("0x%1").arg(userBits, 8, 16, QChar('0'));

		timecode->Release();
	} else {
		*timecodeString = "";
		*userBitsString = "";
	}
}

void DeckLinkInputDevice::getHDRMetadataFromFrame(IDeckLinkVideoInputFrame* videoFrame, HDRMetadataStruct* hdrMetadata)
{
	hdrMetadata->electroOpticalTransferFunction = "";
	hdrMetadata->displayPrimariesRedX = "";
	hdrMetadata->displayPrimariesRedY = "";
	hdrMetadata->displayPrimariesGreenX = "";
	hdrMetadata->displayPrimariesGreenY = "";
	hdrMetadata->displayPrimariesBlueX = "";
	hdrMetadata->displayPrimariesBlueY = "";
	hdrMetadata->whitePointX = "";
	hdrMetadata->whitePointY = "";
	hdrMetadata->maxDisplayMasteringLuminance = "";
	hdrMetadata->minDisplayMasteringLuminance = "";
	hdrMetadata->maximumContentLightLevel = "";
	hdrMetadata->maximumFrameAverageLightLevel = "";
	hdrMetadata->colorspace = "";

	if (videoFrame->GetFlags() & bmdFrameContainsHDRMetadata) {
		IDeckLinkVideoFrameMetadataExtensions* metadataExtensions = nullptr;
		if (videoFrame->QueryInterface(IID_IDeckLinkVideoFrameMetadataExtensions, (void**)&metadataExtensions) == S_OK) {
			double doubleValue = 0.0;
			int64_t intValue = 0;

			if (metadataExtensions->GetInt(bmdDeckLinkFrameMetadataHDRElectroOpticalTransferFunc, &intValue) == S_OK) {
				switch (intValue) {
				case 0:
					hdrMetadata->electroOpticalTransferFunction = "SDR";
					break;
				case 1:
					hdrMetadata->electroOpticalTransferFunction = "HDR";
					break;
				case 2:
					hdrMetadata->electroOpticalTransferFunction = "PQ (ST2084)";
					break;
				case 3:
					hdrMetadata->electroOpticalTransferFunction = "HLG";
					break;
				default:
					hdrMetadata->electroOpticalTransferFunction = QString("Unknown EOTF: %1").arg((int32_t)intValue);
					break;
				}
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesRedX, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesRedX = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesRedY, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesRedY = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesGreenX, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesGreenX = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesGreenY, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesGreenY = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesBlueX, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesBlueX = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRDisplayPrimariesBlueY, &doubleValue) == S_OK) {
				hdrMetadata->displayPrimariesBlueY = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRWhitePointX, &doubleValue) == S_OK) {
				hdrMetadata->whitePointX = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRWhitePointY, &doubleValue) == S_OK) {
				hdrMetadata->whitePointY = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRMaxDisplayMasteringLuminance, &doubleValue) == S_OK) {
				hdrMetadata->maxDisplayMasteringLuminance = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRMinDisplayMasteringLuminance, &doubleValue) == S_OK) {
				hdrMetadata->minDisplayMasteringLuminance = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRMaximumContentLightLevel, &doubleValue) == S_OK) {
				hdrMetadata->maximumContentLightLevel = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetFloat(bmdDeckLinkFrameMetadataHDRMaximumFrameAverageLightLevel, &doubleValue) == S_OK) {
				hdrMetadata->maximumFrameAverageLightLevel = QString::number(doubleValue, 'f', 4);
			}

			if (metadataExtensions->GetInt(bmdDeckLinkFrameMetadataColorspace, &intValue) == S_OK) {
				switch (intValue) {
				case bmdColorspaceRec601:
					hdrMetadata->colorspace = "Rec.601";
					break;
				case bmdColorspaceRec709:
					hdrMetadata->colorspace = "Rec.709";
					break;
				case bmdColorspaceRec2020:
					hdrMetadata->colorspace = "Rec.2020";
					break;
				default:
					hdrMetadata->colorspace = QString("Unknown Colorspace: %1").arg((int32_t)intValue);
					break;
				}
			}

			metadataExtensions->Release();
		}
	}
}

DeckLinkInputFormatChangedEvent::DeckLinkInputFormatChangedEvent(BMDDisplayMode displayMode, Rational const &fps)
	: QEvent(kVideoFormatChangedEvent)
	, display_mode_(displayMode)
	, fps_(fps)
{
}

DeckLinkInputFrameArrivedEvent::DeckLinkInputFrameArrivedEvent(bool signalValid)
	: QEvent(kVideoFrameArrivedEvent)
	, signal_valid_(signalValid)
{
}


