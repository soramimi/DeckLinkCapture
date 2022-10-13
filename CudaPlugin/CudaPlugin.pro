
QT = core

TARGET = cudaplugin
#CONFIG(release,debug|release):TARGET = cudaplugin
#CONFIG(debug,debug|release):TARGET = cudaplugind
TEMPLATE = lib
CONFIG += plugin

DESTDIR = $$PWD/_bin

unix:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

LIBS += -L/opt/cuda/targets/x86_64-linux/lib -lcudart

pre.target = pre
pre.commands = cd $$PWD && make build
QMAKE_EXTRA_TARGETS += pre
PRE_TARGETDEPS += pre

HEADERS += \
	src/Cuda.h \
	src/CudaInterface.h \
	src/CudaPlugin.h
SOURCES += \
	src/Cuda.cpp \
	src/CudaInterface.cpp \
	src/CudaPlugin.cpp

LIBS += \
	$$PWD/_build1/cudalib.cu.o

DISTFILES += \
	src/cudalib.cu.cpp \
	cudaplugin.json
