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

#pragma once

#include "AncillaryDataTable.h"
#include "DeckLinkAPI.h"
#include <QEvent>
#include <QString>
#include <stdint.h>

class MainWindow;
class DeckLinkCapture;

class DeckLinkInputDevice : public QObject, public IDeckLinkInputCallback {
	Q_OBJECT
private:
	struct Private;
	Private *m;

	static void getAncillaryDataFromFrame(IDeckLinkVideoInputFrame *frame, BMDTimecodeFormat format, QString *timecodeString, QString *userBitsString);
	static void getHDRMetadataFromFrame(IDeckLinkVideoInputFrame *videoFrame, HDRMetadataStruct *hdrMetadata);

public:
	DeckLinkInputDevice(MainWindow *mw, IDeckLink *deckLink, DeckLinkCapture *capture);
	virtual ~DeckLinkInputDevice();

	bool init();
	const QString &getDeviceName() const;
	bool isCapturing() const;
	bool supportsFormatDetection() const;
	BMDVideoConnection getVideoConnections() const;

	bool startCapture(BMDDisplayMode displayMode, IDeckLinkScreenPreviewCallback *screenPreviewCallback, bool applyDetectedInputMode, bool input_audio);
	void stopCapture(void);

	IDeckLink *getDeckLinkInstance();
	IDeckLinkInput *getDeckLinkInput();
	IDeckLinkConfiguration *getDeckLinkConfiguration();
	IDeckLinkProfileManager *getProfileManager();

	// IUnknown interface
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID iid, void **ppv);
	virtual ULONG STDMETHODCALLTYPE AddRef ();
	virtual ULONG STDMETHODCALLTYPE Release ();

	// IDeckLinkInputCallback interface
	virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags);
	virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame *videoFrame, IDeckLinkAudioInputPacket *audioPacket);
	static double frameRate(IDeckLinkDisplayMode *mode);
signals:
	void audio(QByteArray const &samples);
};

class DeckLinkInputFormatChangedEvent : public QEvent {
private:
	BMDDisplayMode display_mode_;
	double fps_;

public:
	DeckLinkInputFormatChangedEvent(BMDDisplayMode displayMode, double fps);
	virtual ~DeckLinkInputFormatChangedEvent() {}

	BMDDisplayMode DisplayMode() const { return display_mode_; }
	double fps() const { return fps_; }
};

class DeckLinkInputFrameArrivedEvent : public QEvent {
private:
	AncillaryDataStruct *ancillary_data_;
	HDRMetadataStruct *hdr_metadata_;
	bool signal_valid_;

public:
	DeckLinkInputFrameArrivedEvent(AncillaryDataStruct *ancillaryData, HDRMetadataStruct *hdr_metadata_, bool signalValid);
	virtual ~DeckLinkInputFrameArrivedEvent() {}

	AncillaryDataStruct *AncillaryData(void) const { return ancillary_data_; }
	HDRMetadataStruct *HDRMetadata(void) const { return hdr_metadata_; }
	bool SignalValid(void) const { return signal_valid_; }
};

