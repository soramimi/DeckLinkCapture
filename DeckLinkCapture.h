#ifndef DECKLINKCAPTURE_H
#define DECKLINKCAPTURE_H

#include "DeckLinkInputDevice.h"
#include <QImage>
#include <QThread>
#include <QWaitCondition>

class DeckLinkInputDevice;

struct Images {
	QImage last_image;
	QImage curr_image;
	QImage deinterlaced_image;
};

enum class DeinterlaceMode {
	None,
	InterpolateEven,
	InterpolateOdd,
	Merge,
};

class DeckLinkCapture : public QThread, public IDeckLinkScreenPreviewCallback {
	Q_OBJECT
protected:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppv)
	{
		(void)iid;
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return 1;
	}
	virtual ULONG STDMETHODCALLTYPE Release()
	{
		return 1;
	}
	virtual HRESULT STDMETHODCALLTYPE DrawFrame(IDeckLinkVideoFrame *frame);
private:
	struct Private;
	Private *m;
	struct Task {
		QSize sz;
		QByteArray ba;
	};
	void process(const Task &task);
	void run();
	void pushFrame(const Task &task);
public:
	DeckLinkCapture();
	~DeckLinkCapture();
	void setPixelFormat(BMDPixelFormat pixel_format);
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool start(DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, bool applyDetectedInputMode, bool input_audio);
	void stop();
signals:
	void newFrame(QImage const &image);
};

#endif // DECKLINKCAPTURE_H
