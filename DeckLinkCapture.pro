
CONFIG(release,debug|release):TARGET = DeckLinkCapture
CONFIG(debug,debug|release):TARGET = DeckLinkCaptured
TEMPLATE = app
QT += core gui widgets opengl multimedia
CONFIG += c++1z

DESTDIR = $$PWD/_bin

win32:INCLUDEPATH += C:\opencv\build\include

linux:QMAKE_CXXFLAGS += -fopenmp
linux:QMAKE_LFLAGS += -fopenmp

linux:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32
macx:LIBS += -framework CoreFoundation

gcc:QMAKE_CXXFLAGS += -Wno-switch

# recording

CONFIG += use_video_recording
use_video_recording {
	DEFINES += USE_VIDEO_RECORDING
	win32 {
		INCLUDEPATH += C:/ffmpeg-4.1.3-win64-dev/include
		LIBS += -LC:/ffmpeg-4.1.3-win64-dev/lib
	}
    macx:INCLUDEPATH += /usr/local/Cellar/ffmpeg/4.1.4_1/include
    macx:LIBS += -L/usr/local/Cellar/ffmpeg/4.1.4_1/lib
    LIBS += -lavutil -lavcodec -lavformat -lswscale -lswresample
}

# OpenCV

CONFIG += use_opencv
use_opencv {
	DEFINES += USE_OPENCV
	linux:INCLUDEPATH += /usr/include/opencv4
    linux:LIBS += -lopencv_core -lopencv_highgui -lopencv_imgproc
    macx:INCLUDEPATH += /usr/local/Cellar/opencv/4.1.0_2/include/opencv4
    macx:LIBS += -L/usr/local/Cellar/opencv/4.1.0_2/lib -lopencv_core -lopencv_highgui -lopencv_imgproc
    CONFIG(release,debug|release):win32:LIBS += -LC:\opencv\build\x64\vc15\lib -lopencv_world410
	CONFIG(debug,debug|release):win32:LIBS += -LC:\opencv\build\x64\vc15\lib -lopencv_world410d
}

#

SOURCES += \
	AncillaryDataTable.cpp \
	DeckLinkCapture.cpp \
	DeckLinkDeviceDiscovery.cpp \
	DeckLinkInputDevice.cpp \
	FrameRateCounter.cpp \
	Image.cpp \
	ImageConvertThread.cpp \
	ImageUtil.cpp \
	ImageWidget.cpp \
	MainWindow.cpp \
	MyDeckLinkAPI.cpp \
	OverlayWindow.cpp \
	ProfileCallback.cpp \
	RecoringDialog.cpp \
	VideoFrame.cpp \
	main.cpp \
	VideoEncoder.cpp \
	StatusLabel.cpp

HEADERS += \
	AncillaryDataTable.h \
	DeckLinkCapture.h \
	DeckLinkDeviceDiscovery.h \
	DeckLinkInputDevice.h \
	FrameRateCounter.h \
	Image.h \
	ImageConvertThread.h \
	ImageUtil.h \
	ImageWidget.h \
	MainWindow.h \
	MyDeckLinkAPI.h \
	OverlayWindow.h \
	ProfileCallback.h \
	RecoringDialog.h \
	VideoFrame.h \
	common.h \
	StatusLabel.h \
	VideoEncoder.h \
	main.h

FORMS += \
    MainWindow.ui \
    OverlayWindow.ui \
    RecoringDialog.ui

win32:SOURCES += sdk/Win/DeckLinkAPI_i.c
win32:HEADERS += sdk/Win/DeckLinkAPI_h.h
linux:SOURCES += sdk/Linux/include/DeckLinkAPIDispatch.cpp
macx:SOURCES += sdk/Mac/include/DeckLinkAPIDispatch.cpp

RESOURCES += \
	resources.qrc

