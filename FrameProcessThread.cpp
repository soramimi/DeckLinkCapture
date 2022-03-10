
#include "FrameProcessThread.h"
#include "ImageUtil.h"

struct FrameProcessThread::Private {
	QMutex mutex;
	QWaitCondition waiter;
	CaptureFrame requested_frame;
	QSize scaled_size;
};

FrameProcessThread::FrameProcessThread()
	: m(new Private)
{
}

FrameProcessThread::~FrameProcessThread()
{
	stop();
	delete m;
}

void FrameProcessThread::run()
{
	while (1) {
		CaptureFrame frame;
		{
			QMutexLocker lock(&m->mutex);
			if (isInterruptionRequested()) break;
			if (m->requested_frame) {
				frame = m->requested_frame;
				m->requested_frame = {};
			} else {
				m->waiter.wait(&m->mutex);
			}
		}
		if (frame) {
			frame.image_for_view = ImageUtil::qimage(frame.image).scaled(m->scaled_size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
			emit ready(frame);
		}
	}
}

void FrameProcessThread::stop()
{
	{
		QMutexLocker lock(&m->mutex);
		requestInterruption();
		m->requested_frame = {};
	}
	m->waiter.wakeAll();
	wait();
}

void FrameProcessThread::request(CaptureFrame const &image, const QSize &size)
{
	if (image && size.width() > 0 && size.height() > 0) {
		QMutexLocker lock(&m->mutex);
		m->requested_frame = image;
		m->scaled_size = size;
		m->waiter.wakeAll();
	}
}
