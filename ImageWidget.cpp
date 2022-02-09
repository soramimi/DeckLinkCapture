
#include "ImageWidget.h"
#include <QDebug>
#include <QPainter>
#include <QThread>
#include <QStyle>
#include <QPainterPath>

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

	font_ = QFont("Monospace", 24);
	font_.setStyleHint(QFont::TypeWriter);
}

void ImageWidget::clear()
{
	fps_ = 0;
	frame_counter_ = 0;
	time_ = QDateTime();
	image_ = QImage();
	next_image_ = QImage();
	timer_.stop();
	recording_pregress_current_ = 0;
	recording_pregress_length_ = 0;
}

void ImageWidget::setRecordingProgress(qint64 current, qint64 length)
{
	recording_pregress_current_ = current;
	recording_pregress_length_ = length;
	update();
}

void ImageWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	QSize sz;
	if (view_mode_ == ViewMode::SmallLQ) {
		QSize sz(image_.width() / 4, image_.height() / 4);
		image_for_view_ = image_.scaled(sz, Qt::IgnoreAspectRatio, Qt::FastTransformation);
	} else if (view_mode_ == ViewMode::FitToWindow) {
		QSize sz = fitSize(image_.size(), width(), height());
		image_for_view_ = image_.scaled(sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	} else if (view_mode_ == ViewMode::DotByDot) {
		image_for_view_ = image_;
	}

	pr.save();
	QPainterPath path1;
	path1.addRect(rect());
	const int w = image_for_view_.width();
	const int h = image_for_view_.height();
	if (w > 0 && h > 0) {
		int x = (width() - w) / 2;
		int y = (height() - h) / 2;
		QRect r(x, y, w, h);
		pr.drawImage(r, image_for_view_);
		QPainterPath path2;
		path2.addRect(r);
		path1 = path1.subtracted(path2);
	}
	pr.setClipPath(path1);
	pr.fillRect(0, 0, width(), height(), Qt::black);
	pr.restore();

	if (recording_pregress_current_ > 0) {
		pr.setFont(font_);
		pr.setPen(Qt::white);
		auto fm = pr.fontMetrics();
		int y = fm.height();
		auto TimeString = [](qint64 secs){
			auto h = secs;
			int s = h % 60;
			h /= 60;
			int m = h % 60;
			h /= 60;
			return QString::asprintf("%02d:%02d:%02d", h, m, s);
		};
		QString s = QString("REC ") + TimeString(recording_pregress_current_) + " / " + TimeString(recording_pregress_length_);
		pr.drawText(0, y, s);
	}
}

void ImageWidget::timerEvent(QTimerEvent *event)
{
	(void)event;
	fps_ = frame_counter_;
	frame_counter_ = 0;
//	qDebug() << QString("----- %1fps").arg(fps_);
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



