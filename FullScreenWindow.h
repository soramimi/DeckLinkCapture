#ifndef FULLSCREENWINDOW_H
#define FULLSCREENWINDOW_H

#include <QMainWindow>

class Image;

namespace Ui {
class FullScreenWindow;
}

class FullScreenWindow : public QMainWindow {
	Q_OBJECT
private:
	Ui::FullScreenWindow *ui;
protected:
	void closeEvent(QCloseEvent *event);
public:
	explicit FullScreenWindow(QWidget *parent = nullptr);
	~FullScreenWindow();
	void setImage(QImage const &image);
	QSize scaledSize(Image const &image);
};

#endif // FULLSCREENWINDOW_H
