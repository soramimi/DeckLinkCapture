#include "../src/Cuda.h"
#include "../src/CudaInterface.h"
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <memory>


int main(int argc, char **argv)
{
	QCoreApplication a(argc, argv);

	QPluginLoader loader("cudaplugin");
	CudaInterface *plugin = dynamic_cast<CudaInterface *>(loader.instance());
	if (plugin) {
		auto p = std::shared_ptr<Cuda>(plugin->create());
		QString s = p->func("Hello,", " world");
		qDebug() << s;
	} else {
		qDebug() << "failed to load the plugin";
	}

	return 0;
}



