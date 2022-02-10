#include "MySettings.h"
#include "main.h"

MySettings::MySettings(QObject *)
	: QSettings(global->config_file_path, QSettings::IniFormat)
{
}

