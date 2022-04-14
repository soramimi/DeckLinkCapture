
#include "ImageWidget.h"
#include <QDebug>
#include <QPainter>
#include <QThread>
#include <QStyle>
#include <QPainterPath>
#include <QWaitCondition>
#include "Image.h"
#include "FrameProcessThread.h"
#include "ImageUtil.h"
#include "CaptureFrame.h"

struct ImageWidget::Private {
	QImage scaled_image;
	ViewMode view_mode = ViewMode::SmallLQ;
	qint64 recording_pregress_current = 0;
	qint64 recording_pregress_length = 0;
	QFont large_font;
	QFont error_font;
	QString critical_error_title;
	QString critical_error_message;
};

ImageWidget::ImageWidget(QWidget *parent)
	: QWidget(parent)
	, m(new Private)
{
	startTimer(1000);
	m->large_font = QFont("Monospace", 24);
	m->large_font.setStyleHint(QFont::TypeWriter);
	m->error_font = QFont("Sans", 12);
}

ImageWidget::~ImageWidget()
{
	delete m;
}

void ImageWidget::clear()
{
	m->recording_pregress_current = 0;
	m->recording_pregress_length = 0;
}

void ImageWidget::setViewMode(ImageWidget::ViewMode vm)
{
	m->view_mode = vm;
	update();
}

ImageWidget::ViewMode ImageWidget::viewMode() const
{
	return m->view_mode;
}

void ImageWidget::updateRecordingProgress(qint64 current, qint64 length)
{
	m->recording_pregress_current = current;
	m->recording_pregress_length = length;
	update();
}

QSize ImageWidget::scaledSize(Image const &image)
{
	auto FitSize = [](QSize const &size, int dw, int dh){
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
	};

	if (m->view_mode == ViewMode::SmallLQ) {
		return {960, 540};
	} else if (m->view_mode == ViewMode::FitToWindow) {
		return FitSize({image.width(), image.height()}, width(), height());
	} else if (m->view_mode == ViewMode::DotByDot) {
		return {image.width(), image.height()};
	}
	return {};
}

void ImageWidget::paintEvent(QPaintEvent *)
{
	QPainter pr(this);

	if (!m->scaled_image.isNull()) {
		pr.save();
		QPainterPath path1;
		path1.addRect(rect());
		const int w = m->scaled_image.width();
		const int h = m->scaled_image.height();
		if (w > 0 && h > 0) {
			int x = (width() - w) / 2;
			int y = (height() - h) / 2;
			QRect r(x, y, w, h);
			pr.drawImage(r, m->scaled_image);
			QPainterPath path2;
			path2.addRect(r);
			path1 = path1.subtracted(path2);
		}
		pr.setClipPath(path1);
		pr.fillRect(0, 0, width(), height(), Qt::black);
		pr.restore();
	}

	int y = 0;
	auto DrawText = [](QPainter *p, int y, QString const &s){
		auto fm = p->fontMetrics();
		auto sz = fm.size(Qt::TextSingleLine, s);
		p->fillRect(0, y, sz.width(), sz.height(), QColor(64, 64, 64, 192));
		p->drawText(0, y + fm.ascent(), s);
		return sz.height();
	};

	if (!m->critical_error_message.isEmpty()) {
		pr.setFont(m->error_font);
		pr.setPen(QColor(255, 128, 0));
		y += DrawText(&pr, y, m->critical_error_title);
		y += DrawText(&pr, y, m->critical_error_message);
	}

	if (m->recording_pregress_current > 0 || m->recording_pregress_length > 0) {
		pr.setFont(m->large_font);
		pr.setPen(Qt::white);
		auto TimeString = [](qint64 secs){
			auto h = secs;
			int s = h % 60;
			h /= 60;
			int m = h % 60;
			h /= 60;
			return QString::asprintf("%02d:%02d:%02d", h, m, s);
		};
		QString s = QString("REC ") + TimeString(m->recording_pregress_current) + " / " + TimeString(m->recording_pregress_length);
		y += DrawText(&pr, y, s);
	}
}

void ImageWidget::setImage(QImage const &image)
{
	m->scaled_image = image;
	update();
}

void ImageWidget::setCriticalError(const QString &title, const QString &message)
{
	m->critical_error_title = title;
	m->critical_error_message = message;
	update();
}

