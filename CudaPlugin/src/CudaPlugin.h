#ifndef CUDAPLUGIN_H
#define CUDAPLUGIN_H

#include <QtCore/qplugin.h>
#include "CudaInterface.h"

class CudaPlugin : public QObject, public CudaInterface {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "mynamespace.CudaPlugin" FILE "cudaplugin.json")
	Q_INTERFACES(CudaInterface)
public:
	Cuda *create() override
	{
		return new Cuda();
	}
};

#endif // CUDAPLUGIN_H
