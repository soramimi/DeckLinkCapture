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
#include <QTextStream>

class DebugMessageHandler {
public:
	DebugMessageHandler() = delete;

	static void install();

	static void abort(const QMessageLogContext &context, const QString &message);
};

void DebugMessageHandler::abort(QMessageLogContext const &/*context*/, const QString &/*message*/)
{
	::abort();
}

void debugMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
	(QTextStream(stderr) << qFormatLogMessage(type, context, message) << '\n').flush();

	if (type == QtFatalMsg) {
		DebugMessageHandler::abort(context, message);
	}
}

void DebugMessageHandler::install()
{
	qInstallMessageHandler(debugMessageHandler);
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
	CoInitialize(nullptr);
#endif

	DebugMessageHandler::install();

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
