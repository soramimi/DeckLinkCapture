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
};

Q_DECLARE_METATYPE(VideoFrame)

#endif // VIDEOFRAME_H
