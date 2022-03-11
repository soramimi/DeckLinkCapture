#include "MySettings.h"
#include "GlobalData.h"
#include "main.h"

MySettings::MySettings(QObject *)
	: QSettings(global->config_file_path, QSettings::IniFormat)
{
}

