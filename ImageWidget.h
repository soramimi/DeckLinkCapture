
#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QMutex>
#include <QOpenGLWidget>
#include <DeckLinkAPI.h>
#include <QGLWidget>
#include <deque>
#include <QTimer>
#include <QDateTime>

class ImageWidget : public QGLWidget {
	Q_OBJECT
public:
	enum class ViewMode {
		SmallLQ,
		FitToWindow,
		DotByDot,
	};
private:
	QTimer timer_;
	int fps_ = 0;
	int frame_counter_ = 0;
	QImage image_;
	QImage next_image_;
	QImage image_for_view_;
	QDateTime time_;
	ViewMode view_mode_ = ViewMode::SmallLQ;
	qint64 recording_pregress_current_ = 0;
	qint64 recording_pregress_length_ = 0;
	QFont font_;
protected:
	void paintEvent(QPaintEvent *);
	void timerEvent(QTimerEvent *event);
protected:
public:
	ImageWidget(QWidget *parent = 0);
	void clear();
	void setViewMode(ViewMode vm)
	{
		view_mode_ = vm;
		update();
	}
	ViewMode viewMode() const
	{
		return view_mode_;
	}

	void setRecordingProgress(qint64 current, qint64 length);

public slots:
	void setImage(const QImage &image0, const QImage &image1);
};

#endif // IMAGEWIDGET_H
