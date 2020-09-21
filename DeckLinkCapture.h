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

class DeckLinkCapture : public QThread {
	Q_OBJECT
	friend class DeckLinkInputDevice;
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
	void newFrame_(QImage const &image0, QImage const &image1);
public:
	DeckLinkCapture();
	~DeckLinkCapture();
	void setPixelFormat(BMDPixelFormat pixel_format);
	DeinterlaceMode deinterlaceMode() const;
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool startCapture(DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio);
	void stop();
	QImage nextFrame();
signals:
	void newFrame();
};

#endif // DECKLINKCAPTURE_H
