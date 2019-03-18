
CONFIG(release,debug|release):TARGET = DeckLinkCapture
CONFIG(debug,debug|release):TARGET = DeckLinkCaptured
TEMPLATE = app
QT += core gui widgets opengl multimedia
CONFIG += c++11

DESTDIR = $$PWD/_bin

win32:INCLUDEPATH += C:\opencv\build\include

linux:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

CONFIG += use_opencv
use_opencv {
	DEFINES += USE_OPENCV
	linux:LIBS += -lopencv_core -lopencv_highgui -lopencv_imgproc
	win32:LIBS += -LC:\opencv\build\x64\vc12\lib -lopencv_core2412 -lopencv_gpu2412 -lopencv_highgui2412 -lopencv_imgproc2412
}

SOURCES += \
	AncillaryDataTable.cpp \
	DeckLinkCapture.cpp \
	DeckLinkDeviceDiscovery.cpp \
	DeckLinkInputDevice.cpp \
	ImageWidget.cpp \
	MainWindow.cpp \
	ProfileCallback.cpp \
	main.cpp \
    StatusLabel.cpp

HEADERS += \
	AncillaryDataTable.h \
	DeckLinkAPI.h \
	DeckLinkCapture.h \
	DeckLinkDeviceDiscovery.h \
	DeckLinkInputDevice.h \
	ImageWidget.h \
	MainWindow.h \
	ProfileCallback.h \
	common.h \
    StatusLabel.h

FORMS += \
    MainWindow.ui

win32:SOURCES += sdk/Win/DeckLinkAPI_i.c
win32:HEADERS += sdk/Win/DeckLinkAPI_h.h
linux:SOURCES += sdk/Linux/include/DeckLinkAPIDispatch.cpp
