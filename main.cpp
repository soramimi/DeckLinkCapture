#include "MainWindow.h"
#include "CaptureFrame.h"
#include "GlobalData.h"
#include "joinpath.h"
#include "main.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QStandardPaths>

static bool isHighDpiScalingEnabled()
{
//	MySettings s;
//	s.beginGroup("UI");
//	QVariant v = s.value("EnableHighDpiScaling");
//	return v.isNull() || v.toBool();
	return true;
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

	if (isHighDpiScalingEnabled()) {
#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
		qDebug() << "High DPI scaling is not supported";
#else
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
	} else {
		QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	}

	QApplication a(argc, argv);

	qRegisterMetaType<Rational>();
	qRegisterMetaType<CaptureFrame>();

	{
		QPixmap pm(1, 1);
		pm.fill(Qt::transparent);
		global->invisible_cursor = QCursor(pm);
	}

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
