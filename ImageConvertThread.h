#ifndef IMAGECONVERTTHREAD_H
#define IMAGECONVERTTHREAD_H

#include "Image.h"
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class ImageConvertThread : public QThread {
	Q_OBJECT
private:
	QMutex mutex_;
	QWaitCondition waiter_;
	Image requested_image_;
	QSize scaled_size_;
protected:
	void run() override;
public:
	void stop();
	void request(Image const &image, QSize const &size);
signals:
	void ready(QImage image);
};

#endif // IMAGECONVERTTHREAD_H
