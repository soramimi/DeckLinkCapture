#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include "Image.h"
#include <QMetaType>

class VideoFrame {
public:
	Image image;
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

Q_DECLARE_METATYPE(VideoFrame)

#endif // VIDEOFRAME_H
