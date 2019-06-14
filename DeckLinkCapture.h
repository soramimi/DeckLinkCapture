#ifndef DECKLINKCAPTURE_H
#define DECKLINKCAPTURE_H

#include "DeckLinkInputDevice.h"
#include <QImage>
#include <QThread>
#include <QWaitCondition>

class DeckLinkInputDevice;

enum class DeinterlaceMode {
	None,
	InterpolateEven,
	InterpolateOdd,
	Merge,
	MergeX2, // double frames
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
	void clear();
public:
	DeckLinkCapture();
	~DeckLinkCapture();
	void setPixelFormat(BMDPixelFormat pixel_format);
	DeinterlaceMode deinterlaceMode() const;
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool start(DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio);
	void stop();
signals:
	void newFrame(QImage const &image0, QImage const &image1);
};

#endif // DECKLINKCAPTURE_H
