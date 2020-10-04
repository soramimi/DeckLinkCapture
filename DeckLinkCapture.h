#ifndef DECKLINKCAPTURE_H
#define DECKLINKCAPTURE_H

#include "DeckLinkInputDevice.h"
#include "Image.h"
#include <QImage>
#include <QThread>
#include <QWaitCondition>

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
	virtual ~DeviceEvent()
	{

	}
};

class DeckLinkCaptureDelegate {
public:
	virtual void addDevice(IDeckLink *decklink) = 0;
	virtual void removeDevice(IDeckLink *decklink) = 0;
	virtual void updateProfile(IDeckLinkProfile *newProfile) = 0;
	virtual void changeDisplayMode(BMDDisplayMode dispmode, double fps) = 0;
	virtual void videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid) = 0;
	virtual void haltStreams() = 0;
	virtual void criticalError(QString const &title, QString const &message) = 0;
};

enum class DeinterlaceMode {
	None,
	InterpolateEven,
	InterpolateOdd,
	Merge,
	MergeX2, // double frames
};

class DeckLinkCapture : public QThread {
	Q_OBJECT
	friend class DeckLinkInputDevice;
	friend class ProfileCallback;
private:
	struct Private;
	Private *m;

	static Image createImage(int w, int h, BMDPixelFormat pixel_format, uint8_t const *data, int size);

	DeckLinkCaptureDelegate *delegate();
	struct Task {
		Image image;
		bool isValid() const
		{
			return !image.isNull();
		}
	};
	void process(const Task &task);
	void run();
	void pushFrame(const Task &task);
	void clear();
	void newFrame_(const Image &image0, const Image &image1);

	void addDevice(IDeckLink *decklink);
	void removeDevice(IDeckLink* decklink);
	void updateProfile(IDeckLinkProfile *newProfile);
	void changeDisplayMode(BMDDisplayMode dispmode, double fps);
	void videoFrameArrived(AncillaryDataStruct const *ancillary_data, HDRMetadataStruct const *hdr_metadata, bool signal_valid);
	void haltStreams();
	void criticalError(QString const &title, QString const &message);

	BMDPixelFormat pixelFormat() const;
	void setPixelFormat(BMDPixelFormat pixel_format);
protected:
	void customEvent(QEvent *event);

public:
	DeckLinkCapture(DeckLinkCaptureDelegate *delegate);
	~DeckLinkCapture();
	DeinterlaceMode deinterlaceMode() const;
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool startCapture(DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio);
	void stop();
	Image nextFrame();
signals:
	void newFrame();
};

#endif // DECKLINKCAPTURE_H
