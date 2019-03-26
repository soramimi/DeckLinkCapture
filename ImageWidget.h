
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
private:
	QTimer timer_;
	int fps_ = 0;
	int frame_counter_ = 0;
	QImage image_;
	QImage next_image_;
	QDateTime time_;
protected:
	void paintEvent(QPaintEvent *);
protected:
	void timerEvent(QTimerEvent *event);
public:
	ImageWidget(QWidget *parent = 0);
	void clear();
public slots:
	void setImage(const QImage &image0, const QImage &image1);
};

#endif // IMAGEWIDGET_H
