#ifndef CUDAINTERFACE_H
#define CUDAINTERFACE_H

#include <QtPlugin>
#include "Cuda.h"

class CudaInterface {
public:
	CudaInterface();
	virtual ~CudaInterface() {}
	virtual Cuda *create() = 0;
};

Q_DECLARE_INTERFACE(CudaInterface, "mynamespace.CudaPlugin/1.0")

#endif // CUDAINTERFACE_H
