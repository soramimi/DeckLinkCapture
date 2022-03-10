#include "MainWindow.h"
#include "CaptureFrame.h"
#include "main.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QStandardPaths>
#include "joinpath.h"

GlobalData *global;

static bool isHighDpiScalingEnabled()
{
//	MySettings s;
//	s.beginGroup("UI");
//	QVariant v = s.value("EnableHighDpiScaling");
//	return v.isNull() || v.toBool();
	return false;
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	CoInitialize(nullptr);
#endif

	global = new GlobalData;

	global->organization_name = ORGANIZATION_NAME;
	global->application_name = APPLICATION_NAME;
	global->generic_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
	global->app_config_dir = global->generic_config_dir / global->organization_name / global->application_name;
	global->config_file_path = joinpath(global->app_config_dir, global->application_name + ".ini");
	if (!QFileInfo(global->app_config_dir).isDir()) {
		QDir().mkpath(global->app_config_dir);
	}

	QApplication a(argc, argv);

	if (isHighDpiScalingEnabled()){
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
		qDebug() << "High DPI scaling is not supported";
#else
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
		a.setAttribute(Qt::AA_UseHighDpiPixmaps);
	} else {
		QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	}


	qRegisterMetaType<CaptureFrame>();

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
