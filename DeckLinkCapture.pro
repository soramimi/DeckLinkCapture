
TARGET = DeckLinkCapture
TEMPLATE = app
QT += core gui widgets opengl multimedia
CONFIG += c++17

DESTDIR = $$PWD/_bin

win32:INCLUDEPATH += C:\opencv\build\include

linux:QMAKE_CXXFLAGS += -fopenmp
linux:QMAKE_LFLAGS += -fopenmp

linux:LIBS += -ldl
win32:LIBS += -lole32 -loleaut32
macx:LIBS += -framework CoreFoundation

gcc:QMAKE_CXXFLAGS += -Wno-switch

pre.target = pre
pre.commands = cd $$PWD && make
QMAKE_EXTRA_TARGETS += pre
PRE_TARGETDEPS += pre

# recording

win32 {
	INCLUDEPATH += C:/ffmpeg-4.1.3-win64-dev/include
	LIBS += -LC:/ffmpeg-4.1.3-win64-dev/lib
}
macx:INCLUDEPATH += /usr/local/Cellar/ffmpeg/4.1.4_1/include
macx:LIBS += -L/usr/local/Cellar/ffmpeg/4.1.4_1/lib
!linux:LIBS += -lavutil -lavcodec -lavformat -lswscale -lswresample

#CONFIG += static_ffmpeg
static_ffmpeg {

INCLUDEPATH += $$PWD/../FFmpeg
INCLUDEPATH += $$PWD/../FFmpeg/fftools

LIBS += \
	$$PWD/../FFmpeg/libavdevice/libavdevice.a \
	$$PWD/../FFmpeg/libavcodec/libavcodec.a \
	$$PWD/../FFmpeg/libavformat/libavformat.a \
	$$PWD/../FFmpeg/libavutil/libavutil.a \
	$$PWD/../FFmpeg/libswresample/libswresample.a \
	$$PWD/../FFmpeg/libavfilter/libavfilter.a \
	$$PWD/../FFmpeg/libswscale/libswscale.a

	LIBS += -lpthread
	LIBS += -L/usr/local/lib -lSvtAv1Enc -lSvtAv1Dec -lpthread -lm
	LIBS += -lX11 -lXext -lbz2 -lz -llzma -lopus -lva -lva-drm -lva-x11 -lvdpau
	LIBS += -ldl

	SOURCES += \
	../FFmpeg/libavcodec/adts_parser.c \
	../FFmpeg/libavcodec/avfft.c \
	../FFmpeg/libavformat/mux.c \
	../FFmpeg/libavutil/file.c \
	../FFmpeg/libavutil/pixelutils.c

	HEADERS += \
	../FFmpeg/libavcodec/adts_parser.h \
	../FFmpeg/libavcodec/avfft.h \
	../FFmpeg/libavcodec/codec2utils.h \
	../FFmpeg/libavutil/file.h \
	../FFmpeg/libavutil/pixelutils.h
}
!static_ffmpeg {
	linux:LIBS += -lavutil -lavcodec -lavformat -lswscale -lswresample
}

#

SOURCES += \
	ActionHandler.cpp \
	AncillaryDataTable.cpp \
	CaptureFrame.cpp \
	DeckLinkCapture.cpp \
	DeckLinkDeviceDiscovery.cpp \
	DeckLinkInputDevice.cpp \
	Deinterlace.cpp \
	FrameProcessThread.cpp \
	FrameRateCounter.cpp \
	GlobalData.cpp \
	Image.cpp \
	ImageUtil.cpp \
	ImageWidget.cpp \
	MainWindow.cpp \
	MyDeckLinkAPI.cpp \
	MySettings.cpp \
	ProfileCallback.cpp \
	Rational.cpp \
	RecordingDialog.cpp \
	StatusLabel.cpp \
	TestForm.cpp \
	UIWidget.cpp \
	VideoEncoder.cpp \
	joinpath.cpp \
	main.cpp

HEADERS += \
	ActionHandler.h \
	AncillaryDataTable.h \
	CaptureFrame.h \
	DeckLinkCapture.h \
	DeckLinkDeviceDiscovery.h \
	DeckLinkInputDevice.h \
	Deinterlace.h \
	FrameProcessThread.h \
	FrameRateCounter.h \
	GlobalData.h \
	Image.h \
	ImageUtil.h \
	ImageWidget.h \
	MainWindow.h \
	MyDeckLinkAPI.h \
	MySettings.h \
	ProfileCallback.h \
	Rational.h \
	RecordingDialog.h \
	StatusLabel.h \
	TestForm.h \
	UIWidget.h \
	VideoEncoder.h \
	common.h \
	includeffmpeg.h \
	joinpath.h \
	main.h

FORMS += \
    MainWindow.ui \
    RecordingDialog.ui \
    TestForm.ui \
    UIWidget.ui

RESOURCES += \
	resources.qrc

DISTFILES += \
	CudaPlugin/src/cudalib.cu.cpp

win32:SOURCES += sdk/Win/DeckLinkAPI_i.c
win32:HEADERS += sdk/Win/DeckLinkAPI_h.h
linux:SOURCES += sdk/Linux/include/DeckLinkAPIDispatch.cpp
macx:SOURCES += sdk/Mac/include/DeckLinkAPIDispatch.cpp
