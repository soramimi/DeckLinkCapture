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
public:
	explicit FullScreenWindow(QWidget *parent = nullptr);
	~FullScreenWindow();
	void setImage(QImage const &image);
	QSize scaledSize(Image const &image);

	// QWidget interface
protected:
	void closeEvent(QCloseEvent *event);
};

#endif // FULLSCREENWINDOW_H
