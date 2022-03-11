#ifndef FRAMEPROCESSTHREAD_H
#define FRAMEPROCESSTHREAD_H

#include "Image.h"
#include "CaptureFrame.h"
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class CaptureFrame;

class FrameProcessThread : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void run();
public:
	FrameProcessThread();
	~FrameProcessThread();
	void start();
	void stop();
	void request(const CaptureFrame &image, QSize const &size);
signals:
	void ready(CaptureFrame const &image);
};

#endif // FRAMEPROCESSTHREAD_H
