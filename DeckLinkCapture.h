#ifndef DECKLINKCAPTURE_H
#define DECKLINKCAPTURE_H

#include "DeckLinkInputDevice.h"
#include <QImage>
#include <QThread>
#include <QWaitCondition>

class Image;
class DeckLinkInputDevice;

class AbstractDeckLinkCapture {
public:
	virtual void addDevice(IDeckLink *decklink) = 0;
	virtual void removeDevice(IDeckLink *decklink) = 0;
	virtual void updateProfile(IDeckLinkProfile *newProfile) = 0;
	virtual void changeDisplayMode(BMDDisplayMode dispmode, double fps) = 0;
	virtual void setPixelFormat(BMDPixelFormat pixel_format) = 0;
	virtual void setSignalStatus(bool valid) = 0;
	virtual void criticalError(QString const &title, QString const &message) = 0;
};



enum class DeinterlaceMode {
	None,
	InterpolateEven,
	InterpolateOdd,
	Merge,
	MergeX2, // double frames
};

class DeckLinkCapture : public QThread, public AbstractDeckLinkCapture {
	Q_OBJECT
	friend class DeckLinkInputDevice;
private:
	struct Private;
	Private *m;
	MainWindow *mainwindow_;
	struct Task {
		QSize sz;
		QByteArray ba;
	};
	void process(const Task &task);
	void run();
	void pushFrame(const Task &task);
	void clear();
	void newFrame_(const Image &image0, const Image &image1);

	void addDevice(IDeckLink *decklink) override;
	void removeDevice(IDeckLink* decklink) override;
	void updateProfile(IDeckLinkProfile *newProfile) override;
	void changeDisplayMode(BMDDisplayMode dispmode, double fps) override;
	void criticalError(QString const &title, QString const &message) override;
	void setSignalStatus(bool valid) override;
	void setPixelFormat(BMDPixelFormat pixel_format) override;

public:
	DeckLinkCapture();
	~DeckLinkCapture();
	DeinterlaceMode deinterlaceMode() const;
	void setDeinterlaceMode(DeinterlaceMode mode);
	bool startCapture(MainWindow *mainwindow, DeckLinkInputDevice *selectedDevice_, BMDDisplayMode displayMode, BMDFieldDominance fieldDominance, bool applyDetectedInputMode, bool input_audio);
	void stop();
	Image nextFrame();
signals:
	void newFrame();
};

#endif // DECKLINKCAPTURE_H
