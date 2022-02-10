
#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QGLWidget>

class Image;

class ImageWidget : public QGLWidget {
	Q_OBJECT
public:
	enum class ViewMode {
		SmallLQ,
		FitToWindow,
		DotByDot,
	};
private:
	struct Private;
	Private *m;
	QSize scaledSize(const Image &image);
protected:
	void paintEvent(QPaintEvent *);
public:
	ImageWidget(QWidget *parent = 0);
	~ImageWidget();
	void clear();
	void setViewMode(ViewMode vm);
	ViewMode viewMode() const;
	void updateRecordingProgress(qint64 current, qint64 length);
public slots:
	void setImage(const Image &image);
private slots:
	void ready(QImage image);
};

#endif // IMAGEWIDGET_H
