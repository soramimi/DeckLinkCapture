#ifndef MAIN_H
#define MAIN_H

#include <QCursor>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "DeckLinkCapture"

struct GlobalData {
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
	QCursor transparent_cursor;
};

extern GlobalData *global;

#endif // MAIN_H
