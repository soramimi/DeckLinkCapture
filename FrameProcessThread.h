#ifndef FRAMEPROCESSTHREAD_H
#define FRAMEPROCESSTHREAD_H

#include "Image.h"
#include "VideoFrameData.h"
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class VideoFrameData;

class FrameProcessThread : public QObject {
	Q_OBJECT
private:
	struct Private;
	Private *m;
	void run();
public:
	FrameProcessThread();
	~FrameProcessThread() override;
	void start();
	void stop();
	void request(const VideoFrameData &image, QSize const &size);
	void enableDeinterlace(bool enable);
signals:
	void ready(VideoFrameData const &image);
};

#endif // FRAMEPROCESSTHREAD_H
