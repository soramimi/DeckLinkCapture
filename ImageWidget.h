
#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QWidget>

class Image;
class VideoFrameData;

class ImageWidget : public QWidget {
	Q_OBJECT
public:
	enum class ViewMode {
		SmallLQ,
		DotByDot,
		FitToWindow,
	};
private:
	struct Private;
	Private *m;
protected:
	void paintEvent(QPaintEvent *) override;
public:
	ImageWidget(QWidget *parent = nullptr);
	~ImageWidget() override;
	void clear();
	void setViewMode(ViewMode vm);
	ViewMode viewMode() const;
	void updateRecordingProgress(qint64 current, qint64 length);
	QSize scaledSize(const Image &image);
	void setImage(const QImage &image);
	void setCriticalError(const QString &title, const QString &message);
};

#endif // IMAGEWIDGET_H
