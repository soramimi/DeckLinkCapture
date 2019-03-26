
#include "ImageWidget.h"
#include <QDebug>
#include <QPainter>
#include <QThread>

QSize fitSize(QSize const &size, int dw, int dh)
{
	int w = size.width();
	int h = size.height();
	if (w < 1 || h < 1) {
		return QSize();
	}
	if (dh * w > dw * h) {
		h = h * dw / w;
		w = dw;
	} else {
		w = w * dh / h;
		h = dh;
	}
	return QSize(w, h);
}

ImageWidget::ImageWidget(QWidget *parent)
	: QGLWidget(parent)
{
	startTimer(1000);
	connect(&timer_, &QTimer::timeout, [&](){
		image_ = next_image_;
		frame_counter_++;
		update();
	});
}

void ImageWidget::clear()
{
	fps_ = 0;
	frame_counter_ = 0;
	time_ = QDateTime();
	image_ = QImage();
	next_image_ = QImage();
	timer_.stop();
}

void ImageWidget::paintEvent(QPaintEvent *)
{
	int dw = std::min(1920, width());
	int dh = std::min(1080, height());
	QSize sz = fitSize(image_.size(), dw, dh);
	int x = (width() - sz.width()) / 2;
	int y = (height() - sz.height()) / 2;
	QPainter pr(this);
	pr.drawImage(QRect(x, y, sz.width(), sz.height()), image_, image_.rect());
}

void ImageWidget::timerEvent(QTimerEvent *event)
{
	(void)event;
	fps_ = frame_counter_;
	frame_counter_ = 0;
	qDebug() << QString("----- %1fps").arg(fps_);
}

void ImageWidget::setImage(const QImage &image0, const QImage &image1)
{
	QDateTime now = QDateTime::currentDateTime();

	image_ = image0;
	next_image_ = image1;

	if (!image_.isNull()) {
		frame_counter_++;
		update();
	}
	if (!next_image_.isNull() && time_.isValid()) {
		next_image_ = image1;
		qint64 ms = time_.msecsTo(now);
		timer_.setSingleShot(true);
		timer_.start(ms / 2);
	}

	time_ = now;
}



