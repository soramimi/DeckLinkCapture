
CONFIG(release,debug|release):TARGET = DeckLinkCapture
CONFIG(debug,debug|release):TARGET = DeckLinkCaptured
TEMPLATE = app
QT += core gui widgets opengl multimedia
CONFIG += c++11

DESTDIR = $$PWD/_bin

win32:INCLUDEPATH += C:\opencv\build\include

linux:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32

# recording

#CONFIG += use_video_recording
use_video_recording {
	DEFINES += USE_VIDEO_RECORDING
	win32 {
		INCLUDEPATH += C:/ffmpeg-4.1.3-win64-dev/include
		LIBS += -LC:/ffmpeg-4.1.3-win64-dev/lib
	}
	LIBS += -lavutil -lavcodec -lavformat -lswscale -lswresample
}

# OpenCV

#CONFIG += use_opencv
use_opencv {
	DEFINES += USE_OPENCV
	linux:LIBS += -lopencv_core -lopencv_highgui -lopencv_imgproc
	CONFIG(release,debug|release):win32:LIBS += -LC:\opencv\build\x64\vc15\lib -lopencv_world410
	CONFIG(debug,debug|release):win32:LIBS += -LC:\opencv\build\x64\vc15\lib -lopencv_world410d
}

#

SOURCES += \
	AncillaryDataTable.cpp \
	DeckLinkCapture.cpp \
	DeckLinkDeviceDiscovery.cpp \
	DeckLinkInputDevice.cpp \
	ImageWidget.cpp \
	MainWindow.cpp \
	ProfileCallback.cpp \
	main.cpp \
	Deinterlace.cpp \
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
	Deinterlace.h \
	StatusLabel.h

FORMS += \
    MainWindow.ui

use_video_recording {
	SOURCES += VideoEncoder.cpp
	HEADERS += VideoEncoder.h
}

win32:SOURCES += sdk/Win/DeckLinkAPI_i.c
win32:HEADERS += sdk/Win/DeckLinkAPI_h.h
linux:SOURCES += sdk/Linux/include/DeckLinkAPIDispatch.cpp

