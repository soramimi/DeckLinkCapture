#ifndef CAPTUREFRAME_H
#define CAPTUREFRAME_H

#include "Image.h"
#include <QImage>
#include <QMetaType>

class CaptureFrame {
public:
	enum State {
		Idle,
		Busy,
		Ready,
	};
	State state = Idle;
	Image image;
	QByteArray audio;
	QImage image_for_view;
	operator bool () const
	{
		return (bool)image;
	}
	int width() const
	{
		return image ? image.width() : 0;
	}
	int height() const
	{
		return image ? image.height() : 0;
	}
};

Q_DECLARE_METATYPE(CaptureFrame)

#endif // CAPTUREFRAME_H
