
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

win32 {
	INCLUDEPATH += C:/ffmpeg-4.1.3-win64-dev/include
	LIBS += -LC:/ffmpeg-4.1.3-win64-dev/lib
}
macx:INCLUDEPATH += /usr/local/Cellar/ffmpeg/4.1.4_1/include
macx:LIBS += -L/usr/local/Cellar/ffmpeg/4.1.4_1/lib
#LIBS += -lavutil -lavcodec -lavformat -lswscale -lswresample

LIBS += \
	$$PWD/../ffmpeg/libavfilter/libavfilter.a \
	$$PWD/../ffmpeg/libavdevice/libavdevice.a \
	$$PWD/../ffmpeg/libavformat/libavformat.a \
	$$PWD/../ffmpeg/libavcodec/libavcodec.a \
	$$PWD/../ffmpeg/libswscale/libswscale.a \
	$$PWD/../ffmpeg/libavutil/libavutil.a \
	$$PWD/../ffmpeg/libswresample/libswresample.a \
	-lasound -lsndio -lxcb -lxcb-shm -lxcb-xfixes -lxcb-shape -lz -llzma -lSDL2 -lX11 -lXext -lXv -lvdpau

#

SOURCES += \
	ActionHandler.cpp \
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
	MySettings.cpp \
	OverlayWindow.cpp \
	ProfileCallback.cpp \
	RecoringDialog.cpp \
	TestForm.cpp \
	VideoFrame.cpp \
	joinpath.cpp \
	main.cpp \
	VideoEncoder.cpp \
	StatusLabel.cpp

HEADERS += \
	ActionHandler.h \
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
	MySettings.h \
	OverlayWindow.h \
	ProfileCallback.h \
	RecoringDialog.h \
	TestForm.h \
	VideoFrame.h \
	common.h \
	StatusLabel.h \
	VideoEncoder.h \
	joinpath.h \
	main.h

FORMS += \
    MainWindow.ui \
    OverlayWindow.ui \
    RecoringDialog.ui \
    TestForm.ui

RESOURCES += \
	resources.qrc

win32:SOURCES += sdk/Win/DeckLinkAPI_i.c
win32:HEADERS += sdk/Win/DeckLinkAPI_h.h
linux:SOURCES += sdk/Linux/include/DeckLinkAPIDispatch.cpp
macx:SOURCES += sdk/Mac/include/DeckLinkAPIDispatch.cpp
