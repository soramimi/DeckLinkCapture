#ifndef FRAMEPROCESSTHREAD_H
#define FRAMEPROCESSTHREAD_H

#include "Image.h"
#include "CaptureFrame.h"
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class CaptureFrame;

class FrameProcessThread : public QThread {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void run() override;
public:
	FrameProcessThread();
	~FrameProcessThread() override;
	void stop();
	void request(const CaptureFrame &image, QSize const &size);
signals:
	void ready(CaptureFrame const &image);
};

#endif // FRAMEPROCESSTHREAD_H
