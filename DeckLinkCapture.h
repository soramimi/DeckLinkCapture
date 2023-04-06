#ifndef DECKLINKCAPTURE_H
#define DECKLINKCAPTURE_H

#include "DeckLinkInputDevice.h"
#include "Image.h"
#include "VideoFrameData.h"
#include "Rational.h"

class Image;
class DeckLinkInputDevice;

class DeviceEvent : public QEvent {
public:
	enum Operation {
		Add,
		Remove,
	} op;
	IDeckLink *decklink;
	DeviceEvent(Operation op, IDeckLink *decklink)
		: QEvent(QEvent::User)
		, op(op)
		, decklink(decklink)
	{

	}
	~DeviceEvent() override = default;
};

class DeckLinkCaptureDelegate {
public:
	virtual void addDevice(IDeckLink *decklink) = 0;
	virtual void removeDevice(IDeckLink *decklink) = 0;
	virtual void updateProfile(IDeckLinkProfile *newProfile) = 0;
	virtual void changeDisplayMode(BMDDisplayMode dispmode, Rational const &fps) = 0;
//	virtual void videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid) = 0;
	virtual void haltStreams() = 0;
	virtual void criticalError(QString const &title, QString const &message) = 0;
};

class DeckLinkCapture : public QObject {
	Q_OBJECT
	friend class DeckLinkInputDevice;
	friend class ProfileCallback;
private:
	struct Private;
	Private *m;

	static Image createImage(int w, int h, BMDPixelFormat pixel_format, uint8_t const *data, int size);

	DeckLinkCaptureDelegate *delegate();

	void addDevice(IDeckLink *decklink);
	void removeDevice(IDeckLink* decklink);
	void updateProfile(IDeckLinkProfile *newProfile);
	void changeDisplayMode(BMDDisplayMode dispmode, Rational const &fps);
//	void videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid);
	void haltStreams();
	void criticalError(QString const &title, QString const &message);
	void clearCriticalError();

//	BMDPixelFormat pixelFormat() const;
	void setPixelFormat(BMDPixelFormat pixel_format);
protected:
	void customEvent(QEvent *event) override;

public:
	DeckLinkCapture(DeckLinkCaptureDelegate *delegate);
	~DeckLinkCapture() override;
	bool startCapture(DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio);
signals:
	void newFrame(VideoFrameData const &frame);
};

#endif // DECKLINKCAPTURE_H
