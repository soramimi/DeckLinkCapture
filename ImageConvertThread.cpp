
#include "ImageConvertThread.h"
#include "ImageUtil.h"

void ImageConvertThread::run()
{
	while (1) {
		Image image;
		{
			QMutexLocker lock(&mutex_);
			if (isInterruptionRequested()) break;
			if (requested_image_) {
				image = requested_image_;
				requested_image_.clear();
			} else {
				waiter_.wait(&mutex_);
			}
		}
		if (isInterruptionRequested()) break;
		if (image) {
			QImage qimg = ImageUtil::qimage(image).scaled(scaled_size_, Qt::IgnoreAspectRatio, Qt::FastTransformation);
			emit ready(qimg);
		}
	}
}

void ImageConvertThread::stop()
{
	requestInterruption();
	waiter_.wakeAll();
	if (!wait(1000)) {
		terminate();
	}
}

void ImageConvertThread::request(const Image &image, const QSize &size)
{
	if (image && size.width() > 0 && size.height() > 0) {
		QMutexLocker lock(&mutex_);
		requested_image_ = image;
		scaled_size_ = size;
		waiter_.wakeAll();
	}
}
