#include "MainWindow.h"
#include "main.h"
#include <QApplication>
#include <QMetaType>

GlobalData *global;

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	CoInitialize(nullptr);
#endif

	global = new GlobalData;

	QApplication a(argc, argv);

	global->transparent_cursor = QCursor(QPixmap(":/transparent32x32.png"));

	MainWindow w;
	w.show();
	w.setup();

	int r = a.exec();

#ifdef Q_OS_WIN
	CoUninitialize();
#endif

	delete global;

	return r;
}
