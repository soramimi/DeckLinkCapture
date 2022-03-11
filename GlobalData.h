#ifndef GLOBALDATA_H
#define GLOBALDATA_H

#include <QCursor>
#include <QString>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "DeckLinkCapture"

struct GlobalData {
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
	QCursor invisible_cursor;
};

extern GlobalData *global;

#endif // GLOBALDATA_H
