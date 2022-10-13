#ifndef GLOBALDATA_H
#define GLOBALDATA_H

#include <QCursor>
#include <QString>
#include <memory>

#define ORGANIZATION_NAME "soramimi.jp"
#define APPLICATION_NAME "DeckLinkCapture"

class Cuda;

struct GlobalData {
	QString organization_name;
	QString application_name;
	QString generic_config_dir;
	QString app_config_dir;
	QString config_file_path;
	QCursor invisible_cursor;

	std::shared_ptr<Cuda> cuda_plugin;
};

extern GlobalData *global;

#endif // GLOBALDATA_H
