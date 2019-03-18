#include "MainWindow.h"
#include <QApplication>
#include <QMetaType>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	CoInitialize(nullptr);
#endif

	QApplication a(argc, argv);

	MainWindow w;
	w.show();
	w.setup();

	int r = a.exec();

#ifdef Q_OS_WIN
	CoUninitialize();
#endif

	return r;
}
